# OFDM-HLS Worklog

Append-only log of significant work sessions. Newest entries on top.

---

## 2026-04-28 — Reed-Solomon outer code (RS(255, 223)) integrated, ACM thresholds locked

Branch: `rx-dsp-opt`

### Outer-code integration

| # | Item | Files | Notes |
|---|---|---|---|
| 1.1 | RS(255, 223) wrappers — `rs_encode()` / `rs_decode()` / `rs_max_payload()` (CCSDS poly, t=16 byte errors per 255-byte codeword) | `sim/fec_reference.py` | thin wrappers around `reedsolo` library; HLS production swaps to Xilinx RS IP later |
| 1.2 | TX integration: `app → scramble → RS encode → conv encode → interleave` | `sim/ofdm_reference.py:generate()` | `use_reed_solomon=True` default |
| 1.3 | RX integration: `… → Viterbi → RS decode → descramble`. On uncorrectable codeword, prints `RS UNCORRECTABLE` and returns all-zero frame (BER comparison flags it) | `sim/ofdm_reference.py:decode_full(),decode()` | same default |
| 1.4 | CLI: `--no-rs` opt-out (mirrors `--no-header-fec`) | `sim/ofdm_reference.py` argparse | needed for HLS-TX comparison runs (HLS chain has no RS yet) |
| 1.5 | Dead-zone padding helper — reedsolo emits only K + 32·⌈K/223⌉ bytes; `rs_max_payload()` finds largest K below target output and zero-pads the rest | `sim/fec_reference.py` | only triggers for 16QAM 2/3 at small `n_syms`; default `N_SYMS=255` hits zero-padding for all 4 modcods |

### Library / venv setup

- `RSCodec(32)` → 32 parity bytes per codeword, t=16 (CCSDS deep-space standard, ~3 dB more coding gain than RS(255, 239)).
- New project venv at `ofdm-hls/.venv/` with `numpy`, `scipy`, `reedsolo`. System `python3` doesn't have these.
- `tests/run_regression.sh`: added `PYTHON=$REPO_DIR/.venv/bin/python` autodetect; replaced all `python3 …` calls to use `$PYTHON`. Without this the script silently fell back to system python (no reedsolo) and gen failed → all-50% BER.

### Regression-script wiring

| # | Change | Files |
|---|---|---|
| 2.1 | New `NO_RS_FLAG=""` (sets to `--no-rs` only when `--hls-rx` is passed, same pattern as `NO_HDR_FEC_FLAG`) | `tests/run_regression.sh` |
| 2.2 | `NO_RS_FLAG` threaded through every Python decoder invocation (FP64-hard, Q15-hard, FP64-soft, Q15-soft) | `tests/run_regression.sh` |
| 2.3 | All `python3 …` → `"$PYTHON" …` so the venv is honoured | `tests/run_regression.sh` |

### Bugs found & fixed

| # | Issue | Fix |
|---|---|---|
| 3.1 | First regression with `--no-rs` and reedsolo not installed: gen failed silently, downstream decoded stale TX file → 50% BER at every SNR | added `PYTHON=…/.venv/bin/python` and installed `numpy + scipy + reedsolo` into the venv |
| 3.2 | 16QAM 2/3 with `--nsyms 4`: pre-existing assert `len(coded_bytes) == total_coded` fires (399 vs 400). `total_raw=266` not divisible by 2 cleanly with rate-2/3 puncturing. | Pre-existing edge case unrelated to RS — only fires for `n_syms ∉ {3, 6, 9, …}`. Default `N_SYMS=255` works. Documented; no fix attempted. |
| 3.3 | Two regression runs colliding on `tb_tx_output_ref.txt` / `tb_input_to_tx.bin` produced corrupted results (50% BER even at high SNR) | killed both, re-ran fresh; added concurrent-run note for future test design |

### Measurements — Q15-soft + RS cliffs (combined channel, CFO 0.13–0.2 SC)

5-frame averaged, channel = AWGN + multipath + phase + CFO injected.  Q15-soft is the production decoder; FP64 omitted from this table per `feedback_rx_modes.md`.

| Modcod | Pre-RS cliff (yesterday) | RS-on cliff (today) | Coding gain |
|---|---|---|---|
| QPSK 1/2 (CFO 0.2 SC) | ~6 dB | **< 5 dB** | ≥ 1 dB |
| QPSK 2/3 (CFO 0.2 SC) | ~9 dB | **7 dB** (6 dB = 3/5 frames) | ~2 dB |
| 16QAM 1/2 (CFO 0.13 SC) | 12 dB | **10 dB** | 2 dB |
| 16QAM 2/3 (CFO 0.13 SC) | 16 dB | **13 dB** (12 dB = 4/5 frames) | 3 dB |

