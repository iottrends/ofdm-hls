# ofdm-hls

Custom OFDM PHY layer implemented in Vitis HLS — full TX+RX chain targeting **Artix-7 50T + AD9364** at 20 MSPS.

Designed as the PHY for an open-source UAV video link (wfb-ng style), replacing proprietary stacks.

---

## System Parameters

| Parameter | Value |
|-----------|-------|
| FFT size | 256 |
| Cyclic prefix | 32 samples |
| Symbol duration | 288 samples (12.8 µs at 20 MSPS) |
| Data subcarriers | 200 |
| Pilot subcarriers | 6 |
| Modulation | QPSK (mod=0) or 16-QAM (mod=1), runtime-selectable |
| FEC | Rate-1/2 or rate-2/3 convolutional code (K=7, Viterbi decode) |
| Preamble | Zadoff-Chu (root 25) |
| Header | BPSK, 26 bits (mod + n_syms + CRC-16) |
| Target FPGA | Xilinx Artix-7 XC7A50T |
| RFIC | Analog Devices AD9364 |
| Sample rate | 20 MSPS |

---

## Chain Overview

```
TX:  raw bytes → scrambler → conv_enc → interleaver → ofdm_tx
                                                           ↓
                                              ZC preamble + BPSK header + QPSK/16QAM data
                                                           ↓
                                                    AD9364 → RF

RX:  RF → AD9364 → sync_detect → cfo_correct → ofdm_rx
                                                    ↓
                                         deinterleaver → viterbi_dec → descrambler → raw bytes
```

---

## Repository Structure

```
src/    — HLS synthesizable sources
tb/     — C++ testbenches
sim/    — Python reference model and simulation tools
tcl/    — Vitis HLS synthesis TCL scripts (static, version-controlled)
docs/   — Documentation
```

### `src/` — HLS Source (synthesizable)

| File | Description |
|------|-------------|
| `ofdm_tx.cpp / .h` | OFDM modulator — IFFT, CP insert, pilot insert |
| `ofdm_rx.cpp / .h` | OFDM demodulator — FFT, ZC channel estimation, equalizer |
| `sync_detect.cpp / .h` | Schmidl-Cox timing sync + CFO estimator |
| `cfo_correct.cpp / .h` | Carrier frequency offset correction (phase rotation) |
| `conv_enc.cpp` | Convolutional encoder, rate 1/2 and 2/3 (K=7) |
| `viterbi_dec.cpp` | Viterbi decoder (hard decision, sliding-window) |
| `conv_fec.h` | Shared FEC types and constants |
| `scrambler.cpp / .h` | 802.11a 7-stage LFSR scrambler/descrambler |
| `interleaver.cpp / .h` | 802.11a two-step bit interleaver/deinterleaver |

### `tb/` — Testbenches

| File | Description |
|------|-------------|
| `ofdm_tx_tb.cpp` | TX C-sim testbench — reads raw bytes, writes IQ output |
| `ofdm_rx_tb.cpp` | RX C-sim testbench — reads IQ, decodes, reports BER |
| `conv_fec_tb.cpp` | FEC encode+decode loopback test |

### `sim/` — Python Reference & Simulation

| File | Description |
|------|-------------|
| `ofdm_reference.py` | Full floating-point TX+RX reference model + EVM comparison |
| `fec_reference.py` | Python convolutional encoder and Viterbi decoder |
| `ofdm_channel_sim.py` | Channel simulator: AWGN, phase noise, multipath, CFO |
| `plot_ber.py` | BER vs SNR waterfall plot generator |
| `gen_zc_lut.py` | ZC sequence LUT generator |

### Root — Build & Test Scripts

| File | Description |
|------|-------------|
| `setup_vitis.sh` | Vitis HLS 2025.2 environment + all TCL flows (WSL) |
| `run_ber_sweep.sh` | Automated BER sweep across channels and SNR points |
| `run_loopback.sh` | Quick TX→RX loopback test (clean channel) |
| `run_loopback_noisy.sh` | TX→channel→RX loopback with noise |

---

## Resource Usage (Vitis HLS 2025.2, XC7A50T, 10 ns clock)

All numbers from `csynth_design`. FFT blocks use HLS placeholder — real Xilinx xfft v9 IP
saves ~9,000 LUT and ~18 DSP at Vivado implementation time.

### TX Chain

| Block | LUT | DSP | BRAM | Fmax |
|-------|-----|-----|------|------|
| ofdm_tx (full TX) | 17,488 | 18 | 36 | 138 MHz |

### RX Chain

| Block | LUT | DSP | BRAM | Fmax | Status |
|-------|-----|-----|------|------|--------|
| sync_detect | 2,895 | 10 | 2 | 110 MHz | v2 optimised |
| cfo_correct | 2,636 | 27 | 1 | 142 MHz | not yet optimised |
| ofdm_rx | 18,182 | 75 | 17 | 138 MHz | not yet optimised |
| interleaver | 1,042 | 2 | 0 | 171 MHz | done |
| viterbi_dec | 9,338 | 0 | 2 | 139 MHz | v2 optimised |
| conv_enc | 639 | 0 | 0 | 153 MHz | done |
| scrambler | 225 | 0 | 0 | 186 MHz | done |

### Budget vs Actual

| | LUT | DSP | BRAM |
|-|-----|-----|------|
| Raw HLS total | ~52,445 | ~132 | ~58 |
| After xfft IP swap (Vivado) | ~43,445 | ~114 | ~40 |
| Artix-50T budget | 32,600 | 75 | 100 |
| Remaining gap | ~10,845 over | ~39 over | within |

