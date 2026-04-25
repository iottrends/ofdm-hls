# OFDM HLS Optimization Guide — Fitting TX+RX on XC7A50T

> **HISTORICAL ARCHIVE.** Snapshot from the early optimisation phase
> (sync_detect v2, viterbi v2, pre-xfft-IP-swap).  Numbers below are
> superseded.  For the current resource utilisation see
> [`docs/RESOURCES.md`](../RESOURCES.md).
>
> Kept for the design rationale on each optimisation pass — those still
> apply, only the absolute numbers have moved.

## Status snapshot — 2026-04-10 (historical)

| Block | Before | After | Saving |
|-------|--------|-------|--------|
| sync_detect | 12,972 LUT / 40 DSP | **2,895 LUT / 10 DSP** | −78% LUT, −75% DSP ✅ |
| viterbi_dec | 13,388 LUT / 0 DSP | **9,338 LUT / 0 DSP** | −30% LUT ✅ |
| cfo_correct | 2,636 LUT / 27 DSP | — | **pending** |
| ofdm_tx/rx FFT | ~9K LUT inflated | — | **pending (Vivado xfft swap)** |

**Raw HLS total today:** ~52,445 LUT / ~132 DSP  
**After xfft IP swap (Vivado):** ~43,445 LUT / ~114 DSP  
**Artix-50T budget:** 32,600 LUT / 75 DSP  
**Remaining gap:** ~10,845 LUT over, ~39 DSP over

Two items close most of the remaining gap:
1. `cfo_correct` optimisation → saves ~23 DSP
2. Real xfft v9 IP in Vivado → saves ~9K LUT, ~18 DSP

---

## What Was Done — sync_detect v2

**Root cause of v1 cost (12,972 LUT / 40 DSP):**

| Problem | Fix | Saving |
|---------|-----|--------|
| `ap_fixed<32,2>` widening on ri/rq/si/sq → 32×32 multiply = 2 DSP each | Keep as `sample_t` (ap_fixed<16,1>) → 16×16 = 1 DSP | −8 DSP |
| Float metric: Pi²+Pq²/(Rl×R) — 3 float muls × ~3 DSP each | Integer cross-multiply comparison, no division, `BIND_OP fabric` | −9 DSP |
| `hls::atan2f` → float CORDIC ~6 DSP | Fixed-point shift-add CORDIC, `PIPELINE II=1` (not UNROLL) | −6 DSP, −3,596 LUT |
| Float metric comparison needed float storage | Tight integer types: `ap_int<7>`, `ap_uint<6>`, `ap_uint<13>`, `ap_uint<10>` | −LUT |

**C-sim note:** The integer cross-multiply comparison uses `#ifdef __SYNTHESIS__` guard.
C-sim uses float ratio (amplitude-independent). Synthesis uses integer comparison, which
requires signal amplitude ≥ ~0.18 — always satisfied for real RF hardware from ADC.

---

## What Was Done — viterbi_dec v2

**Root cause of v1 cost (13,388 LUT):**

| Problem | Fix | Saving |
|---------|-----|--------|
| Full 64-state `#pragma HLS unroll` on ACS → 64 parallel butterflies | `PIPELINE II=1` + `unroll factor=16` → 16 butterflies, 4 cycles/stage | −~2,000 LUT |
| `pred_word[sp] = value` in unrolled loop → 16-deep read-modify-write chain on 64-bit register → 23 ns critical path | Separate `pred_bits[N_STATES]` array (complete partition) + `PACK_PRED` loop (full unroll, constant indices → direct wires) | timing fixed |
| `fwd % CIRC_SIZE` with CIRC_SIZE=192 (non-power-of-2) → sequential divider, 35–36 cycles/iteration in traceback | Counter-based `circ_wr_idx` with conditional wrap | −3,552 cycles, removes 238 LUT divider |
| `n_padded = (n_data_bits + 95) / 96 * 96` → sequential divider (238 LUT + 394 FF) | WIN_LEN=128 (power of 2) → `(n_data_bits + 127) & ~127` (bit mask) | −476 LUT |
| `win_bits[96]` with complete partition → ~1,500 LUT MUX | `ap_uint<WIN_LEN>` packed register | −LUT |