Hard-decision cliffs are 1–2 dB worse than soft.  RS gain on hard alone is ~2 dB (burst-error limited); RS gain on soft is 3–4 dB (fewer Viterbi error events → fewer RS codewords overflow t=16).

### ACM thresholds locked (combined-channel SNR, 5 dB margin per step)

| SNR range | Modcod | Net throughput |
|---|---|---|
| < 10 dB | QPSK 1/2 | 12 Mbps |
| 10–15 dB | QPSK 2/3 | 16 Mbps |
| 15–20 dB | 16QAM 1/2 | 24 Mbps |
| > 20 dB | 16QAM 2/3 | 33 Mbps |

Saved as `memory/project_acm_thresholds.md`.  The MAC's ACM closed loop uses these breakpoints with hysteresis to prevent hunting.  Default modcod at boot / link-loss = QPSK 1/2.

### Pluto SDR sensitivity story

20 MHz BW + AD9363 NF ~3 dB → noise floor ≈ -98 dBm at the antenna.  Mapping to today's cliffs:

| Modcod | RX sensitivity (cliff) | Margin to ACM threshold |
|---|---|---|
| QPSK 1/2 | -93 dBm @ 5 dB SNR | -88 dBm @ 10 dB ACM ceiling |
| QPSK 2/3 | -91 dBm @ 7 dB | -83 dBm @ 15 dB |
| 16QAM 1/2 | -88 dBm @ 10 dB | -78 dBm @ 20 dB |
| 16QAM 2/3 | -85 dBm @ 13 dB | (max throughput tier) |

Pluto's TX power is ~7 dBm — at -85 dBm RX sensitivity, link budget = 92 dB.  Free-space path loss at 2.4 GHz: 92 dB ≈ 100 m line-of-sight.  Plenty of margin for short-range OTA validation.

### CFO tolerance

`sync_detect`'s Schmidl-Cox estimator uses the preamble's two-identical-halves property → unambiguous CFO range = **±0.5 SC**.  Beyond that the estimator aliases (CFO 0.8 SC misread as -0.2 SC, "correction" leaves residual +1.0 SC = catastrophic).  Tested 0.13 and 0.2 SC; both correct cleanly.  AD9363 TCXO drift at 2.4 GHz is sub-ppm = <2.4 kHz = <0.03 SC at 20 MSPS — comfortable margin to the 0.5 SC ceiling.

### Discussion-only / planned next

- **MAC redesign** — single `ofdm_mac` HLS block with internal `mac_tx_main()` / `mac_rx_main()` functions; ACM closed loop using thresholds above; 2 priority classes (control vs payload); selective ARQ on the control class; AES-128 stub; fragmentation > 4 KB Ethernet/MAVLink frames.
- **Pluto SDR OTA bring-up** — once MAC is stable in Python sim.  Lab line-of-sight first, then short-range outdoor.
- HLS port of the Path-A refinements (sync_detect+CFO, soft Viterbi, RS, channel smoothing, weighted CPE) deferred until MAC sprint completes.

---

## 2026-04-27 — Path-A Python RX hardening: refinements baked in, header FEC, CFO estimator + corrector

Branch: `rx-dsp-opt`

### Python RX chain features added

