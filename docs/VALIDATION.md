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
python3 sim/ofdm_reference.py --gen --mod 1    # 16-QAM (default)
# or
python3 sim/ofdm_reference.py --gen --mod 0    # QPSK
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
python3 sim/ofdm_reference.py --compare
```

Expected result:
```
EVM = 0.056%  (-65 dB)   PASS
PREAMBLE  EVM = 0.056%
HEADER    EVM = 0.132%
DATA[*]   EVM = 0.052–0.062%  (all 255 symbols)
```

EVM ~0.056% is the quantisation noise floor of ap_fixed<16,1> (Q0.15).

### Step 4 — RX C-sim loopback

```bash
./setup_vitis.sh rx_csim
```

Expected:
```
[TB] Timing locked.  CFO estimate = 0 subcarrier spacings
[TB] Byte errors : 0 / 12750
[TB] Bit  errors : 0 / 102000
[TB] PASS  — BER = 0  (perfect decode)
```

### Step 5 — FEC standalone test

```bash
./setup_vitis.sh fec_csim
```

Expected:
```
Test 1: Rate 1/2 clean   [PASS]
Test 2: Rate 2/3 clean   [PASS]
Test 3: Rate 1/2 noisy   [PASS] Viterbi corrected most errors
Test 4: Rate 2/3 noisy   [PASS] Viterbi corrected most errors
RESULT: ALL PASS
```

---

## Level 2 — TX RTL Co-simulation

Proves: synthesized RTL of `ofdm_tx` matches C-sim output sample-for-sample.

```bash
./setup_vitis.sh tx_cosim 1     # 16-QAM (takes ~20 min)
```

Expected:
```
[TB] PASS: sample count correct
INFO: [COSIM 212-1000] *** C/RTL co-simulation finished: PASS ***
```

---

## Level 3 — RX RTL Co-simulation

Proves: synthesized RTL of `ofdm_rx` decodes correctly, BER=0 on clean channel.

```bash
./setup_vitis.sh rx_cosim 1     # 16-QAM (takes ~60 min)
```

Expected:
```
[TB] Byte errors : 0 / 12750
[TB] Bit  errors : 0 / 102000
INFO: [COSIM 212-1000] *** C/RTL co-simulation finished: PASS ***
```

---

## Level 4 — BER Sweep

Proves: full chain performance across AWGN, phase noise, multipath, and combined channels.

```bash
./run_ber_sweep.sh --mod 0     # QPSK, all channels, 10 frames/point
./run_ber_sweep.sh --mod 1     # 16-QAM, all channels, 10 frames/point
python3 sim/plot_ber.py ber_results_mod0.csv
python3 sim/plot_ber.py ber_results_mod1.csv
```

### Reference Results (2026-04-10, sync_detect v2 + viterbi_dec v2)

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

"no sync" = sync_detect failed to acquire at ≤5 dB SNR — expected for Schmidl-Cox.

**Operating points:**
- QPSK: clean at 15 dB (AWGN/phase), 20 dB (multipath/combined)
- 16-QAM: clean at 20 dB all channels, 25 dB combined worst-case

---

## Current Validation Status

| Test | Result | Date |
|------|--------|------|
| TX C-sim EVM (16-QAM) | PASS — 0.056% EVM | 2026-04-09 |
| FEC csim (viterbi_dec v2) | ALL PASS — BER=0 clean, corrects 5% noisy | 2026-04-09 |
| RX C-sim (full chain, post v2) | PASS — BER=0 | 2026-04-10 |
| RX RTL co-sim (ofdm_rx) | PASS — BER=0, C/RTL match | 2026-04-10 |
| BER sweep QPSK | PASS — BER=0 at ≥20 dB all channels | 2026-04-10 |
| BER sweep 16-QAM | PASS — BER=0 at ≥20 dB (≥25 dB combined) | 2026-04-10 |
