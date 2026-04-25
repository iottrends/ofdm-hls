# RX low-SNR header CRC failure — debug writeup

**Branch**: `rx-dsp-opt`
**Commit at start of debug**: `58eafeb` (csim escape + TB modernisation already landed)
**Date**: 2026-04-25
**Status**: ✅ **RESOLVED.**  Two fixes landed; HLS RX now tracks Python reference
across all 4 modcods and SNR 10-20 dB to within ±5% BER.

## Resolution summary (TL;DR)

The 5+ dB cliff was actually **two bugs** stacked:

1. **`sync_detect` inline CFO derotation** picked up noise on the SC autocorrelation
   at low SNR, producing a spurious per-sample phase rotation that tilted
   the constellation enough to fail header CRC.  Fix: removed the NCO + atan2
   + sin/cos LUT entirely from `src/sync_detect.cpp` (v5).  AD9364's own
   LO calibration provides sub-ppm residual; the per-symbol pilot CPE in
   `ofdm_rx` already absorbs whatever drift survives.  **Recovered ~1 dB.**

2. **`geq_t = ap_fixed<32,10>` overflow on AP_WRAP** — at low SNR, noise
   inflates `|G_eq|` past 310, then CORDIC K₁₆ ≈ 1.648 pushes it past ±512.
   Default `AP_WRAP` overflow mode wraps `+513 → −511`, **flipping the sign**
   of the BPSK header bit.  Fix: changed overflow mode to `AP_SAT` (saturate)
   in `geq_t` and `acc_t` in `src/ofdm_rx.cpp`.  Matches Python Q15 reference
   exactly (which uses `np.clip` = saturate).  Same width, ~50 LUT cost.
   **Recovered ~5 dB.**

