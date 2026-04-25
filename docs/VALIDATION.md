# Validation Guide

How to run the full validation suite for the OFDM TX+RX HLS chain.

For the planned **regression matrix** (4 modcods × 5 frame sizes ×
5 channels × 5 SNRs), see [`REGRESSION_PLAN.md`](REGRESSION_PLAN.md).
This document covers the day-to-day spot checks.

---

## Validation Levels

```
Level 1 — Quick clean loopback (1 modcod, 1 SNR)              ~3 min
Level 2 — All-modcod clean loopback                            ~10 min
Level 3 — Noisy loopback with Python Q15 cross-check           ~5 min/point
Level 4 — RTL co-simulation (per IP)                           ~20–60 min
Level 5 — Full BER sweep (channel models)                      ~30 min – 2 hrs
```

---

## Level 1 — Quick clean loopback

5-step end-to-end test on AWGN-clean signal — fastest sanity check.

```bash
./run_loopback.sh                          # default: 16-QAM rate-1/2
./run_loopback.sh --mod 0 --rate 0         # QPSK rate-1/2
./run_loopback.sh --mod 1 --rate 1         # 16-QAM rate-2/3
```

Steps the script runs:
1. `ofdm_reference.py --gen` — random input bits + Python float reference
2. `setup_vitis.sh csim` — HLS TX C-sim → `tb_tx_output_hls.txt`
3. `ofdm_reference.py --compare` — EVM check (HLS vs Python TX)
4. `setup_vitis.sh rx_csim` — HLS RX C-sim → BER vs original bits
5. `ofdm_reference.py --decode-hls` — Python ref decoder on HLS TX output

Expected: all 5 steps PASS, EVM ≈ 0.054% (Q15 quantisation floor),
header CRC OK, RX BER = 0.

## Level 2 — All-modcod clean loopback

```bash
./run_loopback_all.sh
```

Sweeps all 4 modcods (`{QPSK, 16-QAM} × {r=1/2, r=2/3}`) on clean signal.
Expected: 4/4 PASS with `EVM ≈ 0.05%` and `BER = 0` for each.

## Level 3 — Noisy loopback with Python Q15 cross-check

Single-SNR test that exercises HLS RX against AWGN and compares with the
Python golden Q15-precision reference:

```bash
./run_loopback_noisy.sh --mod 1 --rate 0 --snr 15
```

Step 5 of this script invokes **two** Python decoders side-by-side:
- `decode_full(use_q15=False)` — float64 (idealised reference)
- `decode_full(use_q15=True)`  — HLS-equivalent Q15/Q20/Q22 precision

The Q15 decoder is the apples-to-apples HLS twin; HLS BER should track
its BER within ±5%. If they diverge, that's an HLS implementation bug
(not an algorithm cliff).

To sweep SNRs:

```bash
for snr in 20 17 15 12 10; do
    echo "##### SNR=$snr dB #####"
    ./run_loopback_noisy.sh --mod 1 --rate 0 --snr $snr 2>&1 | \
        grep -E "HLS RX|FP64|Q15 |RESULT"
done
```

Expected (16-QAM r=1/2):

| SNR  | HLS BER | Python Q15 BER | Header |
|------|---------|----------------|--------|
| 20   | 0       | 0              | PASS   |
| 17   | 0       | 0              | PASS   |
| 15   | 0       | 0              | PASS   |
| 12   | ≈0.30%  | ≈0.31%         | PASS   |
| 10   | ≈6.1%   | ≈6.2%          | PASS   |

## Level 4 — FEC standalone test

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

## Level 5 — Diagnostic helpers (when something fails)

These are not part of routine CI — use when debugging a specific issue.

**Bypass sync_detect** — feed pre-aligned IQ directly to ofdm_rx
(isolates sync-detect bugs from ofdm_rx bugs):

```bash
OFDM_RX_BYPASS_SYNC=1 ./run_loopback_noisy.sh --mod 1 --rate 0 --snr 15
```

**Python BPSK header decoder only** (Q15-class, mirrors HLS exactly):

```bash
python3 sim/ofdm_reference.py --decode-header-q15 \
    --input tb_tx_output_hls_noise.txt --mod 1 --rate 0
```

**HLS-side per-pilot diagnostic dumps** appear in `vitis_rx_csim.log` /
`vitis_rx_noisy_csim.log`, prefixed `[HLS]`:

```
[HLS] phase_err = +0.384 deg
[HLS] freq_buf[pilot 50,75,100,154,179,204]: ...
[HLS] G_eq[pilot 50,75,100,154,179,204]: ...
[HLS] hdr eq.real (26 bits): ...
```

These can be diff'd against the Python `decode_header_q15` output to
pin down precision divergence — used to find the AP_SAT bug in commit
`d08a537` (see `RX_LOW_SNR_DEBUG.md`).

---

## Level 4 — RTL Co-simulation (per IP)

Proves: synthesized RTL of an HLS IP matches its C-sim output sample-for-sample.

```bash
./setup_vitis.sh tx_cosim 1     # ofdm_tx, 16-QAM, ~20 min
./setup_vitis.sh rx_cosim 1     # ofdm_rx, 16-QAM, ~60 min
```

Expected:
```
INFO: [COSIM 212-1000] *** C/RTL co-simulation finished: PASS ***
```

If C-sim passes but co-sim fails, the failure is in HLS-to-RTL
synthesis (e.g., overflow modes, AXIS handshake races). Currently
co-sim is run on demand, not in CI — see `REGRESSION_PLAN.md` Step 6
for the proposed cosim subset.

## Level 5 — BER sweep (legacy script)

```bash
./run_ber_sweep.sh --mod 0     # QPSK, all channels, 10 frames/point
./run_ber_sweep.sh --mod 1     # 16-QAM, all channels, 10 frames/point
python3 sim/plot_ber.py ber_results_mod0.csv
```

The sweep covers AWGN, phase-noise, multipath, and combined channels.
Output is a CSV per modcod plus a waterfall plot. **For modcod-by-
SNR matrices with the Q15 cross-check, use `run_loopback_noisy.sh`
in a loop instead** (see Level 3) — that path produces the apples-to-
apples HLS-vs-Python comparison.

---

## Current Validation Status

| Test | Result | Notes |
|------|--------|-------|
| TX C-sim EVM (4 modcods) | PASS — EVM 0.054–0.062% | Q15 quantisation floor |
| FEC csim (rate 1/2 + 2/3) | PASS — BER=0 clean, corrects 5% noisy | |
| RX C-sim clean (4 modcods) | PASS — BER=0 | `run_loopback_all.sh` |
| RX C-sim AWGN 16-QAM r=1/2 | PASS — tracks Python Q15 within ±5% BER from 10 dB up | post commit `d08a537` |
| RX C-sim AWGN, all 4 modcods, 10–13 dB | PASS at expected algorithm cliff | |
| Header CRC at 10 dB AWGN | PASS at every modcod | post AP_SAT fix |
| RX RTL co-sim | LAST GREEN: 2026-04-10 (pre `rx-dsp-opt`); needs re-run after `d08a537` | |
| Variable frame size (4–32 syms) | ⏳ pending | see REGRESSION_PLAN.md |
| Multipath / phase-noise / CFO regression | ⏳ pending | channel sim has hooks; loopback scripts don't yet |
