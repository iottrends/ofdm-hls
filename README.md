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

RX:  RF → AD9364 → sync_detect → ofdm_rx
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
| `sync_detect.cpp / .h` | Free-running preamble gate + inline CFO estimation/correction (CP-based) |
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

## Resource Usage — Post-Implementation (Vivado 2025.2, XC7A50T, 100 MHz)

Latest LiteX SoC build (full PCIe + AD9361 wrapper + OFDM PHY chain), routed and
bitstream-generated. Numbers from
`litex/build/hallycon_m2sdr_platform/gateware/hallycon_m2sdr_platform_utilization_place.rpt`.

| Resource     | Used   | Available | Utilization |
|--------------|--------|-----------|-------------|
| Slice LUTs   | 25,913 | 32,600    | **79.5%**   |
| LUT as Logic | 23,549 | 32,600    | 72.2%       |
| LUT as Memory| 2,364  | 9,600     | 24.6%       |
| Slice FF     | 32,450 | 65,200    | **49.8%**   |
| DSP48E1      | 81     | 120       | **67.5%**   |
| Block RAM    | 67     | 75        | **89.3%**   |
| Bonded IOB   | 57     | 150       | 38.0%       |

**Timing**: Setup WNS = +0.210 ns, Hold WNS = +0.111 ns, all paths positive
slack at 100 MHz. Routing 100% successful, bitstream OK.

BRAM is the tightest resource (89.3%). The 4096-sample circular buffer in
`sync_detect` accounts for 4 RAMB36 of that — could be trimmed to 2048 if
another change pressures BRAM.

For the per-functional-block resource breakdown see [`docs/RESOURCES.md`](docs/RESOURCES.md).

## Throughput (20 MSPS sample rate, 255 data symbols/frame)

PHY-layer gross throughput = `200 SCs × bps × code_rate × 69,444 sym/sec × 99.2%`
(99.2% is the data efficiency from 255/(255+2) preamble+header overhead).
Net throughput is PHY minus 10–15% MAC + framing overhead.

| Modcod         | PHY (gross) | Net (after MAC) |
|----------------|-------------|-----------------|
| QPSK   r=1/2   | 13.8 Mbps   | 11.7–12.4 Mbps  |
| QPSK   r=2/3   | 18.4 Mbps   | 15.6–16.5 Mbps  |
| 16-QAM r=1/2   | 27.6 Mbps   | 23.4–24.8 Mbps  |
| **16-QAM r=2/3** | **36.7 Mbps** | **31–33 Mbps** |

Frame time at 255 symbols + preamble + header = 257 × 14.4 µs = **3.7 ms**.
Half-duplex round-trip ≥ 7.4 ms.

---

## Validated Results

### EVM (TX accuracy)

HLS TX vs NumPy float reference (74,016 samples, 16-QAM, 255 symbols):

```
EVM ≈ 0.054–0.062%   — ap_fixed<16,1> quantisation noise floor
PREAMBLE  0.054%
HEADER    0.128%
DATA[*]   0.054% (median, range 0.05–0.06% across 255 symbols)
```

### BER vs SNR — HLS RX vs Python Q15 reference (post `rx-dsp-opt` fixes)

After the AP_SAT overflow fix and sync_detect inline-CFO removal landed
(commit `d08a537`), HLS RX tracks the Python Q15 reference within ±5%
relative BER at every SNR / modcod combination.

**16-QAM, rate-1/2** — AWGN, single-frame:

| SNR  | HLS BER | Python Q15 BER |
|------|---------|----------------|
| 20   | 0       | 0              |
| 17   | 0       | 0              |
| 16   | 0       | 0 (was 8 errors before fix) |
| 15   | **0**   | 0 (was hdr CRC FAIL before fix) |
| 13   | 0.07%   | 0.07%          |
| 12   | 0.30%   | 0.31%          |
| 11   | 1.5%    | 1.5%           |
| 10   | 6.1%    | 6.2%           |

**16-QAM rate-2/3** is ~2 dB more sensitive than rate-1/2; **QPSK r=1/2 and
r=2/3** stay at BER=0 down to 10 dB. See `docs/RX_LOW_SNR_DEBUG.md` for the
full debug trace and `tests/test.md` for the regression matrix (multipath +
phase noise + CFO).