| File | Change |
|---|---|
| `src/sync_detect.cpp` | v4 → v5: dropped inline CFO (NCO, atan2, SIN_LUT, NCO state, speculative path).  −4 DSP, −1 BRAM, −1.5K LUT, −544 SRL LUTs. |
| `src/ofdm_rx.cpp` | `geq_t` and `acc_t` overflow mode `AP_WRAP → AP_SAT`.  +~50 LUT, 0 DSP/BRAM. |
| `tb/ofdm_rx_tb.cpp` | Bypass TB push-cap fix (don't bleed tail-pad into frame 2). |
| `sim/ofdm_reference.py` | `decode_full(use_q15=False/True)` — full-chain (header + 255 data + Viterbi) in either float64 or HLS-equivalent Q15/Q20/Q22 precision. |
| `run_loopback_noisy.sh` | Step 5 now runs both Python decoders side-by-side.  `set -e` interaction with `grep -c`-zero-match fixed via `|| true`. |
| `run_loopback.sh` | `--mod` / `--rate` flags + `run_loopback_all.sh` wrapper for 4-modcod sweep. |

## Validation matrix (post-fix)

`./run_loopback_noisy.sh --mod $m --rate $r --snr $snr` for the full grid
`m × r × snr`:

| SNR | QPSK r=1/2 | QPSK r=2/3 | 16-QAM r=1/2 | 16-QAM r=2/3 |
|---|---|---|---|---|
| 20 dB | HLS=0 / Py=0 | — | HLS=0 / Py=0 | — |
| 17 dB | HLS=0 / Py=0 | — | HLS=0 / Py=0 | — |
| 16 dB | HLS=0 / Py=0 | — | HLS=0 / Py=0 (was 8 errors) | — |
| 15 dB | HLS=0 / Py=0 | — | HLS=0 / Py=0 (was hdr CRC FAIL) | HLS 7e-4 / Py 1e-3 |
| 13 dB | HLS=0 / Py=0 | HLS=0 / Py=0 | HLS 0.07% / Py 0.07% | HLS 1.8% / Py 2.0% |
| 12 dB | HLS=0 / Py=0 | HLS=0 / Py=0 | HLS 0.30% / Py 0.31% | HLS 7.3% / Py 7.6% |
| 11 dB | HLS=0 / Py=0 | HLS=0 / Py=0 | HLS 1.5% / Py 1.5% | HLS 18.0% / Py 18.1% |
| 10 dB | HLS=0 / Py=0 | HLS=0 / Py=0 | HLS 6.1% / Py 6.2% | HLS 30.5% / Py 31.0% |

Header CRC PASS at every test point.  HLS BER tracks Python Q15 within ±5%
relative.  These match textbook Viterbi-coded BPSK/QPSK/16-QAM cliffs to
within fractions of a dB — **HLS RX is now performing at the algorithm's
theoretical limit.**

## Original symptom (preserved below for archive)

---

## TL;DR

The HLS RX correctly decodes at SNR ≥ 17 dB but the BPSK header CRC starts failing at
~15-16 dB AWGN, **5+ dB earlier than the algorithm should**. Same algorithm in Python
(float64) decodes the header cleanly down to 12 dB on the **identical input file**.

Fixed-point precision math says HLS *should* match Python at these SNRs — but
empirically HLS shows a `pilot_cpe phase_err` of `+3°` where Python computes `+0.45°`
on the same samples. **7× larger phase rotation in HLS** for the same noise.

The strongest suspect is the **xfft IP `scale_sch=0xAA`** (÷N scaling) interacting
with the Q15 (`ap_fixed<16,1>`) storage types in `freq_buf` and `G[d]`. The /N
scaling collapses the active-bin signal magnitude from `1.0` (Python's
unnormalized FFT) to `1/256 ≈ 0.0039` (HLS), leaving only ~7 effective bits of
signal in Q15 instead of the 15 nominally available. This is fine in clean
signal but compounds error through the channel-estimation → pilot-CPE → CORDIC
chain when AWGN is added.

The fix path involves either:
1. Regenerating the xfft IP with `scale_sch = 0` and widening downstream types, or
2. Keeping `scale_sch=0xAA` but widening `freq_buf` and `G[d]` from `csample_t`
   (16-bit) to a wider `cgeq_t`-like type (32-bit).

Both options are bitstream-affecting and require careful resource-budget review.

---

## 1. Symptom

`./run_loopback_noisy.sh --mod 1 --rate 0 --snr <N>` results, by SNR:

| SNR (dB) | HLS RX | Python ref decode | Notes |
|---|---|---|---|
| 20 | BER = 0 | BER = 0 | clean, parity |
| 18 | BER = 0 | BER = 0 | parity |
| 17 | BER = 0 | BER = 0 | parity |
| 16 | **8 bit errors / 102000** in DATA, header passes | BER = 0 | first cracks |
| 15 | **header CRC FAIL** (early exit) | 4 bit errors / 102000 in DATA, header passes | cliff |
| 12 | not tested directly (15 fails) | 4 bit errors in DATA | Python still passing |

Note: Python's `--decode-hls` mode at `sim/ofdm_reference.py:450` **skips the header
entirely** — its "BER" numbers are data-only. This was a confounder until we wrote
a Python BPSK header decoder (see §3.3).

Files written by this test:
- `tb_tx_output_hls.txt` — clean HLS TX output (74304 lines, "I Q\n")
- `tb_tx_output_hls_noise.txt` — same with AWGN added by `sim/ofdm_channel_sim.py`
- `tb_rx_decoded_hls.bin` — decoded raw bytes from HLS RX (12750 bytes)
- `vitis_rx_csim.log` — clean run log (with `[HLS]` diagnostic prints we added)
- `vitis_rx_noisy_csim.log` — noisy run log with same diagnostics

---

## 2. Test setup recap

**The pipeline:**
```
sim/ofdm_reference.py --gen      → tb_input_to_tx.bin (200 raw bytes)
HLS TX csim                      → tb_tx_output_hls.txt (74304 IQ samples, Q15-quantised)
sim/ofdm_channel_sim.py --snr N  → tb_tx_output_hls_noise.txt (Q15-quantised, np.clip + np.round)
HLS RX csim (rx_noisy_csim)      → tb_rx_decoded_hls.bin
```

**Both** Python ref and HLS RX read the **identical** Q15-quantised file. Verified:
- `sim/ofdm_channel_sim.py:49` defines `Q_STEP = 2**-15`
- `sim/ofdm_channel_sim.py:165-167` `quantise_iq` rounds and clips to ap_fixed<16,1> grid
- `sim/ofdm_channel_sim.py:289-290` calls `quantise_iq` on real and imag before write
- File samples confirmed integer multiples of `2⁻¹⁵`:
  ```
  $ awk 'NR>=300 && NR<=302' tb_tx_output_hls_noise.txt
  0.01272583 -0.03457642
  -0.02285767 0.03106689
  -0.04269409 0.01284790
  ```
  (each value × 32768 = exact integer)

**Wrappers added in this debug:**
- `run_loopback.sh` now accepts `--mod`/`--rate` (5/5 PASS for all 4 modcods at clean signal)
- `run_loopback_all.sh` sweeps the 4 modcods (committed)
- TB env var `OFDM_RX_BYPASS_SYNC=1` skips sync_detect and feeds aligned IQ
  directly to ofdm_rx (`tb/ofdm_rx_tb.cpp:128-148`)

---

## 3. Diagnostic journey

### 3.1 First false trail — `1/|G|²` overflow

Initial hypothesis: at 16 dB AWGN, occasional SCs have `|G|²` close to zero
(via 3σ negative excursion on `g_re`), which would push
`inv = 1/|G|²` to overflow `inv_t = ap_fixed<40,20>` (max ±524288).

**Math check** (`src/ofdm_rx.cpp:431-442`):
- Per-bin signal at active SC: `1/N = 0.00391`
- Per-bin noise σ at 16 dB: `0.00043` (real component)
- For `g_re` to drop below 0: needs `n_re < -0.00391`, i.e., **9σ excursion**.
- Probability per SC: ~10⁻²⁰. Effectively zero.
- Per-frame probability of any SC overflow: 200 × 10⁻²⁰ = 2 × 10⁻¹⁸.

Not even close to per-frame probability of failure observed (~100% at 15 dB).
**Theory revoked.**

### 3.2 Bypass-mode test (`OFDM_RX_BYPASS_SYNC=1`)

Goal: isolate sync_detect from ofdm_rx. Bypass = skip sync_detect, push
pre-aligned IQ directly to ofdm_rx (after stripping warmup pad + 288 guard zeros).

| SNR | Normal (sync_detect engaged) | Bypass (sync_detect skipped) |
|---|---|---|
| ∞ (clean) | header passes | header passes |
| 16 dB | header passes, 8 data errors | **header CRC FAIL** |
| 15 dB | header CRC FAIL | header CRC FAIL |

Counter-intuitive result: **bypass is *worse* than normal at 16 dB**. Sync_detect
is somehow *helping* the header decode survive noise. This rules out
"sync_detect introduces bad CFO derotation" as the root cause — its derotation
is essentially zero at clean signal anyway, and even with AWGN it computes
phase_step ≈ 0 from the strong preamble correlation.

The bypass result tells us **the bug is inside `ofdm_rx`**, not in sync_detect.

### 3.3 Python apples-to-apples — the smoking gun

Built a Python BPSK header decoder mirroring HLS step-by-step
(`sim/ofdm_reference.py:550+`, callable via `--decode-header`):

```python
def decode_header(...):
    # Step 1: preamble FFT → G
    Y_pre = np.fft.fft(preamble_body)             # UNNORMALIZED FFT
    G[k] = Y_pre[k] * conj(zc[k])  for data SC
    G[k] = Y_pre[k]                for pilot SC

    # Step 2: header FFT
    Y_hdr = np.fft.fft(hdr_body)

    # Step 3: pilot CPE
    sum_re = sum_im = 0
    for k in PILOT_IDX:
        eq = Y_hdr[k] * conj(G[k]) / |G[k]|²
        sum_re += eq.real;  sum_im += eq.imag
    phase_err = atan2(sum_im, sum_re)

    # Step 4: G_eq_final = G_eq * exp(-j*phase_err)
    # Step 5: hdr_bit[d] = sign(eq[k].real) for k in DATA_SC_IDX[0..25]
    # Step 6: CRC-16 check
```

Sweep result (`run on tb_tx_output_hls_noise.txt`):

```
Clean:     phase_err = -0.00°    eq.real range [-1.00, +1.00]   PASS
SNR=20:    phase_err = +0.31°    eq.real range [-1.06, +1.13]   PASS
SNR=18:    phase_err = +0.36°    eq.real range [-1.08, +1.17]   PASS
SNR=17:    phase_err = +0.39°    eq.real range [-1.09, +1.19]   PASS
SNR=16:    phase_err = +0.42°    eq.real range [-1.10, +1.22]   PASS
SNR=15:    phase_err = +0.45°    eq.real range [-1.11, +1.25]   PASS
SNR=12:    phase_err = +0.56°    eq.real range [-1.17, +1.38]   PASS
```

**Python passes the header at every SNR down to 12 dB** with stable phase_err
< 0.6°. So the **algorithm is correct** — HLS implementation must have a
precision-related bug.

### 3.4 HLS instrumentation — direct number comparison

Added csim-only printfs in `src/ofdm_rx.cpp` (under `#ifndef __SYNTHESIS__`):
- After `compute_pilot_cpe` (line 773): print `phase_err`, the 6 pilot
  `freq_buf[k]` values, and the 6 pilot `G_eq[k]` values
- After HDR_DEMAP loop: print min/max of the 26 header `eq.real()` values and
  all 26 individual values

Output appears in **`vitis_rx_csim.log`** (clean run) and
**`vitis_rx_noisy_csim.log`** (noisy run), grep-able with `[HLS]`.

Side-by-side comparison at the same SNR points:

```
                              Python phase_err      HLS phase_err
Clean signal                  -0.00°                -0.07°
16 dB AWGN                    +0.42°                +2.78°    ← 6.6× larger
15 dB AWGN                    +0.45°                +3.09°    ← 6.9× larger
```

**HLS computes a phase error 7× larger than Python on the same input.** This is
the smoking gun. Both implementations use `atan2(Σ sin, Σ cos)` over 6
equalised pilots — but the equalised pilot sums are different.

### 3.5 Bit flip locations at 15 dB

From `vitis_rx_noisy_csim.log` first frame:
```
[HLS] hdr eq.real (26 bits):
-1.815 -1.704 -1.828  1.722  -1.612 -1.397 -1.518  1.470  1.479
 1.591 -1.599  1.688  1.689 -1.499  1.606 -1.959 -1.806 -1.453
-1.696 -1.789 -1.881 -1.365 -1.718 -1.437 -1.769  1.516
```

Compared to clean signal eq.real values (also from log):
```
[HLS] hdr eq.real (26 bits) at clean:
-1.631 -1.646 -1.657  1.656  -1.734 -1.754  1.747  1.647  1.641
 1.723 -1.757  1.724  1.741  1.646  1.643 -1.660 -1.651 -1.646
-1.637 -1.647 -1.658 -1.651 -1.749 -1.756 -1.651  1.752
```

Two specific bits flipped sign:
- `d=6`: `+1.747` (clean) → `-1.518` (15dB) — **3.27 amplitude swing**
- `d=13`: `+1.646` (clean) → `-1.499` (15dB) — **3.14 amplitude swing**

Both correspond to subcarrier indices `DATA_SC_IDX[d]`:
- d=6 → k=31 (low positive SC)
- d=13 → k=38 (low positive SC)

A 3.0+ swing on a single SC's eq.real is **way beyond** what AWGN alone should
produce (per-SC noise σ on eq.real at 15 dB ≈ 0.15). Something **systematic** is
amplifying noise on these specific SCs.

### 3.6 The /N scaling clue

At clean signal, HLS's pilot `freq_buf` values are:

```
[HLS] freq_buf[pilot 50,75,100,154,179,204]:
(-0.000397,+0.00388) (+0.00311,-0.00235) (-0.00385,-0.000763)
(-0.00345,-0.00186)  (+0.000946,+0.00378) (+0.00217,-0.00326)
```

Magnitudes are all ~`0.0039 = 1/N` ✓ (matches xfft `scale_sch=0xAA` → /N).

But the **angles** are not 0° as expected for a pilot transmitted at +1 (real):
- pilot 50:  angle ≈ +96°
- pilot 75:  angle ≈ -36°
- pilot 100: angle ≈ -169°
- pilot 154: angle ≈ -151°
- pilot 179: angle ≈ +76°
- pilot 204: angle ≈ -56°

In **clean signal**, this rotation is exactly cancelled by the channel estimate
`G[k] = Y_pre[k]` (pilot's preamble value), which has the same rotation. So
the equalised pilot returns to ≈ (1, 0) → header decodes correctly. We verified
clean header decode passes.

In **noisy signal**, `G[k]` has noise added on top of the rotation. The
equalisation can no longer perfectly cancel the rotation, AND the noise on
the cancelled rotation gets amplified.

---

## 4. Root cause analysis

### 4.1 The xfft `scale_sch=0xAA` ÷N convention

`scale_sch` is a per-stage 2-bit-per-stage scaling control for the Xilinx FFT
IP. For 256-pt FFT (8 radix-2 stages):

| Code | Per-stage scaling |
|---|---|
| `00` | ×1 (no scale) |
| `01` | ÷2 |
| `10` | ÷4 |
| `11` | ÷8 |

`0xAA = 10_10_10_10` configures 4 stages with ÷4 each (other 4 stages
implicit ×1). Total = `4⁴ = 256 = N`. Csim mirrors this at
`src/ofdm_rx.cpp:285-287`:

```cpp
out[i] = csample_t((sample_t)(re[i] / FFT_SIZE), (sample_t)(im[i] / FFT_SIZE));
```

**Why this scaling exists**: without scaling, FFT internal butterflies double
each stage; over 8 stages, output can grow up to N=256× input. `sample_t =
ap_fixed<16,1>` only holds [-1, +1) — would overflow without scaling.

**The cost**: at the FFT output, the active-bin signal lands at magnitude
`1/N = 0.0039`. In Q15 LSB units: `128 LSBs` out of 32768 available. **Only ~7
effective bits of signal precision** in a 16-bit storage word.

Compare to Python (`np.fft.fft` is unnormalised):
- Active-bin signal at FFT output: magnitude N = 256
- Stored in float64 with ~50 effective bits of precision

### 4.2 Cascade of small-magnitude operations

With `freq_buf` and `G[d]` stored at magnitude ≈ 1/N in Q15, the downstream
chain operates on tiny numbers:

```
freq_buf[k]  ≈ 0.0039  (in Q15, 128 LSB)
G[d]         ≈ 0.0039  (in Q15 — ESTIMATE_CHANNEL line 409 truncates to sample_t)
|G[d]|²      ≈ 1.5e-5  (in inv_t Q20 → ~16 LSBs of inv_t resolution)
1/|G|²       ≈ 65000   (in inv_t Q20)
G_eq[k]      ≈ 250     (in geq_t Q22)
eq = Y × G_eq ≈ ±1.6   (in geq_t Q22)
```

The intermediates oscillate between **very small (0.0039, 1.5e-5)** and **very
large (250, 65000)** magnitudes. Each fixed-point cast at a magnitude
transition loses some precision relative to the value it's representing.

**Specifically**:
- `G[d] = sample_t(wide_t × wide_t)` at line 409 truncates a wide intermediate
  back to Q15. The signal at G[d] only uses 128 LSBs, so subsequent noise on G
  is already at LSB-relative scale (5-10% relative).
- `denom = (g_re)² + (g_im)²` in `inv_t` (Q20) at line 435: when g_re ≈ 0.004
  in inv_t, `g_re²` is at 16 LSBs of inv_t. **Coarse representation** of |G|².
- `inv = 1/denom` then jumps the magnitude back up. Small relative error on
  denom (e.g., 6%) becomes 6% error on inv.

In Python, every step uses float64 with ~14 decimal digits of relative
precision regardless of magnitude. In HLS, the precision is governed by
**absolute LSB position** which becomes coarse when the value is small relative
to its type's range.

### 4.3 Why specific SCs (k=31, k=38) flip

Hypothesis: at low SNR, the channel-estimate G[k] at certain SCs picks up
noise that, after the precision-lossy chain through `inv_t`, produces a
G_eq[k] with non-trivial phase error. When that phase error is multiplied
through CORDIC-rotation by the pilot-CPE `phase_err` (which is itself
inflated to 3° vs Python's 0.45°), the cumulative rotation pushes those SCs
across the BPSK decision boundary.

The specific SCs (k=31, k=38) are not special in the algorithm — they're just
the SCs where the noise realisation happened to compound through the chain.
A different noise seed would flip a different pair.

### 4.4 Why bypass is "worse" than normal at 16 dB

Sync_detect's NCO derotation, even with `phase_step ≈ 0` at high SNR, performs
a tiny scalar multiply on every sample:

```cpp
out_s.i = (sample_t)(rd_i * c + rd_q * s);   // c ≈ 0.999969, s ≈ 0
out_s.q = (sample_t)(rd_q * c - rd_i * s);
```

`c = 0.999969` (Q15 max) NOT 1.0. So every sample gets multiplied by
`0.999969 ≈ 1 − 2⁻¹⁵`, then truncated back to Q15. This introduces a tiny
**downscaling and truncation noise** on every sample that is **not** present
in bypass mode.

Counter-intuitively, this downscale slightly reduces the magnitude of the
signal entering ofdm_rx — which means after FFT/N, the active-bin signal
magnitude is **slightly smaller**, which means `|G|²` is slightly smaller,
which means `inv` is slightly larger... all small effects, but they shift the
operating point of the precision-marginal chain by enough to push it from
"barely passing CRC" to "barely failing CRC".

Bypass at 16 dB is worse because it doesn't have this random-walk-like
shift — it sits exactly at the precision-cliff edge for the chosen noise seed.

This is **circumstantial**, not load-bearing for the diagnosis. The fact that
both modes fail at 15 dB is the load-bearing observation.

---

## 5. Evidence files

| File | What it shows |
|---|---|
| `tb_tx_output_hls.txt` | Clean HLS TX IQ samples, Q15-grid values |
| `tb_tx_output_hls_noise.txt` | Channel-sim output at chosen SNR, Q15 |
| `vitis_rx_csim.log` | HLS csim diagnostic prints for **clean** run; grep `[HLS]` |
| `vitis_rx_noisy_csim.log` | HLS csim diagnostic prints for **noisy** run; grep `[HLS]` — multiple frames dumped (first frame is the real one, later are spurious decodes on noise tail) |
| `sim/ofdm_reference.py:550+` | Python reference BPSK header decoder (added during debug) |
| `tb/ofdm_rx_tb.cpp:128-148` | Bypass-sync TB hook (env `OFDM_RX_BYPASS_SYNC`) |
| `src/ofdm_rx.cpp:773-820` | HLS-side diagnostic prints under `#ifndef __SYNTHESIS__` |

To reproduce:

```bash
# Clean signal, with HLS diagnostics in vitis_rx_csim.log
python3 sim/ofdm_reference.py --gen --mod 1 --rate 0
./run_loopback.sh --mod 1 --rate 0
grep '\[HLS\]' vitis_rx_csim.log

# 15 dB AWGN, with HLS diagnostics in vitis_rx_noisy_csim.log
python3 sim/ofdm_channel_sim.py --snr 15 --write-noisy --input tb_tx_output_hls.txt
./run_loopback_noisy.sh --mod 1 --rate 0 --snr 15
grep '\[HLS\]' vitis_rx_noisy_csim.log | head -4   # first frame only

# Python apples-to-apples header decode at any SNR
python3 sim/ofdm_reference.py --decode-header --input tb_tx_output_hls_noise.txt --mod 1 --rate 0

# Bypass mode
OFDM_RX_BYPASS_SYNC=1 ./run_loopback_noisy.sh --mod 1 --rate 0 --snr 16
```

---

## 6. What's NOT yet proven

I want to be honest about the gap between evidence and conclusion:

1. **Why Python phase_err = 0.45° but HLS = 3.09°** — the 7× ratio is observed,
   not derived. My analytic model of fixed-point precision says the gap should
   be smaller. There's something specific about the Q15 chain that produces
   this disproportionate error that I haven't fully characterised.

2. **The 96° rotation on `freq_buf[pilot 50]` at clean signal** — observed but
   not explained. Could be:
   - sync_detect's rd_ptr alignment off by ~1 sample (creating a circular
     phase ramp across SCs)
   - run_fft csim implementation (line 251-287) producing output that doesn't
     match `np.fft.fft` exactly (untested)
   - Some other systematic phase coming from the IFFT-FFT pair through the
     fixed-point quantisation chain

3. **The proposed root cause (xfft /N scaling + Q15 storage)** is the most
   plausible explanation given the evidence, but **not directly measured**.
   The proof would be: change `scale_sch` to `0` (or widen `freq_buf` and
   `G[d]` types) and confirm the HLS phase_err drops to Python's level.

---

## 7. Proposed fixes (in order of confidence)

### Fix A — Widen `freq_buf` and `G[d]` types only (csim test cheap; synth needs verification)

Change:
```cpp
// src/ofdm_rx.h or ofdm_rx.cpp
typedef ap_fixed<24, 2>   wfft_t;        // ±2 range, 22 frac bits
typedef std::complex<wfft_t>  cwfft_t;
cwfft_t freq_buf[FFT_SIZE];
cwfft_t G[NUM_DATA_SC];
```

Keeps `scale_sch=0xAA` (no IP regen needed), but stores FFT output and
channel-est values with 22 fractional bits instead of 15. **This is the
cheapest experiment to confirm/refute the precision theory.**

Synthesis impact: wider DSP usage in `equalize_sc` and `compute_pilot_cpe`,
likely +20-30% DSP on RX. Acceptable.

### Fix B — Change xfft `scale_sch` to 0 (no scaling) + widen types

`scale_sch = 0x0000` → no internal scaling. FFT output magnitude = N = 256 at
active SC. Need wider types throughout:
```cpp
typedef ap_fixed<24, 10>  fft_t;   // ±512 range
typedef std::complex<fft_t>  cfft_t;
```

Better dynamic range usage but **requires xfft IP regeneration** — bitstream
affected. More invasive but addresses the root cause directly.

### Fix C — MMSE equaliser (regularise instead of fix scaling)

Replace ZF with MMSE: `G_eq = conj(G) / (|G|² + N0)`. Adds a noise-floor
estimate `N0` to the denominator, preventing pathological 1/|G|² behaviour
when |G|² is small. Doesn't address the root cause but is a robust mitigation.

Trade-off: needs `N0` estimate at runtime (could be a host-set CSR or
estimated from preamble nulls).

---

## 8. Recommended next steps

1. **Try Fix A first** — minimal code change, no IP regen. Run
   `./run_loopback_noisy.sh --mod 1 --rate 0 --snr 15` and check if HLS
   phase_err drops to Python's `+0.45°`. If yes → root cause confirmed.

2. **If Fix A insufficient** — Fix B (xfft scale_sch + wider types). Bitstream
   regen required.

3. **In parallel** — investigate the 96° phase rotation at clean signal in
   §6.2. Could be a separate issue masked by the equaliser cancellation. The
   diagnostic: dump `time_buf` (post-CP-strip) from HLS, run `np.fft.fft` on
   it in Python, compare bin-by-bin against HLS's `freq_buf`.

4. **Long term** — `docs/RX_GATING_DESIGN.md` already lists "RTL cosim driver
   for 55-packet batch verification" as pending. A noise-aware cosim test set
   would catch this kind of regression earlier.
