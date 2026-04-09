# Validation Guide

How to run the full validation suite for the OFDM TX+RX HLS chain.

---

## Validation Levels

```
Level 1 — Algorithm (C-sim + Python cross-check)   ~5 min
Level 2 — TX RTL co-simulation                     ~20 min
Level 3 — RX RTL co-simulation                     ~60 min
Level 4 — BER sweep (channel simulation)           ~30 min (quick) / 2+ hrs (full)
```

Run them in order. Each level builds on the previous.

---

## Level 1 — Algorithm Validation

Proves: C-sim algorithm is correct; HLS fixed-point matches floating-point reference.

### Step 1 — Generate input bits + Python TX reference

```bash
python3 ofdm_reference.py --gen --mod 1    # 16-QAM (default)
# or
python3 ofdm_reference.py --gen --mod 0    # QPSK
```

Creates:
- `tb_input_to_tx.bin`       — raw input bytes (ground truth for BER check)
- `tb_tx_output_ref.txt`     — floating-point Python TX reference IQ

### Step 2 — Run HLS TX C-simulation

```bash
./setup_vitis.sh csim 1        # 16-QAM
# or
./setup_vitis.sh csim 0        # QPSK
```

Creates `tb_tx_output_hls.txt` — HLS C-sim TX IQ output.

### Step 3 — EVM comparison (TX accuracy check)

```bash
python3 ofdm_reference.py --compare
```

Expected result:
```
EVM = 0.056%  (-65 dB)   PASS
PREAMBLE  EVM = 0.056%
HEADER    EVM = 0.132%
DATA[*]   EVM = 0.052–0.062%  (all 255 symbols)
```

EVM ~0.056% is the quantisation noise floor of ap_fixed<16,1> (Q0.15).
Any EVM above ~1% indicates an algorithmic error.

### Step 4 — Run HLS RX C-simulation (full TX→RX loopback)

```bash
./setup_vitis.sh rx_csim
```

Expected result:
```
[TB] PASS  — BER = 0  (perfect decode)
```

### Step 5 — Python RX cross-check

```bash
python3 ofdm_reference.py --decode-hls --mod 1
```

Independent NumPy receiver decoding the HLS TX output.

Expected result:
```
[DEC] Bit errors : 0 / 102000
[DEC] PASS  — BER = 0
```

---

## Level 2 — TX RTL Co-simulation

Proves: synthesized Verilog ofdm_tx RTL produces bit-identical output to C-sim.

### Prerequisites

- Level 1 complete (tb_input_to_tx.bin must exist)
- Vitis HLS 2025.2

### Run TX cosim

```bash
./setup_vitis.sh tx_cosim 1    # 16-QAM (~20 min)
```

Expected result in log:
```
INFO: [COSIM 212-1000] *** C/RTL co-simulation finished: PASS ***
```

### ⚠️ After cosim: regenerate tb_tx_output_hls.txt

The cosim post-check phase overwrites `tb_tx_output_hls.txt` with test-vector
replay output, which has minor format artefacts (~27% EVM on some symbols).
This does NOT indicate an RTL bug — the PASS verdict is based on direct
C-sim vs RTL test vector comparison, not on the replayed file.

Always regenerate before running EVM check:

```bash
./setup_vitis.sh csim 1
python3 ofdm_reference.py --compare    # should still read EVM = 0.056%
```

---

## Level 3 — RX RTL Co-simulation

Proves: synthesized Verilog ofdm_rx RTL produces bit-identical output to C-sim.

### Prerequisites

- Level 1 complete
- `tb_tx_output_hls.txt` must exist (run `./setup_vitis.sh csim 1` if needed)

### Run RX cosim

```bash
./setup_vitis.sh rx_cosim 1    # 16-QAM (~60 min)
```

Expected result:
```
INFO: [COSIM 212-1000] *** C/RTL co-simulation finished: PASS ***
```

### After RX cosim: regenerate clean TX output

```bash
./setup_vitis.sh csim 1
```

---

## Level 4 — BER Sweep

Proves: full chain (TX → channel → RX) meets BER requirements across
SNR points and channel conditions.

### Quick sweep (~5 min, 3 frames)

```bash
./run_ber_sweep.sh --mod 0 --quick    # QPSK
./run_ber_sweep.sh --mod 1 --quick    # 16-QAM
```

### Full sweep (~30 min per modulation, 10 frames)

```bash
./run_ber_sweep.sh --mod 0            # QPSK  → ber_results_mod0.csv
./run_ber_sweep.sh --mod 1            # 16QAM → ber_results_mod1.csv
```

### Plot results

```bash
python3 plot_ber.py ber_results_mod0.csv    # → ber_results_mod0.png
python3 plot_ber.py ber_results_mod1.csv    # → ber_results_mod1.png
```

### Expected results

| Channel | QPSK floor | 16-QAM floor |
|---------|-----------|--------------|
| AWGN | ≥10 dB | ≥20 dB |
| Phase noise (σ=0.005) | ≥10 dB | ≥20 dB |
| Multipath (2-tap UAV) | ≥15 dB | ≥20 dB |
| Combined (worst case) | ≥20 dB | ≥25 dB |

---

## Validation Status Summary

| Test | Command | Expected |
|------|---------|----------|
| TX EVM | `ofdm_reference.py --compare` | 0.056% |
| TX→RX loopback BER | `setup_vitis.sh rx_csim` | BER=0 |
| Python RX decode | `ofdm_reference.py --decode-hls` | BER=0 |
| TX RTL cosim | `setup_vitis.sh tx_cosim 1` | PASS |
| RX RTL cosim | `setup_vitis.sh rx_cosim 1` | PASS |
| QPSK BER sweep | `run_ber_sweep.sh --mod 0` | BER=0 at ≥20 dB |
| 16-QAM BER sweep | `run_ber_sweep.sh --mod 1` | BER=0 at ≥25 dB |

---

## Known Artefacts

### Cosim post-check EVM artefact

After running `tx_cosim` or `rx_cosim`, the file `tb_tx_output_hls.txt` is
overwritten by the cosim post-check with test-vector replay output. This shows
~27% EVM on ~18% of data symbols when compared against the Python reference.

**This is not an RTL bug.** The cosim PASS verdict is based on direct C-sim vs
RTL comparison using internal test vectors, not the written file. The artefact
is caused by the binary fixed-point test vector format round-trip.

Fix: always run `./setup_vitis.sh csim 1` after cosim to regenerate a clean file.

### 0 dB SNR sync failure

At 0 dB SNR, `sync_detect` may fail to lock (metric too flat in heavy noise).
This shows as "FAIL (no output)" in the BER sweep. This is expected behaviour —
not a bug. The sync threshold is set correctly; the signal is simply too noisy
to acquire.
