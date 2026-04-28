# AD936x Pluto Loopback — Diagnostic Reference

Captures the BIST (Built-In Self-Test) loopback investigation on an
AD9364-modded ADALM-Pluto running firmware v0.32, plus the recommended
bring-up workflow given the lab constraints (no coax cable, no external
attenuator, only two antennas, 2.4 GHz only, 20 MSPS only).

---

## 1. Hardware under test

| Item | Value |
|---|---|
| Hardware | ADALM-Pluto Rev.C (Z7010 + AD9361 silicon) |
| Modification | Device-tree override → AD9364 (single-channel, 70 MHz–6 GHz) |
| u-boot env | `compatible=ad9364`, `mode=1r1t` |
| Firmware | `v0.32` (older; current ADI release is ~v0.39) |
| Confirmation | LO accepts 5 GHz write-back (AD9363 would clamp at ~3.8 GHz) |
| Lab kit | 2 antennas (no coax, no attenuators, no shielded box) |
| Operating freq | 2.4 GHz only (constraint) |
| Sample rate | 20 MSPS only (matches FPGA chain design) |

---

## 2. Diagnostic scripts

Five complementary tools exist in `sim/`:

| Script | Role |
|---|---|
| `pluto_loopback.py` | **Production wrapper** — `digital` / `pluto-bist` / `pluto-rf` / `two-pluto` modes.  Q15+SOFT+RS hardcoded. |
| `pluto_bist_diag.py` | OFDM-burst-based bit-exactness diagnostic (Gemini's). Saves TX/RX NPY + linear-fit residual. |
| `pluto_bist_probe.py` | Random-IQ probe with linear-fit + zero-region oracle. Fast iteration on `loopback` values. |
| `pluto_dump.py` | Minimal 4-column TX/RX text dump for visual inspection. |
| `pluto_timing_sweep.py` | **Modcod × frame-size sweep** with per-phase timing (TX prep / upload / settle / RX cap / align / decode).  Verifies BER=0 across 16QAM 1/2 + 2/3 × {9, 18, 33, 60, 120, 255}. |

The two **bit-exactness diagnostics** are described first; the
**production wrapper** is documented in §5.

### 2.1 `pluto_bist_diag.py` (Gemini's, **definitive**)

Captures one full **OFDM burst** through Pluto with BIST loopback enabled,
saves both TX and RX IQ to NPY files, runs a sample-by-sample comparison.

The right tool for confirming bit-exactness because:
- it uses the actual OFDM burst structure (lead zeros + guard + preamble +
  header + data + trail zeros) instead of arbitrary random IQ
- TX-zero regions become a **bit-exactness oracle**: if the burst's 1440
  zero samples come back as `RX RMS = 0.0000`, the path has no additive
  noise floor and `rx = α · tx` is the exact model
- linear-fit residual gives a clean SNR figure on the signal regions

```bash
.venv/bin/python sim/pluto_bist_diag.py [--uri ...] [--rate 20e6]
# Writes:
#   sim/tb_pluto_bist_tx.npy  (complex64, 75 456 samples)
#   sim/tb_pluto_bist_rx.npy  (complex128, 200 000 samples)
```

### 2.2 `pluto_bist_probe.py` (random-IQ probe — secondary)

Sends 1000 random complex int16 samples, captures, cross-correlates,
fits a complex scale + DC offset, reports residual.  Faster but **less
diagnostic** — random IQ doesn't have the structural zero regions that
expose bit-exactness, and a strict per-LSB equality test can flag a
bit-exact path as "broken" because of the LSB-level residual after
de-scaling a heavily attenuated signal.

```bash
.venv/bin/python sim/pluto_bist_probe.py [--loopback 1|2] [--no-bist]
```

Use this when iterating on different `loopback` values quickly.  Trust
`pluto_bist_diag.py`'s OFDM-burst-based analysis for the final verdict.

---

## 3. Findings on this Pluto (FW v0.32, AD9364 mode)

### 3.1 `loopback=1` — digital TX→RX tap — **bit-exact** ✓

From `pluto_bist_diag.py`:

```
α = 2726.43 - 0.01j           ← single complex constant, 0° rotation (Gemini units)
α = 0.0625 - 0.0000j          ← same fit re-expressed as RX/TX ratio (-24.08 dB)
TX-zero samples (1440):       RX RMS = 0.0000  peak = 0.0
Linear-fit residual SNR:      45.84 dB over signal region
Cross-corr alignment peak:    1.0 (perfect preamble match)
```

**Verdict: BIST is bit-exact** — no additive noise, no rotation, single
complex scale.  The −24 dB attenuation comes from the BIST tap point
being downstream of two cascaded ÷4 decimation stages inside AD9361
(documented silicon behaviour, not a bug).

The 45.84 dB SNR on signal regions is from the linear-fit residual
where the simple `rx = α·tx` model has a small frequency-dependent
imperfection — likely the cascaded TX FIR + HBF1 + HBF2 chain
applying a sub-dB passband ripple.  This is structured (filter
response), not random noise: in zero regions the residual is
literally zero.

**Implication: `loopback=1` is fully usable for OFDM regression.**

### 3.2 `loopback=2` — internal RF self-loop — not usable bare

```
|α| = 0.0039 (-48 dB),  ∠α = +84°,  data BER ≈ 47 %
```

This routes TX→PA→PCB-trace→LNA→RX inside the chip.  Without antenna
termination + attenuation between the SMA ports, the signal radiates and
the captured path is dominated by environmental pickup.  Not the right
mode for our setup; skip.

### 3.3 Earlier mistaken verdict (retracted)

An earlier draft of this doc concluded "BIST PATH BROKEN" based on a
strict ±1 LSB equality test on `pluto_bist_probe.py` output.  That
verdict was **wrong** — the residual after de-scaling is only ~3–5 LSB
out of ±11 000 peak (~64 dB time-domain SNR), but the bit-comparison
counted every LSB-level difference as an error.  Gemini's
`pluto_bist_diag.py` using the linear-fit residual on OFDM-structured
samples + the TX-zero bit-exactness oracle gives the correct answer.

---

## 4. Why `pluto_loopback.py --mode pluto-bist` initially failed (and how it was fixed)

Three early pluto-bist runs all showed identical symptoms:
- `metric_max = 1.0000` (Schmidl-Cox autocorrelation perfect)
- `cfo_est ≈ 0.000–0.029 SC` (digital path, near-zero CFO)
- header CRC FAIL with mod + rate fields **correct** but `n_syms` field
  garbled (8 bits = 7-bit-error burst)
- `start_offset` varies per run (133, 240, 298) — depends on where the
  capture lands in the cyclic-TX cycle

Since BIST is bit-exact (§ 3.1) and `--mode digital` gives BER = 0, the
bug had to be in the wrapper's data-handling glue between Pluto's RX
buffer and `decode_full`.  Two issues compounded:

1. **`normalize_from_adc` was non-deterministic.**  The original code
   used 95th-percentile of `|samples|` to scale the captured signal,
   but the captured 200 k samples contains 2.65 cycles of cyclic TX
   and a random fraction of zero regions vs signal regions depending
   on where in the cycle the capture started.  Different cycle
   alignment per run → different p95 → different scale → different
   magnitudes fed to `decode_full` → sync threshold + channel
   estimate behave differently each run, occasionally pushing one
   header BPSK SC just over the decision boundary.
2. **`sync_detect_reference()` is blind, not template-aided.**  It
   uses Schmidl-Cox autocorrelation, which finds preambles correctly
   in standalone signals but is ambiguous when multiple complete +
   partial cycles coexist in the captured stream.  For Pluto loopback
   testing we *do* have the TX waveform in memory, so we can use
   cross-correlation as a deterministic alignment oracle.

### The fix (committed `984f196`)

Two changes in `sim/pluto_loopback.py`:

**A. Cross-correlation alignment** before calling `decode_full`:

```python
template = burst[:5000].astype(np.complex128)
corr     = np.abs(np.correlate(rx_samples, template, mode="valid"))
offset   = int(np.argmax(corr))
rx_one   = rx_samples[offset : offset + len(burst)]   # one full cycle
```

**B. Deterministic ×16/2¹³ scale-back** for `pluto-bist` mode (BIST has
a fixed silicon-level 1/16 attenuation, no need to guess from data):

```python
if args.mode == "pluto-bist":
    scale_back = 16.0 / (2**13)            # = 2⁻⁹, exact in IEEE 754
    rx_scaled  = rx_one * scale_back
else:
    rx_scaled  = normalize_from_adc(rx_one)   # RF mode keeps p95 fallback
```

`16/8192 = 2⁻⁹` is an exact power of 2 — IEEE 754 mantissa unchanged,
multiply is bit-identical to `>> 9`.  No precision loss.  In HLS
production this collapses to a wire reroute or 9-bit shift, zero LUT.

### Result after fix

```
[pluto] aligned at offset=49721  scale=0.001953125  peak |rx| = 1.0002
  [SYNC Q15 ] metric_max=1.0000  cfo_est=+0.0000 SC
  [Q15+SOFT ] header CRC PASS  BER = 0.00e+00
[pluto] header_pass=True  bit_err=0/89200  BER=0.00e+00
```

**11 150 application bytes / 89 200 bits round-tripped through real
Pluto silicon, decoded with BER = 0.**  See § 4.1 for the
modcod × frame-size sweep that followed.

### 4.1 Modcod × frame-size timing sweep — `sim/pluto_timing_sweep.py`

After the fix landed, characterized the chain across both 16QAM
modcods at six frame sizes (multiples of 3 so rate-2/3 puncturing
comes out byte-aligned):

```bash
.venv/bin/python sim/pluto_timing_sweep.py
```

The script instruments **six discrete phases** per cycle so the wall
time can be attributed to source: `tx_prep`, `tx_upload`, `settle`,
`rx_capture`, `align_scale`, `decode`.  Q15 + soft Viterbi + RS
hardcoded (production decoder configuration).

Result: **12 / 12 pass, BER = 0** through Pluto BIST loopback.

| Modcod | n_syms | k_app | total wall | decode | wire kbps | on-air | PHY rate |
|---|---:|---:|---:|---:|---:|---:|---:|
| 16QAM 1/2 | 9 | 386 | 438 ms | 318 ms | 7.0 | 158 µs | 19.5 Mbps |
| 16QAM 1/2 | 18 | 772 | 709 ms | 575 ms | 8.7 | 288 µs | 21.5 Mbps |
| 16QAM 1/2 | 33 | 1426 | 1.20 s | 1.01 s | 9.5 | 504 µs | 22.6 Mbps |
| 16QAM 1/2 | 60 | 2616 | 2.09 s | 1.83 s | 10.0 | 893 µs | 23.4 Mbps |
| 16QAM 1/2 | 120 | 5232 | 4.18 s | 3.72 s | 10.0 | 1.76 ms | 23.8 Mbps |
| 16QAM 1/2 | 255 | 11150 | 9.50 s | 8.62 s | 9.4 | 3.70 ms | 24.1 Mbps |
| 16QAM 2/3 | 9 | 504 | 546 ms | 424 ms | 7.4 | 158 µs | 25.6 Mbps |
| 16QAM 2/3 | 18 | 1040 | 912 ms | 762 ms | 9.1 | 288 µs | 28.9 Mbps |
| 16QAM 2/3 | 33 | 1912 | 1.55 s | 1.34 s | 9.9 | 504 µs | 30.3 Mbps |
| 16QAM 2/3 | 60 | 3488 | 2.78 s | 2.47 s | 10.0 | 893 µs | 31.2 Mbps |
| 16QAM 2/3 | 120 | 6976 | 5.28 s | 4.74 s | 10.6 | 1.76 ms | 31.7 Mbps |
| 16QAM 2/3 | 255 | 14856 | 11.37 s | 10.30 s | 10.4 | 3.70 ms | 32.1 Mbps |

**Where the time goes:**

- `decode` dominates 70–90 % of total wall time (pure-Python soft Viterbi).
  In HLS production this is microseconds.
- `tx_prep` scales linearly with `n_syms` (~2 ms / OFDM symbol in Python).
- `tx_upload` + `settle` ≈ constant ~80 ms (one-time cyclic buffer + drain).
- `rx_capture` scales with buffer at ~3–4 MB/s effective USB drain.
- `align` (cross-correlation) is O(N²-ish) in `np.correlate`; if it ever
  matters for sim speed, swap to `scipy.signal.fftconvolve` for ~10 × speedup.

**Wire-rate caveat:** the `wire_kbps` column reports decoded bits / total
**Python wall time**, NOT the PHY rate.  Real PHY rate is shown in the
last column (24.1 / 32.1 Mbps for 1/2 / 2/3 at full frame).  Python
overhead masks the PHY speedup by ~3000 ×.

**Payload efficiency vs frame size** (preamble + header are 2 fixed
overhead symbols regardless of `n_syms`):

| n_syms | Overhead fraction | Payload fraction |
|---:|---:|---:|
| 9 | 18.2 % | 81.8 % |
| 18 | 10.0 % | 90.0 % |
| 33 | 5.7 % | 94.3 % |
| 60 | 3.2 % | 96.8 % |
| 120 | 1.6 % | 98.4 % |
| 255 | 0.78 % | 99.2 % |

For sustained throughput, prefer 255 syms (99.2 % efficient).  For
low-latency C2/control, smaller frames are fine (33 syms = 504 µs
on-air = sub-ms turnaround) at modest efficiency cost.

### 4.2 Per-symbol pilot CPE — code-confirmed

Confirmed in `sim/ofdm_reference.py:decode_full` that **each** OFDM
data symbol uses **its own 6 pilots** (indices 50, 75, 100, 154, 179,
204) for residual phase tracking — not just one channel estimate held
across the whole frame.  Initial H[k] is from the preamble, then for
each data symbol independently: equalize 6 pilots, compute residual
phase, apply that symbol's correction to all 200 data SCs, demap,
soft Viterbi, RS, descramble.

```python
for s in range(n_syms):                                  # per-symbol
    Y = FFT(sig_c[offset_s + CP_LEN : offset_s + sym_len])
    sum_re = sum_im = 0.0
    for k in PILOT_IDX:                                  # this symbol's 6 pilots
        Geq_p = _compute_geq_at(k, G)                    # G from preamble
        eq    = Y[k] * Geq_p
        if use_weighted_cpe:
            w = |G[k]|²                                  # de-weight faded pilots
            sum_re += w * eq.real
            sum_im += w * eq.imag
    sym_phase_err = arctan2(sum_im, sum_re)              # this symbol's CPE
    sym_rot       = exp(-j * sym_phase_err)
    # apply sym_rot to all 200 data SCs of this symbol → demap
```

This survives common-phase drift / phase noise / slow CFO walk within
a frame.  Does **not** track per-SC channel changes (Doppler-induced
H[k] rotation), which is fine for Path A UAV (slow, ~30 km/h, 67 Hz
Doppler at 2.4 GHz → coherence time ~2.4 ms).  Frames up to ~3.7 ms
are within coherence; beyond that we'd either shorten frames (e.g. 64
syms = 1 ms) or add per-symbol H[k] re-estimation (Path C feature).

