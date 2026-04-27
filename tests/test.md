# OFDM Test Catalog

All test/regression scripts live under `tests/`. Run them from the **repo root**:

```bash
./tests/<script>.sh [options]
```

Each script `cd`s to the repo root internally, so build artifacts (clean TX
IQ, noisy IQ, decoded bytes, Vitis logs) end up in the usual locations.

---

## Script overview

| Script | Purpose | Modcod loop | Channel | RX path |
|---|---|---|---|---|
| [`run_regression.sh`](#run_regressionsh-full-tx--channel--rx-regression) | Full TX → channel → RX regression — primary entry point | yes (4) | awgn / multipath / phase / cfo / combined | Python FP64 + Q15 (hard by default; optional soft; optional HLS) |
| [`run_loopback.sh`](#run_loopbacksh-clean-loopback-single-modcod) | TX → RX clean loopback for one modcod | no | none | HLS C-sim |
| [`run_loopback_all.sh`](#run_loopback_allsh-clean-loopback-all-modcods) | Wrapper: runs `run_loopback.sh` over all 4 modcods | yes (4) | none | HLS C-sim |
| [`run_loopback_noisy.sh`](#run_loopback_noisysh-noisy-loopback-single-modcod) | TX → AWGN → RX for one modcod at one SNR | no | AWGN only | HLS RX + Python ref cross-check |
| [`run_ber_sweep.sh`](#run_ber_sweepsh-hls-rx-only-sweep) | HLS RX BER sweep across modcods × SNR × channel | yes | configurable | HLS only |

Per-IP HLS unit testbenches (`tb/conv_fec_tb.cpp`, `tb/ofdm_tx_tb.cpp`,
`tb/ofdm_rx_tb.cpp`, `tb/sync_detect_tb.cpp`) are invoked through
`./setup_vitis.sh` (`fec_csim`, `csim`, `rx_csim`, etc.) — those are
per-block, not full chain.

---

## `run_regression.sh` — full TX → channel → RX regression

Primary regression driver. For each `(modcod × channel × SNR × frame)` point
it (1) runs HLS TX C-sim once per modcod, (2) applies the requested channel,
(3) decodes through every selected Python RX path (and optionally HLS RX
C-sim), (4) records header CRC + data BER per decoder.

### Defaults

| Knob | Default |
|---|---|
| `--modcods`   | `0:0 0:1 1:0 1:1` (all 4: QPSK 1/2, QPSK 2/3, 16QAM 1/2, 16QAM 2/3) |
| `--channels`  | `awgn multipath phase cfo combined` |
| `--snr`       | `15` (single point) |
| `--frames`    | `1` |
| `--phase-sigma` | `0.005` rad/sample (Wiener walk) |
| `--cfo-sc`    | `0.3` subcarrier spacings |
| Decoders run  | `FP64-hard`, `Q15-hard` |
| `--soft`      | off (adds `FP64-soft`, `Q15-soft` when present) |
| `--smooth`    | off (when present, swaps each enabled decoder for its `*-smooth` variant — DFT-based MMSE channel smoothing) |
| `--smooth-taps N` | `64` (number of time-domain taps to keep — sweet spot 64–128) |
| `--hls-rx`    | off (adds HLS `rx_noisy_csim` BER when present) |

### Usage examples

```bash
./tests/run_regression.sh                                 # defaults: 4 mod × 5 ch × SNR=15 × 1 frame = 20 points
./tests/run_regression.sh --snr 10                        # single SNR
./tests/run_regression.sh --snr "5 10 15 20"              # SNR list
./tests/run_regression.sh --channels "awgn multipath"     # subset of channels
./tests/run_regression.sh --modcods "0:0 1:1"             # subset of modcods
./tests/run_regression.sh --frames 5                      # average over 5 frames per point
./tests/run_regression.sh --soft                          # add soft-Viterbi variants
./tests/run_regression.sh --smooth                        # enable DFT channel smoothing
./tests/run_regression.sh --soft --smooth                 # both — strongest Python RX
./tests/run_regression.sh --smooth --smooth-taps 96       # tune the smoothing window
./tests/run_regression.sh --hls-rx                        # add HLS RX C-sim per point
./tests/run_regression.sh --snr 10 --soft --smooth --hls-rx --frames 3   # full diagnostic mode
```

### Standard regression workflows (copy-paste ready)

These are the canonical sweeps used to baseline + measure improvements.
Run from the repo root.  All produce per-frame raw + aggregated mean+stddev CSVs.

#### 1. Quick baseline (single-frame, ~12 min)

```bash
./tests/run_regression.sh --modcods "0:0 0:1" --snr "5 6 7 8 9 10" \
    --channels combined --cfo-sc 0 --soft --smooth \
 && mv regression_results.csv regression_qpsk_soft_smooth.csv \
 && mv regression_summary.csv regression_qpsk_soft_smooth_summary.csv \
 && \
./tests/run_regression.sh --modcods "1:0 1:1" --snr "10 11 12 13 14 15 16" \
    --channels combined --cfo-sc 0 --soft --smooth \
 && mv regression_results.csv regression_16qam_soft_smooth.csv \
 && mv regression_summary.csv regression_16qam_soft_smooth_summary.csv
```

#### 2. Frame-averaged sweep (5 frames per point, ~50 min — variance-limited cliff localization)

```bash
./tests/run_regression.sh --modcods "0:0 0:1" --snr "5 6 7 8 9 10" \
    --channels combined --cfo-sc 0 --soft --smooth --frames 5 \
 && mv regression_results.csv regression_qpsk_avg5.csv \
 && mv regression_summary.csv regression_qpsk_avg5_summary.csv \
 && \
./tests/run_regression.sh --modcods "1:0 1:1" --snr "10 11 12 13 14 15 16" \
    --channels combined --cfo-sc 0 --soft --smooth --frames 5 \
 && mv regression_results.csv regression_16qam_avg5.csv \
 && mv regression_summary.csv regression_16qam_avg5_summary.csv
```

#### 3. Baseline before changes (rename to `_baseline` to keep prior runs)

Same as #2 but renames outputs to `regression_*_baseline.csv` so a prior `_avg5`
run is preserved for comparison.  Use this pattern when you're about to add a
chain feature and want a clean before/after.

```bash
./tests/run_regression.sh --modcods "0:0 0:1" --snr "5 6 7 8 9 10" \
    --channels combined --cfo-sc 0 --soft --smooth --frames 5 \
 && mv regression_results.csv regression_qpsk_baseline.csv \
 && mv regression_summary.csv regression_qpsk_baseline_summary.csv \
 && \
./tests/run_regression.sh --modcods "1:0 1:1" --snr "10 11 12 13 14 15 16" \
    --channels combined --cfo-sc 0 --soft --smooth --frames 5 \
 && mv regression_results.csv regression_16qam_baseline.csv \
 && mv regression_summary.csv regression_16qam_baseline_summary.csv
```

#### 4. CFO-enabled sweep (only after CFO correction is implemented in the chain)

Real-radio CFO will be in the 0.05–0.30 SC range (sub-ppm AD9364 to ±20 ppm
TCXO drift).  Today the chain has **no** CFO correction (`sync_detect` v5
removed it), so this sweep will fail catastrophically until the CFO block is
re-added.  Once CFO correction lands, the same matrix should produce cliffs
within ~1 dB of the `--cfo-sc 0` baseline.

```bash
# 0.1 SC — modest realistic TCXO offset
./tests/run_regression.sh --modcods "0:0 0:1" --snr "5 6 7 8 9 10" \
    --channels combined --cfo-sc 0.1 --soft --smooth --frames 5 \
 && mv regression_results.csv regression_qpsk_cfo01.csv \
 && mv regression_summary.csv regression_qpsk_cfo01_summary.csv \
 && \
./tests/run_regression.sh --modcods "1:0 1:1" --snr "10 11 12 13 14 15 16" \
    --channels combined --cfo-sc 0.1 --soft --smooth --frames 5 \
 && mv regression_results.csv regression_16qam_cfo01.csv \
 && mv regression_summary.csv regression_16qam_cfo01_summary.csv
```

CFO injection levels (channel sim `--cfo-sc N`, where 1 = one full subcarrier
spacing ≈ 78 kHz at 20 MSPS):

| `--cfo-sc` | What it represents |
|---|---|
| 0.001 | sub-ppm AD9364 (calibrated) — chain handles this today |
| **0.05** | **±2 ppm TCXO at 2.4 GHz** — typical non-AD9364 RFIC |
| **0.13** | **Ku-band 200 m/s Doppler** — MALE/HALE airframe |
| 0.30 | aggressive stress test (currently → BER ~0.36) |
| 0.50 | Schmidl-Cox max pull-in — absolute limit |
| 1.0 | beyond pull-in — unphysical |

#### 5. Path-A baked-in defaults — what runs by default in the Python chain

These three refinements were measured during the Task-3 / Task-2 work and
are **default ON** in the Python chain.  They are picked up automatically
by `run_regression.sh`, `python3 sim/ofdm_reference.py --gen`, and every
`--decode-full-*` variant.  No flags needed.

| Refinement | What it does | Effect |
|---|---|---|
| **LLR clipping** | Median-based outlier clipping of soft Viterbi inputs (`--llr-clip-factor 5.0`) | Tames metric outliers at the cliff edge — small but consistent variance reduction |
| **Weighted CPE** | Weights each pilot's CPE contribution by `\|G[k]\|²` | +0.3–0.5 dB on multipath; near-zero on AWGN |
| **Header FEC (rate-1/2)** | K=7 conv-coded BPSK header (default rate-1/2 for forward-compat with future MAC ~70-bit payload; rate-1/3 available via `--header-fec-rate 1/3` for max gain on small headers) | +3–5 dB header-CRC margin |

**To opt out** (e.g. for HLS-TX backward compat, or to measure the delta vs no refinements):

| Flag | Effect |
|---|---|
| `--no-llr-clip` | Disable LLR clipping |
| `--no-weighted-cpe` | Disable pilot-magnitude weighting |
| `--no-header-fec` | Disable conv-coded header — uses original 26-bit uncoded BPSK layout |

**HLS TX compatibility**: `run_regression.sh --hls-rx` automatically enables
`--no-header-fec` because `./setup_vitis.sh csim` (the HLS TX) only produces
the uncoded 26-bit header.  No user action needed — the `--hls-rx` flag
takes care of it.

**Default Python TX flow** (`run_regression.sh` without `--hls-rx`):
- TX: `python3 sim/ofdm_reference.py --gen` writes `tb_tx_output_ref.txt`
  (with header FEC ON by default — 64 BPSK SCs at rate-1/2)
- Channel sim reads `tb_tx_output_ref.txt`
- Python decoder reads the noisy IQ with all refinements ON
- HLS TX C-sim is **not** invoked

**HLS-comparison flow** (`run_regression.sh --hls-rx`):
- TX: `python3 --gen --no-header-fec` + `./setup_vitis.sh csim` writes `tb_tx_output_hls.txt`
  (uncoded header, matches HLS RX expectations)
- Channel sim reads `tb_tx_output_hls.txt`
- Python decoders called with `--no-header-fec`; HLS RX C-sim runs unmodified
- Apples-to-apples HLS-vs-Python comparison on the same uncoded bitstream

### Per-point cost (rough)

| Decoder | Time per frame |
|---|---|
| Channel sim                     | ~2 s |
| FP64+sync HARD                  | ~4 s |
| Q15+sync HARD                   | ~21 s (Q15 sync loop dominates) |
| FP64+sync SOFT (`--soft`)       | ~4 s |
| Q15+sync SOFT (`--soft`)        | ~21 s |
| Channel smoothing (`--smooth`)  | adds ~0 s — just one iDFT+DFT per frame, dwarfed by sync |
| HLS rx_noisy_csim (`--hls-rx`)  | ~30 s |

Default invocation (4 modcods × 5 channels × 1 SNR × 1 frame, FP64-hard + Q15-hard):
**~10 min**. Adding `--hls-rx` roughly doubles that. Adding `--soft` adds
~50% more. `--smooth` is essentially free (the iDFT/DFT cost is negligible
next to the sync loop and Viterbi).

### Decoder variants in `sim/ofdm_reference.py` (what `run_regression.sh` invokes)

The 8 standalone decoder flags also work directly via Python — useful for
one-off diagnostics:

```bash
python3 sim/ofdm_reference.py --decode-full-sync          --input <noisy.txt> --mod M --rate R   # FP64 + sync + HARD Viterbi
python3 sim/ofdm_reference.py --decode-full-q15-sync      --input <noisy.txt> --mod M --rate R   # Q15  + sync + HARD
python3 sim/ofdm_reference.py --decode-full-soft          --input <noisy.txt> --mod M --rate R   # FP64 + sync + SOFT Viterbi
python3 sim/ofdm_reference.py --decode-full-q15-soft      --input <noisy.txt> --mod M --rate R   # Q15  + sync + SOFT
python3 sim/ofdm_reference.py --decode-full-smooth        --input <noisy.txt> --mod M --rate R   # FP64 + sync + HARD + DFT smoothing
python3 sim/ofdm_reference.py --decode-full-q15-smooth    --input <noisy.txt> --mod M --rate R   # Q15  + sync + HARD + DFT smoothing
python3 sim/ofdm_reference.py --decode-full-soft-smooth   --input <noisy.txt> --mod M --rate R   # FP64 + sync + SOFT + smoothing
python3 sim/ofdm_reference.py --decode-full-q15-soft-smooth --input <noisy.txt> --mod M --rate R # Q15  + sync + SOFT + smoothing
python3 sim/ofdm_reference.py --sync-only                 --input <noisy.txt>                    # sync only, no decode
```

`--smooth-taps N` overrides the channel-smoothing time-domain truncation
length (default 64; sweet spot 64–128 — see `_smooth_channel_dft()`
docstring in `sim/ofdm_reference.py` for why aggressive truncation hurts).

### Outputs

| File | Contents |
|---|---|
| `regression_results.csv` | One row per `(modcod, channel, SNR, frame)` — header CRC + BER per decoder |
| `regression_summary.csv` | Mean + stddev BER per `(modcod, channel, SNR)` across frames |

### Channel models (defined in `sim/ofdm_channel_sim.py`)

| Channel | What it adds (in apply order) |
|---|---|
| `awgn`      | AWGN at the requested SNR |
| `multipath` | 2-tap reflection (delays 1 & 3 samples, amps 0.30 & 0.15) + AWGN |
| `phase`     | AWGN + Wiener phase-noise walk (`phase-sigma` rad/sample) |
| `cfo`       | CFO (`cfo-sc` SC spacings) + AWGN |
| `combined`  | CFO + multipath + AWGN + phase noise (worst-case stack) |

The Python RX does **not** model CFO correction (HLS chain v5 also doesn't —
it relies on per-symbol pilot CPE and the AD9364's own RX-LO). Expect both
`cfo` and `combined` channels to fail when CFO is large enough to outrun the
per-symbol CPE pull-in range.

---

## `run_loopback.sh` — clean loopback, single modcod

Pre-noise sanity check. 5 steps:

1. Generate random bits + Python TX reference (`sim/ofdm_reference.py --gen`).
2. HLS TX C-sim (`./setup_vitis.sh csim`) → clean IQ.
3. EVM compare HLS TX vs Python TX reference.
4. HLS RX C-sim → decoded bytes → BER vs original bits.
5. Python reference decoder on HLS TX IQ → independent BER check.

```bash
./tests/run_loopback.sh                    # defaults: 16-QAM rate-1/2
./tests/run_loopback.sh --mod 0            # QPSK rate-1/2
./tests/run_loopback.sh --mod 0 --rate 1   # QPSK rate-2/3
./tests/run_loopback.sh --mod 1 --rate 1   # 16-QAM rate-2/3
```

Pass criterion: EVM < 5%, RX BER = 0, Python ref BER = 0.

---

## `run_loopback_all.sh` — clean loopback, all modcods

Wraps `run_loopback.sh` over the 4 modcods, harvests `RESULT` + `EVM` + final
`BER` from each per-run log. Fast pre-flight before noisy testing.

```bash
./tests/run_loopback_all.sh
```

Output: per-modcod summary + aggregate pass/fail; per-run logs in `/tmp/loopback_all_<pid>/`.

---

## `run_loopback_noisy.sh` — noisy loopback, single modcod

Like `run_loopback.sh` but injects AWGN between TX and RX. 5 steps:

1. Gen bits + Python TX ref.
2. HLS TX C-sim.
3. `sim/ofdm_channel_sim.py --channel awgn --snr <SNR>` → noisy IQ.
4. HLS RX C-sim on noisy IQ → BER.
5. Python reference decoders (FP64 + Q15) on the same noisy IQ → BER.

```bash
./tests/run_loopback_noisy.sh                          # defaults: SNR=20 16-QAM 1/2
./tests/run_loopback_noisy.sh --snr 15                 # custom SNR
./tests/run_loopback_noisy.sh --snr 10 --mod 0         # QPSK at 10 dB
./tests/run_loopback_noisy.sh --snr 15 --mod 1 --rate 1  # 16-QAM rate-2/3
```

This is the proven single-point flow that `run_regression.sh` is structured
around. Touch with care.

---

## `run_ber_sweep.sh` — HLS RX-only sweep

HLS-only BER sweep over modcods × SNR × channel. Uses `./setup_vitis.sh
rx_noisy_csim` per point and greps `vitis_rx_noisy_csim.log`.

```bash
./tests/run_ber_sweep.sh                                  # full default sweep
./tests/run_ber_sweep.sh --snr-step 2                     # coarser sweep
./tests/run_ber_sweep.sh --snr "10 15 20"                 # custom list
./tests/run_ber_sweep.sh --channel awgn                   # AWGN only
./tests/run_ber_sweep.sh --modcods "0:0 1:1"              # subset
```

This is the older HLS-only sweep — useful for quantifying the HLS chain in
isolation (no Python comparison). For Python-vs-HLS apples-to-apples, prefer
`run_regression.sh --hls-rx`.

---

## File map

```
tests/
├── test.md                  ← this file
├── run_regression.sh        ← primary entry point (full TX → channel → RX)
├── run_loopback.sh          ← single-modcod clean loopback
├── run_loopback_all.sh      ← all 4 modcods, clean loopback
├── run_loopback_noisy.sh    ← single-modcod AWGN loopback (proven)
└── run_ber_sweep.sh         ← HLS RX-only sweep
```

Outside `tests/`, the relevant pieces are:
- `setup_vitis.sh`          — Vitis HLS environment + per-IP csim/cosim/synth driver
- `synth_all_ips.sh`        — bulk HLS synthesis across all 6 IPs
- `sim/ofdm_reference.py`   — Python TX + RX (FP64 and Q15, hard and soft, with sync_detect_reference)
- `sim/ofdm_channel_sim.py` — AWGN / multipath / phase noise / CFO / combined channel models
- `sim/fec_reference.py`    — K=7 conv encoder + Viterbi (hard + soft)
- `tb/*.cpp`                — per-IP HLS testbenches

---

## RTL Co-simulation (per-IP, on-demand)

Proves: synthesised RTL of an HLS IP matches its C-sim output sample-for-sample.
Run on demand, not in the regression — each cosim is 20–60 min per IP.

```bash
./setup_vitis.sh tx_cosim 1     # ofdm_tx, 16-QAM, ~20 min
./setup_vitis.sh rx_cosim 1     # ofdm_rx, 16-QAM, ~60 min
```

Expected:
```
INFO: [COSIM 212-1000] *** C/RTL co-simulation finished: PASS ***
```

If C-sim passes but cosim fails, the failure is in HLS-to-RTL synthesis
(overflow modes, AXIS handshake races, etc).

---

## Diagnostic helpers (when something fails)

These are not part of routine regression — use when debugging a specific issue.

### Bypass sync_detect — feed pre-aligned IQ directly to ofdm_rx

Isolates `sync_detect` bugs from `ofdm_rx` bugs:

```bash
OFDM_RX_BYPASS_SYNC=1 ./tests/run_loopback_noisy.sh --mod 1 --rate 0 --snr 15
```

### Python BPSK header decoder only (Q15-class, mirrors HLS exactly)

Used to find the AP_SAT bug in commit `d08a537` — see `docs/RX_LOW_SNR_DEBUG.md`.

```bash
python3 sim/ofdm_reference.py --decode-header-q15 \
    --input tb_tx_output_hls_noise.txt --mod 1 --rate 0
```

### HLS-side per-pilot diagnostic dumps

Appear in `vitis_rx_csim.log` / `vitis_rx_noisy_csim.log`, prefixed `[HLS]`:

```
[HLS] phase_err = +0.384 deg
[HLS] freq_buf[pilot 50,75,100,154,179,204]: ...
[HLS] G_eq[pilot 50,75,100,154,179,204]: ...
[HLS] hdr eq.real (26 bits): ...
```

Diff against the Python `decode_header_q15` output to pin down precision divergence.

### Python sync detector only (no decode)

Triggers + metric dump for the sync_detect_reference, both Q15 and FP64:

```bash
python3 sim/ofdm_reference.py --sync-only --input tb_tx_output_hls_noise.txt
```

---

## Test layers (conceptual)

| Layer | What runs | Tooling | Pass time |
|-------|-----------|---------|-----------|
| L1: Algorithm | Python `decode_full(use_q15=True)` on noisy file | `sim/ofdm_reference.py` | ~5 sec |
| L2: HLS C-sim | `./tests/run_loopback_noisy.sh --mod m --rate r --snr s` | rebuilds `csim.exe` per modcod | ~50 sec |
| L3: Full regression | `./tests/run_regression.sh ...` | sweeps modcods × channels × SNR × frames | ~10 min default; ~50 min with --frames 5 |
| L4: RTL co-sim | Vivado RTL sim of `ofdm_rx` against same vectors | `setup_vitis.sh rx_cosim` | ~5 min/case (subset only) |
| L5: Bitstream | Vivado P&R + bitstream generation | LiteX make flow | ~30 min |

L1 is the golden reference. L2 must match L1 within ±5%. L3 sweeps the matrix.
L4 must match L2 sample-exact. L5 doesn't change BER but exposes timing/synthesis regressions.