---

## Remaining Work — cfo_correct (27 DSP → target 4 DSP)

`cfo_correct` applies a per-sample phase rotation: `IQ_out = IQ_in × e^{jφ}`.
Each sample needs a complex multiply (4 real muls = 4 DSP).
The 27 DSP reported means it's computing sin/cos in floating-point.

**Fix: fixed-point sin/cos LUT + CORDIC, same pattern as sync_detect atan2:**

```cpp
// Current (expensive):
float cos_phi, sin_phi;
hls::sincosf(float(phase_acc), &cos_phi, &sin_phi);  // ~6 DSP float CORDIC
sample_t out_i = (sample_t)(cos_phi * in_i - sin_phi * in_q);  // float muls
sample_t out_q = (sample_t)(sin_phi * in_i + cos_phi * in_q);

// Target (4 DSP):
// 1. sin/cos from fixed-point CORDIC or ROM LUT → ap_fixed<16,1>
// 2. complex multiply: 4× ap_fixed<16,1> × ap_fixed<16,1> → 4 DSP48
```

With a 256-entry sin/cos LUT (256 × 16-bit = 4 KB = 2 BRAM_18K) and fixed-point
interpolation, all DSP goes to the 4 actual multiply-accumulates. Target: ~2K LUT / 4 DSP.

---

## Remaining Work — FFT IP Replacement (Vivado)

The HLS FFT placeholder inflates reported numbers significantly:

| Instance | HLS reports | Real xfft v9 (Vivado) | Saving |
|----------|------------|----------------------|--------|
| TX IFFT (256-pt, 16-bit) | ~3,000 LUT, 9 DSP, 10 BRAM | ~675 LUT, 9 DSP, 2 BRAM | ~2,325 LUT |
| RX FFT (256-pt, 16-bit) | ~6,000 LUT, 9 DSP, 8 BRAM | ~1,350 LUT, 9 DSP, 2 BRAM | ~4,650 LUT |
| **Total** | **~9,000 LUT** | **~2,025 LUT** | **~6,975 LUT** |

This is handled at Vivado IP Integrator time — replace `hls::fft<>` calls with xfft IP
instances. The `scale_sch` encoding for `pipelined_streaming_io` (Radix-4, 4 stages):

| Architecture | scale_sch for ÷N=256 | Encoding |
|---|---|---|
| `pipelined_streaming_io` (Radix-4) | `0xAA` | 10_10_10_10 (each stage ÷4) |
| `radix_2_burst_io` (Radix-2) | `0xFF` | 11111111 (each stage ÷2) |

See `docs/OFDM_HLS_ANALYSIS.md` for the full Radix-4 scale_sch analysis.

---

## Projected Budget After Remaining Work

| Item | LUT | DSP |
|------|-----|-----|
| Current adjusted total | ~43,445 | ~114 |
| cfo_correct v2 | −~600 | −23 |
| xfft IP swap | −~6,975 | 0 |
| **Projected total** | **~35,870** | **~91** |
| Artix-50T budget | 32,600 | 75 |
| Remaining gap | ~3,270 | ~16 |

Still slightly over on both. Vivado P&R optimisation typically reduces HLS-estimated LUT
by 10–15%, closing the LUT gap. DSP: adding `BIND_OP fabric` to any remaining multiplies
in ofdm_rx (equaliser) could recover the remaining DSPs.

---

## Optimization Order (Remaining)

1. **cfo_correct v2** — fixed-point sin/cos (biggest DSP win, ~15 min change)
2. **Vivado xfft IP swap** — done at integration time, no HLS changes needed
3. **ofdm_rx equaliser DSPs** — audit for unnecessary float, apply `BIND_OP fabric`
4. **BER re-validate** after each change with `./run_loopback.sh` + `./run_ber_sweep.sh`

---

## Quick Verification After Each Change

```bash
./run_loopback.sh                      # clean channel, BER must = 0
./run_ber_sweep.sh --mod 0 --quick     # QPSK quick sweep, BER trend must be similar
./setup_vitis.sh cfo_correct_synth     # check LUT/DSP numbers
./setup_vitis.sh rx_synth              # check full RX
```