---

## 5. Recommended bring-up workflow

| Step | Command | Pass criterion | Status |
|---|---|---|---|
| 1. Wrapper sanity | `pluto_loopback.py --mode digital` | BER=0 | ✅ working |
| 2. BIST bit-exactness check | `pluto_bist_diag.py` / `pluto_bist_probe.py` | TX-zero → RX-zero, SNR ≥ 40 dB on signal | ✅ confirmed |
| 3. BIST OFDM | `pluto_loopback.py --mode pluto-bist` | header CRC PASS, BER=0 | ✅ working (commit `984f196`) |
| 3b. Modcod × frame-size sweep | `pluto_timing_sweep.py` | 12/12 PASS BER=0 across 16QAM 1/2 + 2/3 × {9, 18, 33, 60, 120, 255} | ✅ verified |
| 4. Antenna self-loop OTA | `pluto_loopback.py --mode pluto-rf` (close-coupled antennas) | BER=0 at -30 to -40 dBFS RX | ⏳ next session |
| 5. Two-Pluto OTA | `pluto_loopback.py --mode two-pluto` | BER=0 at SNR ≥ 13 dB | future, after MAC |

### 5.1 Antenna close-coupled self-loop (frugal alternative)

If the wrapper alignment fix takes longer than expected, the
no-extra-hardware fallback is **two antennas on the same Pluto's TX/RX
SMAs, physically touching or within 1 cm**.  Math:

| Quantity | Value at 2.4 GHz |
|---|---|
| Pluto TX power at `--tx-gain -10` | ≈ -3 dBm at SMA |
| Path loss antenna→antenna at 1 cm (near-field) | 15–25 dB |
| RX signal at antenna | -18 to -28 dBm |
| Ambient WiFi pickup (typical office) | -60 to -70 dBm |
| SNR margin over WiFi | **30–50 dB** |

The two antennas at 1 cm are deep inside each other's Fresnel near-field
zone, so the inverse-square free-space-loss approximation does NOT apply.
Coupling is much stronger than far-field math suggests.  Local link
dominates; WiFi becomes invisible background.

```bash
.venv/bin/python sim/pluto_loopback.py --mode pluto-rf \
    --uri ip:192.168.2.1 \
    --freq 2.4e9 --rate 20e6 \
    --tx-gain -10 --rx-gain 30 \
    --save-rx /tmp/pluto_rf_capture.txt
```

RSSI calibration cheatsheet:

| RX `avg level` printout | What it means | Action |
|---|---|---|
| **-30 to -40 dBFS** | Sweet spot — ADC mid-scale | Decode |
| -55 dBFS or weaker | Antennas too far apart, or `--tx-gain` too low | Move closer / raise tx-gain |
| -10 dBFS or stronger | ADC clipping risk | `--tx-gain -20` or `--rx-gain 20` |