| # | Feature | Files | Status |
|---|---|---|---|
| 1.1 | `sync_detect_reference()` — Q15 + FP64 mirror of `src/sync_detect.cpp` v5 (Schmidl-Cox CP correlation + power threshold) | `sim/ofdm_reference.py` | committed `4f30ba6` |
| 1.2 | `_smooth_channel_dft()` — DFT-based MMSE channel smoothing (default `L_taps=64`) | `sim/ofdm_reference.py` | committed `4f30ba6` |
| 1.3 | `viterbi_decode_soft()` — linear-soft Viterbi with optional median-based LLR clipping | `sim/fec_reference.py` | committed `4f30ba6`, extended `c4dc99f` |
| 1.4 | Soft demap (QPSK signed real/imag; 16-QAM per-axis high+low bits) + soft deinterleave | `sim/ofdm_reference.py` | committed `4f30ba6` |
| 1.5 | LLR clipping (`use_llr_clip=True` default; `clip_factor=5.0` × median(\|soft\|)) | `sim/fec_reference.py`, `sim/ofdm_reference.py` | committed `c4dc99f` |
| 1.6 | Pilot-magnitude weighted CPE (`use_weighted_cpe=True` default) | `sim/ofdm_reference.py` | committed `c4dc99f` |
| 1.7 | Header FEC encoder + decoder — K=7 conv, rate-1/2 default (rate-1/3 selectable) | `sim/fec_reference.py`, `sim/ofdm_reference.py` | committed `c4dc99f` |
| 1.8 | `generate()` prepends a guard symbol matching HLS TX C2 layout (Python TX layout-compat with HLS) | `sim/ofdm_reference.py` | committed `c4dc99f` |
| 1.9 | **CFO estimator + time-domain derotator** — Schmidl-Cox angle from sync's P, applied before CP strip. Verified end-to-end (5-frame avg5 sweep at CFO=0.13 SC fully recovers cliff to CFO=0 baseline). | `sim/ofdm_reference.py` | done, commit pending |
| 1.10 | All Path-A refinements (1.5/1.6/1.7/1.9) **default ON** with `--no-*` opt-outs | `sim/ofdm_reference.py` | `c4dc99f` (1.5–1.7); 1.9 pending |

### Bugs / issues found and fixed

| # | Issue | Fix |
|---|---|---|
| 2.1 | `run_ber_sweep.sh` parser broken — FAIL on every point even when decoder worked | rewrote parser to match `run_loopback_noisy.sh` pattern (grep log file, awk fields) |
| 2.2 | Hardcoded `LD_PATH` in `run_ber_sweep.sh` referenced wrong user (`/home/abhinavb`) | sourced from `setup_vitis.sh:VITIS_IP_LDPATH` |
| 2.3 | DFT smoothing with `L_taps=16` made BER **worse** | diagnosed as null-SC windowing leakage; default raised to 64 (sweet spot 64–128) |
| 2.4 | Stale `ber_sweep_summary.csv` showing 36% BER at 20 dB QPSK 1/2 (architect-flagged) | deleted; added to `.gitignore` |
| 2.5 | Header FEC A/B test "showed 0/10 PASS" | tester error — was calling `--decode-full` (legacy path) instead of `--decode-full-soft-smooth` |
| 2.6 | "Soft Viterbi broken" in regression run | stale `tb_input_to_tx.bin` from earlier 16QAM run; fresh `--gen` fixed |
| 2.7 | CFO=0.13 SC kills the chain (~6 dB cliff penalty for 16QAM 2/3) | Task 1 — preamble Schmidl-Cox CFO estimate + derotator restores cliff to CFO=0 baseline |

### Documentation reconciliation (architect review §1 fixes)

| # | Issue | Resolution |
|---|---|---|
| 3.1 | 3 different DSP utilization numbers across docs (67% / 78% / 92%) | reconciled to **67.5%** (full SoC litex build, source: `litex/build/.../utilization_place.rpt`) |
| 3.2 | 2 different throughput numbers (30.9 vs 4.17 Mbps) | single formula: 36.7 Mbps PHY → 31–33 Mbps net for 16-QAM r2/3 at 20 MSPS, 255-symbol frame |
| 3.3 | False "sub-millisecond PHY round-trip" claim | replaced with "3.7 ms one-way / ≥7.4 ms half-duplex round-trip" |
| 3.4 | `sync_cfo` references half-cleaned in ENGINEERING_SPEC | finished retirement (v5 — CFO removed; AD9364 sub-ppm + per-symbol CPE) |
| 3.5 | All `./run_loopback*.sh` refs in docs were stale (root → `tests/`) | bulk-updated across README, OFDM_HLS_ANALYSIS, OPTIMIZATION_GUIDE, RX_LOW_SNR_DEBUG |

### Repo organization

- Created `tests/` folder; `git mv` moved `run_loopback.sh`, `run_loopback_all.sh`, `run_loopback_noisy.sh`, `run_ber_sweep.sh` into it (with `cd "$SCRIPT_DIR/.."` patches).
- New `tests/run_regression.sh` — full TX → channel → RX driver with `--soft`, `--smooth`, `--hls-rx`, `--frames`, `--cfo-sc` flags.
- New `tests/test.md` — script catalog + 5 numbered "Standard regression workflows" + RTL co-sim section + diagnostic helpers + Path-A baked-in defaults.
- Deleted `docs/REGRESSION_PLAN.md` + `docs/VALIDATION.md` (superseded); unique content merged into `tests/test.md`.
- `.gitignore` updated to exclude `regression_*.csv`, `python_rx_*.csv`, `ber_sweep_summary.csv`.
- `run_regression.sh` default flow: Python TX (`tb_tx_output_ref.txt`) skipping HLS C-sim; `--hls-rx` flag = HLS TX + auto `--no-header-fec` for Python decoders.

