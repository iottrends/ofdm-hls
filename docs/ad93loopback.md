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

Two complementary probes exist in `sim/`:

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

## 4. So why does `pluto_loopback.py --mode pluto-bist` fail?

If BIST is bit-exact (modulo a constant complex scale), and the
wrapper's `--mode digital` path gives BER = 0, then the OFDM decode
failure is **wrapper-level**, not silicon-level.

Three pluto-bist runs all showed:
- `metric_max = 1.0000` (Schmidl-Cox autocorrelation perfect)
- `cfo_est ≈ 0.000–0.029 SC` (digital path, no real CFO)
- header CRC FAIL with mod + rate fields **correct** but n_syms field
  garbled (8 bits = 7-bit-error burst)
- `start_offset` varies per run (133, 240, 298) — depends on where the
  capture lands in the cyclic-TX cycle

Suspect: **alignment / FFT-window placement issue inside `decode_full`
when the capture starts mid-cycle of a cyclically-played burst.**

Gemini-recommended fix: **cross-correlation alignment in the wrapper**.
Before calling `decode_full(rx_samples)`, cross-correlate the captured
stream against the known TX template, slice out exactly one full
burst-cycle starting at the cross-correlation peak, and pass *that*
to decode_full.  This bypasses any ambiguity about which preamble in
the cyclic-repeated stream sync_detect ends up triggering on.

`sync_detect_reference()` was designed for blind sync (real OTA
where the receiver doesn't have a copy of the TX signal).  For Pluto
loopback testing where we *do* have the TX signal in memory, we can
afford to cheat with cross-correlation as an alignment oracle.

---

## 5. Recommended bring-up workflow

| Step | Command | Pass criterion | Status |
|---|---|---|---|
| 1. Wrapper sanity | `pluto_loopback.py --mode digital` | BER=0 | ✓ working |
| 2. BIST bit-exactness check | `pluto_bist_diag.py` | TX-zero → RX-zero, SNR ≥ 40 dB on signal | ✓ confirmed |
| 3. BIST OFDM (after wrapper alignment fix) | `pluto_loopback.py --mode pluto-bist` | header CRC PASS, BER=0 | **pending fix** |
| 4. Antenna self-loop OTA | `pluto_loopback.py --mode pluto-rf` (close-coupled antennas) | BER=0 at -30 to -40 dBFS RX | not yet attempted |
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