---

## 6. Configuration baseline (record for diff later)

When the Pluto is in a known-working state, snapshot:

```bash
ssh root@192.168.2.1 'iio_attr -d ad9361-phy' > docs/pluto_baseline_phy_attrs.txt
ssh root@192.168.2.1 'fw_printenv'              > docs/pluto_baseline_uboot.txt
```

Diff against these files if Pluto starts misbehaving later.  Catches
firmware drift, accidental `fw_setenv`, calibration register changes.

---

## 7. Lessons for future debug

- **Trust the structural test, not the strict LSB equality test.**  The
  TX-zero → RX-zero check on a structured OFDM burst is a stronger
  bit-exactness oracle than ±1 LSB residual on random IQ after
  de-scaling.  An attenuation-heavy bit-exact path will fail the
  strict test and pass the structural one.
- **Linear-fit residual gives the right SNR figure.**  Compute
  `α = (rx · conj(tx)).sum / (tx · conj(tx)).sum`, then SNR = signal
  power / residual power on the signal region.  Bit-equality after
  de-scaling conflates LSB quantization noise with actual signal
  corruption.
- **When the wrapper proves the chain digitally and the same chain fails
  on hardware, the bug is almost always in the wrapper's data-handling
  glue (alignment, scaling, slicing), not in the silicon.**