### Architectural decisions

| # | Decision | Memory file |
|---|---|---|
| 5.1 | **Path strategy: A then C** — UAV datalink first, tactical later. Path B (research-only) is OFF the table. | `project_path_strategy.md` |
| 5.2 | **MAC architecture**: single `ofdm_mac` HLS block with internal `mac_tx_main()` / `mac_rx_main()` functions; do NOT split into two physical IPs (host needs single MAC entity, AD9361 + CSR commons are shared) | `project_mac_architecture.md` |
| 5.3 | **Header FEC rate**: default rate-1/2 (capacity for ~70-bit MAC payload) instead of rate-1/3 (limited to ≤44 payload bits) | encoded in `conv_encode_header(rate="1/2")` default |

### Measurements (5-frame averaged, combined channel, soft+smooth, all refinements ON)

| Configuration | 16QAM 1/2 cliff | 16QAM 2/3 cliff |
|---|---|---|
| Path-A refinements ON, CFO=0 | 13 dB | 16 dB |
| Path-A refinements ON, CFO=0.13, **no CFO correct** | ~17 dB (+4 dB penalty) | ≥22 dB (+6 dB penalty) |
| Path-A refinements ON, CFO=0.13, **with CFO correct** (Task 1) | **13 dB** (fully recovered) | **16 dB** (fully recovered) |

CFO estimator accuracy at 16QAM 2/3 with true CFO=0.13:
- SNR=10: estimated 0.116 (11% error)
- SNR=12: estimated 0.121 (7% error)
- SNR=14: estimated 0.122 (6% error)
- SNR=16: estimated 0.125 (4% error)

Full per-SNR cliff recovery, FP64-soft mean BER, 5 frames per point, channel = combined (CFO + multipath + AWGN + phase noise), CFO injected = 0.13 SC:

| SNR | 16QAM 1/2 (CFO=0) | 16QAM 1/2 (CFO=0.13 + Task 1) | 16QAM 2/3 (CFO=0) | 16QAM 2/3 (CFO=0.13 + Task 1) |
|---|---|---|---|---|
| 10 | 1.08e-3 | **1.01e-3** | 3.96e-2 | **4.83e-2** |
| 11 | 1.47e-4 | **1.88e-4** | 1.61e-2 | **1.24e-2** |
| 12 | 5.9e-6  | **5.88e-6** | 2.20e-3 | **2.81e-3** |
| 13 | 0      | **0**       | 1.91e-4 | **2.56e-4** |
| 14 | 0      | **0**       | 4.42e-5 | **7.34e-5** |
| 15 | 0      | **0**       | 4.27e-5 | **1.91e-5** |
| 16 | 0      | **0**       | 0       | **4.41e-6** (1 stray bit) |

All 70 frames per modcod had header CRC PASS.  Q15 ≈ FP64 throughout.  No
SNR penalty from CFO injection — cliff is identical within frame variance.

### Commits today

| SHA | Title |
|---|---|
| `4f30ba6` | sim/tests/docs: Python RX chain improvements + tests/ folder + doc reconciliation |
| `c4dc99f` | sim/tests: Path-A header FEC + soft-Viterbi LLR clip + weighted CPE; bake refinements ON by default |
| (pending) | sim: Task 1 — Python CFO estimator + corrector + worklog |

### Discussion-only / planned next

- **Reed-Solomon outer code** — RS(255, 239) at 6% rate overhead. Expected to push 16QAM 1/2 cliff from 12 dB → ~10 dB; 16QAM 2/3 from 16 dB → ~14 dB. Stacks on top of CFO correction.
- HLS major restructure (sync_detect simplification, CFO block reintroduction, ofdm_rx stage split, Xilinx Viterbi IP swap, header FEC HLS port) — multi-week milestone.
- MAC redesign (single block, internal TX/RX split + ACM closed loop + 2 priority classes + selective ARQ on C2 + AES-128 stub).

---
