# Resource Utilization — Functional Block Breakdown

**Build**: LiteX SoC + OFDM PHY + AD9361 wrapper, full PCIe Gen2 x2.
**Target**: Xilinx XC7A50T-2CSG325I @ 100 MHz fabric clock.
**Toolchain**: Vivado 2025.2 + Vitis HLS 2025.2.
**Source report**:
  `litex/build/hallycon_m2sdr_platform/gateware/hallycon_m2sdr_platform_utilization_hierarchical_place.rpt`
**As of**: commit `d08a537` (post `rx-dsp-opt` branch close-out).

## Top-level summary

| Resource    | Used   | Available | Util  |
|-------------|--------|-----------|-------|
| Slice LUTs  | 25,913 | 32,600    | 79.5% |
| LUT as Logic| 23,549 | 32,600    | 72.2% |
| LUT as Mem  | 2,364  | 9,600     | 24.6% |
| Slice FF    | 32,450 | 65,200    | 49.8% |
| **DSP48E1** | **81** | 120       | **67.5%** |
| **BRAM Tile** | **67** | 75      | **89.3%** |
| RAMB36E1    | 50     | 75        | 66.7% |
| RAMB18E1    | 34     | 150       | 22.7% |
| Bonded IOB  | 57     | 150       | 38.0% |

**Timing**: WNS = +0.210 ns, all paths positive slack at 100 MHz.

## Per-block breakdown — OFDM PHY

| Block                    | LUT    | FF     | BRAM36 | BRAM18 | DSP | Notes |
|--------------------------|--------|--------|--------|--------|-----|-------|
| `ofdm_rx`                | 8,238  | 6,311  | 0      | 15     | **39** | Channel est + CPE + equalize + demap |
| `sync_detect`            | 2,933  | 4,663  | **4**  | 0      | **24** | Preamble detect + 4096-sample replay buffer |
| `ofdm_rx_fft` (xfft v9.1)| 1,843  | 3,378  | 0      | 2      | 9   | Xilinx FFT IP, RX |
| `ofdm_tx_ifft` (xfft v9.1)| 1,843 | 3,376  | 0      | 2      | 9   | Xilinx FFT IP, TX |
| `ofdm_mac`               | 1,767  | 1,850  | 1      | 0      | 0   | Header build/parse, FCS, m_axi |
| `ofdm_tx`                | 979    | 818    | 0      | 12     | 0   | Subcarrier mapping + CP insert |
| `tx_chain`               | 702    | 662    | 0      | 0      | 0   | Scrambler + conv_enc + interleaver |
| `fec_rx`                 | 2,828  | 5,656  | 1      | 0      | 0   | Interleaver(rx) + Viterbi + descrambler |
| ↳ `viterbi_dec` only     | 2,404  | 5,232  | 1      | 0      | 0   | (sub-component of fec_rx) |
| `adc_input_fifo`         | 40     | 69     | 4      | 1      | 0   | AD9364 input absorption FIFO |
| `fec_cc1`/`cc2` (×2)     | 132    | 216    | 0      | 0      | 0   | 100↔200 MHz AXIS clock converters |

DSP allocation by block:

```
ofdm_rx               39 DSP  (48%)
sync_detect           24 DSP  (30%)
ofdm_rx_fft  (Xilinx)  9 DSP  (11%)
ofdm_tx_ifft (Xilinx)  9 DSP  (11%)
─────────────────────────
TOTAL                 81 DSP  (67.5%)
```

## ofdm_rx internals (39 DSP, 8,238 LUT)

