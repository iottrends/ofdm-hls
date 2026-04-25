# OFDM PHY Regression Plan — Master Test Matrix

**Goal**: prove the HLS RX chain decodes correctly across the full
operating envelope before declaring "RX works" and moving to RF
bring-up.

**Scope**: algorithmic regression at C-sim and RTL co-sim level. Does
NOT cover RF analog characterization (that's in the bring-up plan).

**Pass criterion**: HLS BER ≤ Python Q15 reference BER × 1.10 (10% margin)
at every test point. Header CRC must PASS at every point with SNR
≥ algorithm cliff.

## Test dimensions

```
                MODCOD          ×    FRAME SIZE        ×    CHANNEL                 ×    SNR
              ───────────         ─────────────         ─────────────                ─────────
              QPSK r=1/2          n_syms = 4            AWGN-only                    20 / 17 / 15 / 12 / 10 dB
              QPSK r=2/3          n_syms = 8            AWGN + CFO (±2 ppm)          (each modcod has its own
              16-QAM r=1/2        n_syms = 16           AWGN + phase noise            relevant SNR range)
              16-QAM r=2/3        n_syms = 32           AWGN + 2-tap multipath
                                  n_syms = 255          AWGN + multipath + phase noise + CFO
                                  (current default)
```

Total points: 4 modcods × 5 frame sizes × 5 channels × 5 SNRs = **500
test points**. Run as nightly regression matrix; per-point runtime
~1 min ⇒ ~8 hours full pass on a single core.

## Test layers

| Layer | What runs | Tooling | Pass time |
|-------|-----------|---------|-----------|
| L1: Algorithm | Python `decode_full(use_q15=True)` on noisy file | `sim/ofdm_reference.py` | ~5 sec |
| L2: HLS C-sim | `./run_loopback_noisy.sh --mod m --rate r --snr s` | rebuilds `csim.exe` per modcod | ~50 sec |
| L3: RTL co-sim | Vivado RTL sim of `ofdm_rx` against same vectors | `setup_vitis.sh rx_cosim` | ~5 min/case (subset only) |
| L4: Bitstream | Vivado P&R + bitstream generation | LiteX make flow | ~30 min |

L1 is the golden reference. L2 must match L1 within tolerance. L3 must
match L2 (sample-exact). L4 doesn't change BER but exposes timing/
synthesis regressions.

## Channel models — TODO state

`sim/ofdm_channel_sim.py` currently implements:

- ✅ AWGN (`add_awgn`)
- ✅ CFO (`add_cfo`) — ±0.5 SC fractional CFO
- ✅ Phase noise (`add_phase_noise`) — Wiener random walk
- ✅ Multipath (`add_multipath`) — 2-tap channel template
- ✅ `apply_channel(channel='awgn'|'cfo'|'phase'|'multipath'|'combined')`

What's missing for the regression plan:

- [ ] **Frame-size sweep** in TBs — `tb/ofdm_rx_tb.cpp` and `ofdm_tx_tb.cpp`
      currently hardcode `TB_N_SYMS = 255`. Add a `--nsyms <N>` runtime arg
      mirroring the existing `--mod`/`--rate` plumbing.
- [ ] **`run_loopback_noisy.sh --channel <type>`** flag — currently
      hardcoded to AWGN. Add channel selection that maps to
      `ofdm_channel_sim.py --channel <type>`.
- [ ] **Multi-frame test** — push 4 back-to-back packets at the AD9364 rate.
      Tests sync_detect's deaf window + re-arm path, n_syms_fb feedback
      timing across frames.

## Test matrix — current status

| Modcod        | n_syms | Channel  | SNR sweep  | Status |
|---------------|--------|----------|------------|--------|
| QPSK r=1/2    | 255    | AWGN     | 10–20 dB   | ✅ PASS (in `rx-dsp-opt` close-out) |
| QPSK r=2/3    | 255    | AWGN     | 10–20 dB   | ✅ PASS |
| 16-QAM r=1/2  | 255    | AWGN     | 10–20 dB   | ✅ PASS |
| 16-QAM r=2/3  | 255    | AWGN     | 13–20 dB   | ✅ PASS at SNR ≥ 13 |
| All 4 modcods | 4, 8, 16, 32 | AWGN | 10–20 dB | ⏳ pending — needs `--nsyms` plumbing |
| All 4 modcods | 255    | CFO      | 10–20 dB   | ⏳ pending — channel sim ready, scripts not |
| All 4 modcods | 255    | Phase    | 10–20 dB   | ⏳ pending — same |
| All 4 modcods | 255    | Multipath| 10–20 dB   | ⏳ pending — same |
| All 4 modcods | 255    | Combined | 15–25 dB   | ⏳ pending |
| Multi-frame   | 4 × 255 | AWGN    | 15 dB      | ⏳ pending — sync_detect re-arm path |

## Implementation tasks (ordered)

### Step 1 — Frame-size plumbing (~1 hr)

- `tb/ofdm_tx_tb.cpp`: add `--nsyms` arg, default 255.
- `tb/ofdm_rx_tb.cpp`: same. Update `TB_N_SYMS` to be a runtime variable.
  Adjust the bypass `target` count and `total_samples` calculations.
- `setup_vitis.sh csim` and `rx_csim` / `rx_noisy_csim`: add 3rd
  positional arg for nsyms, default 255.
- `sim/ofdm_reference.py`: already has `--nsyms` arg.
- `sim/ofdm_channel_sim.py`: already has `--nsyms` (sym count) arg.
- `run_loopback.sh` and `run_loopback_noisy.sh`: add `--nsyms <N>` flag,
  thread through to all callees.

### Step 2 — Channel model selection in scripts (~1 hr)

- `run_loopback_noisy.sh`: add `--channel <awgn|cfo|phase|multipath|combined>`
  flag, default `awgn`. Pass to `ofdm_channel_sim.py --channel <type>`.

### Step 3 — Multi-frame loopback test (~3-4 hrs)

- New TB `tb/ofdm_rx_multipkt_tb.cpp`: push N frames back-to-back into
  iq_raw with random gaps between (test sync_detect re-arm).
- Verify each frame's CRC, BER independently.
- Catches sync_detect FSM bugs that single-frame TBs miss (deaf window,
  accumulator reset, n_syms_fb sticky-latch behavior).

### Step 4 — Regression sweep wrapper (~1 hr)

- `run_regression.sh`: nested loops over modcod × nsyms × channel × snr.
- Output: CSV `regression_results.csv` with columns
  `modcod,nsyms,channel,snr,hls_ber,py_ber,header_pass,delta_db`.
- Pass criterion: `delta_db ≤ 0.5` AND `header_pass = 1` for every row
  where Python decodes the data cleanly.
- Plot output: HLS-vs-Python overlaid BER curves per channel/modcod.

### Step 5 — Multipath template parameters (~1 hr)

`ofdm_channel_sim.py:add_multipath` defines the 2-tap channel inline.
Externalize to a config:

```
docs/regression/channel_profiles.yaml:
  - name: rural_2tap
    taps: [(0, 1.0+0j), (1, 0.30+0j)]   # ground reflection -10 dB, 50 ns
  - name: urban_4tap
    taps: [(0, 1.0+0j), (2, 0.50+0j), (5, 0.30+0j), (12, 0.15+0j)]
  - name: aviation_long_delay
    taps: [(0, 1.0+0j), (16, 0.40+0j), (32, 0.20+0j)]
```

This lets `--channel multipath:rural` / `:urban` / `:aviation` select
profiles for different deployment scenarios.

### Step 6 — Cosim subset (~half day, optional)

Pick 4-8 representative test points (worst-case + cliff-edge) and run
`csim_design + cosim_design` against them in Vivado. Catches
HLS-to-RTL synthesis regressions that pure C-sim misses (e.g.,
fixed-point overflow modes, AXIS handshake races).

Suggested subset:
- 16-QAM r=2/3 @ 15 dB, 16 syms, AWGN (cliff edge)
- 16-QAM r=1/2 @ 12 dB, 32 syms, multipath (combined stress)
- QPSK r=1/2 @ 10 dB, 4 syms, combined (small frame edge case)
- 16-QAM r=1/2 @ 20 dB, 255 syms, AWGN (golden case)

## Pass / fail decision rules

For each test point:

| Condition | Verdict |
|-----------|---------|
| HLS BER = 0 AND Python Q15 BER = 0 | PASS |
| HLS BER ≤ Python BER × 1.10 (algorithm cliff region) | PASS |
| HLS BER > Python BER × 1.10 | INVESTIGATE — implementation bug |
| HLS header CRC FAIL when Python header passes | FAIL |
| HLS header CRC FAIL AND Python header FAIL (deep cliff, both broken) | DEFER (not at operating point) |

The 1.10 margin accounts for fixed-point quantisation noise in HLS that
Python at Q15 doesn't fully model (CORDIC angle quantization, pipeline
truncation modes).

## Known gaps deferred to RF bring-up

- Real AD9364 ADC noise (vs synthetic AWGN)
- LO phase noise from actual VCO + PLL
- Group delay variation from analog filters
- ADC clock jitter
- LO leakage / DC offset
- IQ imbalance

These are characterized at L4 (board) once L1-L3 regression is green.

## Effort estimate

| Step | Person-hours |
|------|-------------:|
| 1. Frame-size plumbing | 1 |
| 2. Channel selection in scripts | 1 |
| 3. Multi-frame TB | 4 |
| 4. Regression sweep wrapper | 1 |
| 5. Multipath profiles | 1 |
| 6. Cosim subset | 4 |
| **Total** | **~12 hours** = 2 dev-days |

After this: full regression matrix runs on demand. Real RF bring-up
is the next phase.