**Header CRC** passes at every test point — the BPSK header now decodes
cleanly at any SNR ≥ 10 dB regardless of data BER.

### Python RX chain — Path-A refinements (default ON)

The Python reference RX (`sim/ofdm_reference.py`) has these improvements
baked in for Path-A operation; all default to ON, with `--no-*` opt-out
flags for backward-compatibility testing against the HLS chain.

| Refinement | Win |
|---|---|
| `sync_detect_reference()` — Q15 + FP64 mirror of `src/sync_detect.cpp` v5 | preamble timing, no fixed-offset assumption |
| DFT-based MMSE channel smoothing (`L_taps=64` default) | recovers ~3 dB of the single-preamble channel-est noise penalty |
| Soft-decision Viterbi (`viterbi_decode_soft`) | ~2 dB cliff improvement vs hard Hamming branch metric |
| LLR clipping (median-based outlier reject) | tames cliff-edge metric outliers; +0.3 dB |
| Pilot-magnitude weighted CPE (`|G[k]|²` weighted sum) | +0.3–0.5 dB on multipath |
| Header FEC (rate-1/2 K=7 conv, 64 BPSK SCs at 26-bit input) | +5 dB header-CRC margin; scales to ~70-bit MAC payload at rate-1/2 |
| **CFO estimator + time-domain derotator** (Schmidl-Cox angle from preamble correlator) | **fully recovers cliff at CFO=0.13 SC injection** — zero penalty for realistic ±2 ppm TCXO / Ku-band Doppler |

### CFO correction — Task 1 verification (5-frame averaged, combined channel, CFO=0.13 SC)

| SNR | 16QAM 1/2 (no CFO correct) | 16QAM 1/2 (with CFO correct) | 16QAM 2/3 (no CFO correct) | 16QAM 2/3 (with CFO correct) |
|---|---|---|---|---|
| 10 | 3.87e-2 | **1.01e-3** | 3.21e-1 (chance) | **4.83e-2** |
| 12 | 5.83e-3 | **5.88e-6** | 1.64e-1 | **2.81e-3** |
| 14 | 1.37e-3 | **0** | 5.50e-2 | **7.34e-5** |
| 16 | 1.84e-4 | **0** | 2.52e-2 | **4.41e-6** (clean) |

Cliff penalty from CFO=0.13 collapses from +4–6 dB → **0 dB** with the
preamble Schmidl-Cox CFO estimator and time-domain derotator in place.

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
./tests/run_loopback.sh                       # TX csim → RX csim → BER report
./tests/run_loopback_all.sh                   # all 4 modcods
./tests/run_loopback_noisy.sh --snr 15        # AWGN at 15 dB
```

### Full TX → channel → RX regression

```bash
./tests/run_regression.sh --snr 15            # 4 modcods × 5 channels at SNR=15
./tests/run_regression.sh --snr "5 10 15 20" --soft --smooth   # full diagnostic
```

See [`tests/test.md`](tests/test.md) for the complete test catalog.

### Synthesis (individual blocks)

```bash
./setup_vitis.sh synth             # ofdm_tx
./setup_vitis.sh rx_synth          # ofdm_rx
./setup_vitis.sh viterbi_synth     # viterbi_dec
./setup_vitis.sh sync_detect_synth # sync_detect (free-running gate + inline CFO)
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

| Block | Before | After | Saving | Notes |
|-------|--------|-------|--------|-------|
| sync_detect | 12,972 LUT | 2,895 LUT | −78% | Integer cross-multiply metric; pipelined CORDIC; later absorbed cfo_correct (fixed-point sincos LUT, CORDIC phase accumulator) inline — see sync_detect.cpp |
| viterbi_dec | 13,388 LUT | 9,448 LUT | −30% | PIPELINE+UNROLL factor=16; circular buffer; WIN_LEN=128 |
| ofdm_tx/rx FFT | ~20,000 LUT (hls::fft inline) | ~3,600 LUT (xfft v9.1 IP ×2) | −82% | Replaced with Xilinx xfft v9.1 AXI-Stream IP in Vivado BD |
| **Full chain integrated** | **~49,000 LUT (150% — not routable)** | **15,395 LUT (47%)** | **−69%** | Post-implementation, Vivado 2025.2 |

See `docs/OFDM_HLS_ANALYSIS.md` for detailed analysis and hardware bring-up checklist.

---

## License

MIT
