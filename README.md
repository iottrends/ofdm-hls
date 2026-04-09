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

### HLS Source (synthesizable)

| File | Description |
|------|-------------|
| `ofdm_tx.cpp / .h` | OFDM modulator — IFFT, CP insert, pilot insert |
| `ofdm_rx.cpp / .h` | OFDM demodulator — FFT, ZC channel estimation, equalizer |
| `sync_detect.cpp / .h` | Schmidl-Cox timing sync + CFO estimator |
| `cfo_correct.cpp / .h` | Carrier frequency offset correction (phase rotation) |
| `conv_enc.cpp` | Convolutional encoder, rate 1/2 and 2/3 (K=7) |
| `viterbi_dec.cpp` | Viterbi decoder (hard decision) |
| `conv_fec.h` | Shared FEC types and constants |
| `scrambler.cpp / .h` | 802.11a 7-stage LFSR scrambler/descrambler |
| `interleaver.cpp / .h` | 802.11a two-step bit interleaver/deinterleaver |

### Testbenches

| File | Description |
|------|-------------|
| `ofdm_tx_tb.cpp` | TX C-sim testbench — reads raw bytes, writes IQ output |
| `ofdm_rx_tb.cpp` | RX C-sim testbench — reads IQ, decodes, reports BER |
| `conv_fec_tb.cpp` | FEC encode+decode loopback test |

### Python Reference & Simulation

| File | Description |
|------|-------------|
| `ofdm_reference.py` | Full floating-point TX+RX reference model + EVM comparison |
| `fec_reference.py` | Python convolutional encoder and Viterbi decoder |
| `ofdm_channel_sim.py` | Channel simulator: AWGN, phase noise, multipath, CFO |
| `plot_ber.py` | BER vs SNR waterfall plot generator |
| `gen_zc_lut.py` | ZC sequence LUT generator |

### Build & Test Scripts

| File | Description |
|------|-------------|
| `setup_vitis.sh` | Vitis HLS 2025.2 environment + all TCL flows (WSL) |
| `run_ber_sweep.sh` | Automated BER sweep across channels and SNR points |
| `run_loopback.sh` | Quick TX→RX loopback test (clean channel) |
| `run_loopback_noisy.sh` | TX→channel→RX loopback with noise |

---

## Validated Results

### EVM (TX accuracy)

HLS TX output compared against NumPy floating-point reference (74016 samples, 16-QAM, 255 symbols):

```
EVM = 0.056%  (-65 dB)   — pure ap_fixed<16,1> quantisation noise floor
PREAMBLE  EVM = 0.056%
HEADER    EVM = 0.132%
DATA[*]   EVM = 0.052–0.062%  (uniform across all 255 symbols)
```

### BER Sweep (RX performance)

```
Modulation: QPSK (--mod 0)

Channel           | 10 dB     | 15 dB     | 20 dB     | 25 dB
------------------+-----------+-----------+-----------+----------
AWGN              | BER=0     | BER=0     | BER=0     | BER=0
Phase noise       | BER=0     | BER=0     | BER=0     | BER=0
Multipath (2-tap) | 5.1e-3    | BER=0     | BER=0     | BER=0
Combined (worst)  | 2.1e-3    | 1.8e-5    | BER=0     | BER=0


Modulation: 16-QAM (--mod 1)

Channel           | 10 dB     | 15 dB     | 20 dB     | 25 dB
------------------+-----------+-----------+-----------+----------
AWGN              | 9.2e-2    | 3.5e-5    | BER=0     | BER=0
Phase noise       | 1.1e-1    | 3.2e-5    | BER=0     | BER=0
Multipath (2-tap) | 2.0e-1    | 9.2e-4    | BER=0     | BER=0
Combined (worst)  | 2.2e-1    | 1.4e-3    | 2.9e-6    | BER=0
```

**Recommended operating points:**
- 16-QAM: SNR ≥ 25 dB (all channels clean)
- QPSK:   SNR ≥ 20 dB (all channels clean), usable down to ~12 dB on LoS

---

## Quick Start

### Prerequisites
- Vitis HLS 2025.2 (set `XILINX_ROOT` in `setup_vitis.sh`)
- Python 3.8+ with numpy, scipy, matplotlib
- Ubuntu 22.04 / WSL2 (lib_compat symlinks included for libncurses.so.5)

### Run TX C-sim and EVM check

```bash
python3 ofdm_reference.py --gen --mod 1   # generate input bits + Python reference
./setup_vitis.sh csim 1                   # run HLS TX C-sim (16QAM)
python3 ofdm_reference.py --compare       # EVM comparison
```

### Run RX C-sim loopback

```bash
./run_loopback.sh                         # TX csim → RX csim → BER report
```

### BER sweep

```bash
./run_ber_sweep.sh --mod 0                # QPSK sweep, all channels
./run_ber_sweep.sh --mod 1                # 16QAM sweep, all channels
./run_ber_sweep.sh --quick --mod 0        # quick 3-frame subset
python3 plot_ber.py ber_results_mod0.csv  # generate waterfall plot
```

---

## Hardware Path (planned)

```
AD9364 LVDS  →  ADI axi_ad9364 IP  →  AXI-Stream  →  HLS RX chain
HLS TX chain →  AXI-Stream          →  ADI axi_ad9364 IP  →  AD9364 LVDS
```

The HLS blocks use `hls::stream<iq_t>` with `#pragma HLS INTERFACE axis` — they synthesize directly to AXI-Stream ports for connection in Vivado IP Integrator. Only a 12-bit → ap_fixed<16,1> sample width adapter is needed at the AD9364 boundary.

ADI HDL reference: https://github.com/analogdevicesinc/hdl

---

## License

MIT