| Sub-block                | LUT   | FF    | BRAM18 | DSP | Notes |
|--------------------------|-------|-------|--------|-----|-------|
| `compute_geq_final`      | 4,088 | 2,323 | 1      | 0   | CORDIC rotation, no DSP (pure shift-add) |
| ↳ data SC rotate         | 1,989 | 1,138 | 1      | 0   | 200 SCs |
| ↳ pilot SC rotate        | 2,036 | 1,144 | 0      | 0   | 6 SCs |
| `compute_pilot_cpe`      | 1,888 | 1,104 | 0      | 8   | atan2 + complex mul of 6 pilots |
| `estimate_channel`       | 1,306 | 1,411 | 5      | 15  | `1/|G|²` reciprocal — biggest DSP user |
| `equalize_demap_pack`    | 538   | 661   | 1      | 12  | Y × G_eq + 16-QAM/QPSK demap |
| ↳ QPSK pack              | 152   | 209   | 0      | 8   | |
| ↳ 16-QAM pack            | 386   | 440   | 1      | 4   | |
| `HDR_DEMAP`              | 135   | 278   | 0      | 0   | 26-bit BPSK header demap |
| `run_fft` (wrapper)      | 91    | 68    | 0      | 0   | AXIS handshake to xfft IP |
| FFT regslices            | 75    | 138   | 0      | 0   | |

## sync_detect internals (24 DSP, 2,933 LUT, 4 BRAM36)

Top-level only — `sync_detect` is one HLS function (`while(1)` FSM) with
no internal DATAFLOW splits.

- **Circular buffer**: 4096 × 16-bit × 2 (I, Q) → 4 RAMB36 (the bulk of
  the BRAM cost). Could be reduced to 2048 → 2 RAMB36 with no functional
  loss.
- **DSPs**: 24 — running sums (P_re, P_im, R, Rl, pow_env), threshold
  cross-multiply, sample-magnitude products. Inline CFO derotation
  removed in v5 (was +4 DSP +1 BRAM in v4).
- **SRL chains**: 544 LUTs map to SRL32 — three delay lines
  (CP_LEN, FFT_SIZE, CP_LEN+FFT_SIZE) implementing the sliding-window
  reads at fixed offsets from `wr_ptr`.

## DSP journey on the `rx-dsp-opt` branch

| Commit  | Change                                           | DSP   |
|---------|--------------------------------------------------|-------|
| pre-branch baseline (assumed) |                                | ~100  |
| `71b4906` Opt-2: fold CPE rotator into G_eq_final          | −2    |
| `f19a9d2` Opt-1: CORDIC rotator for CPE (replaces NCO)     | −16   |
| `4f3113f` Apr 17 baseline (post Opt-1+Opt-2)               | **86**|
| `d08a537` sync_detect inline CFO removed (this session)    | −5    |
| Current                                                    | **81**|

Total saving: −19 DSP from start of branch to today. Resource budget:

- LUT: 25,913 / 32,600 (79.5%) — comfortable
- DSP: 81 / 120 (67.5%) — comfortable
- BRAM: 67 / 75 (89.3%) — **tight**, plan trim if more blocks land

## Where to look if more savings needed

In priority order:

1. **`compute_geq_final` LUT (4,088)** — could drop CORDIC iterations
   from 16 to 8 (small phase angles in clean operation, 8 stages give
   <0.5° error — good enough for BPSK header). Saves ~30-40% of those
   4 K LUTs.
2. **`estimate_channel` 1/|G|² divider (15 DSP)** — replace with
   reciprocal-LUT + Newton-Raphson correction. Saves ~6-8 DSPs.
3. **`compute_pilot_cpe` (8 DSP)** — share multipliers across 6 pilots
   sequentially instead of parallel. Saves ~4 DSPs at cost of latency.
4. **`sync_detect` buffer 4096 → 2048** — frees 2 RAMB36 (drops BRAM
   util to ~83%). Lowest-risk change.
5. **`sync_detect` accumulator widths** — `prod_t = ap_fixed<32,12>` may
   be wider than needed; trim to `<24,12>` saves ~3 DSPs.

Total potential: ~−15 DSP, −2 BRAM, −1.5 K LUT. Not needed at current
operating point.