**Remaining optimisation targets:** `cfo_correct` (27 DSP → target 4 DSP) and
`ofdm_rx`/`ofdm_tx` FFT replacement with real xfft IP closes most of the gap.

---

## Validated Results

### EVM (TX accuracy)

HLS TX output vs NumPy floating-point reference (74,016 samples, 16-QAM, 255 symbols):

```
EVM = 0.056%  (-65 dB)   — ap_fixed<16,1> quantisation noise floor
PREAMBLE  EVM = 0.056%
HEADER    EVM = 0.132%
DATA[*]   EVM = 0.052–0.062%  (uniform across all 255 symbols)
```

### BER Sweep (RX chain, C-sim, 10 frames/point)

**QPSK, rate 1/2 (mod=0)**

| Channel | 5 dB | 10 dB | 15 dB | 20 dB | 25 dB |
|---------|------|-------|-------|-------|-------|
| AWGN | no sync | 9.8e-6 | 0 | 0 | 0 |
| Phase noise | 2.65e-1 | 1.4e-3 | 0 | 0 | 0 |
| Multipath (2-tap) | 2.3e-1 | 9.5e-3 | 5.5e-5 | 0 | 0 |
| Combined (worst) | 2.96e-1 | 4.0e-3 | 2.4e-3 | 0 | 0 |

**16-QAM, rate 2/3 (mod=1)**

| Channel | 5 dB | 10 dB | 15 dB | 20 dB | 25 dB |
|---------|------|-------|-------|-------|-------|
| AWGN | 4.9e-1 | 9.5e-2 | 2.4e-5 | 0 | 0 |
| Phase noise | 4.9e-1 | 1.4e-1 | 5.4e-5 | 0 | 0 |
| Multipath (2-tap) | 4.9e-1 | 2.2e-1 | 1.7e-3 | 0 | 0 |
| Combined (worst) | 4.9e-1 | 2.4e-1 | 4.6e-3 | 9.8e-6 | 0 |

"no sync" = sync_detect failed to acquire (expected below ~5 dB SNR for Schmidl-Cox).

**Recommended operating points:**
- QPSK:   SNR ≥ 15 dB (AWGN/phase clean), ≥ 20 dB (multipath/combined clean)
- 16-QAM: SNR ≥ 20 dB (all channels clean)

**SNR penalty vs theory:** ~4–7 dB. Sources: ap_fixed<16,1> quantisation (~2–3 dB),
preamble-only channel estimation (~1–2 dB), sync jitter at mid-SNR (~0.5 dB).

### Co-simulation (RTL validation)

| Test | Result |
|------|--------|
| TX C-sim (16-QAM, 255 symbols) | PASS — EVM 0.056% |
| FEC encode+decode loopback | PASS — BER=0 clean, corrects 5% coded BER |
| RX C-sim (full chain, BER=0) | PASS |
| RX RTL co-simulation (ofdm_rx) | PASS — BER=0, C/RTL match confirmed |

---

## Quick Start

### Prerequisites
- Vitis HLS 2025.2 (set `XILINX_ROOT` in `setup_vitis.sh`)
- Python 3.8+ with numpy, scipy, matplotlib
- Ubuntu 22.04 / WSL2 (lib_compat symlinks included for libncurses.so.5)

### Run TX C-sim and EVM check

```bash
python3 sim/ofdm_reference.py --gen --mod 1   # generate input bits + Python reference
./setup_vitis.sh csim 1                       # run HLS TX C-sim (16QAM)
python3 sim/ofdm_reference.py --compare       # EVM comparison
```

### Run RX C-sim loopback

```bash
./run_loopback.sh                         # TX csim → RX csim → BER report
```

### BER sweep

```bash
./run_ber_sweep.sh --mod 0                # QPSK sweep, all channels
./run_ber_sweep.sh --mod 1                # 16QAM sweep, all channels
python3 sim/plot_ber.py ber_results_mod0.csv  # generate waterfall plot
```

### Synthesis (individual blocks)

```bash
./setup_vitis.sh synth             # ofdm_tx
./setup_vitis.sh rx_synth          # ofdm_rx
./setup_vitis.sh viterbi_synth     # viterbi_dec
./setup_vitis.sh sync_detect_synth # sync_detect
./setup_vitis.sh cfo_correct_synth # cfo_correct
```

---

## Hardware Path (planned)

```
AD9364 LVDS  →  ADI axi_ad9364 IP  →  AXI-Stream  →  HLS RX chain
HLS TX chain →  AXI-Stream          →  ADI axi_ad9364 IP  →  AD9364 LVDS
```

The HLS blocks use `hls::stream<iq_t>` with `#pragma HLS INTERFACE axis` — they synthesize
directly to AXI-Stream ports for Vivado IP Integrator. Only a 12-bit → ap_fixed<16,1> width
adapter is needed at the AD9364 boundary.

ADI HDL reference: https://github.com/analogdevicesinc/hdl

---

## Optimization Status

| Block | v1 LUT | v2 LUT | Saving | Notes |
|-------|--------|--------|--------|-------|
| sync_detect | 12,972 | 2,895 | −78% | Integer cross-multiply metric; pipelined CORDIC |
| viterbi_dec | 13,388 | 9,338 | −30% | PIPELINE+UNROLL factor=16; counter-based circular buffer; WIN_LEN=128 |
| cfo_correct | 2,636 | — | pending | 27 DSP target → 4 DSP (fixed-point sincos LUT) |
| ofdm_tx/rx FFT | ~9,000 inflated | — | pending | Replace HLS placeholder with xfft v9 IP in Vivado |

See `docs/files/OPTIMIZATION_GUIDE.md` for detailed analysis.

---

## License

MIT
