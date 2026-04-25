# RX Chain Gating & Preamble Detection — Design Notes

> **STATUS — IMPLEMENTED.** The free-running architecture described
> below landed in commit `fd24e5b`.  The block previously called
> `sync_cfo` was merged into `sync_detect` (commit `ca6f58f`).  The
> inline CFO derotation that this document discusses was later
> *removed* in commit `d08a537` because it picked up noise at low SNR
> and inflated header-decode phase error — see `RX_LOW_SNR_DEBUG.md`
> for that journey.  Today's RX is:
>   `sync_detect (gate, no CFO) → ofdm_rx (FFT + chan-est + CPE) → fec_rx`.
> All `sync_cfo` references in the prose below should be read as
> `sync_detect` for current code.  Document kept as the architectural
> rationale archive.

## Problem statement (historical, pre-fd24e5b)

The RX chain (sync_cfo → ofdm_rx → fec_rx → ofdm_mac) today is an **open-loop
fixed-sample-count** pipeline triggered once per MAC `ap_start`. Between
packets it happily drains ~864 noise samples from `adc_input_fifo` and
forwards `(2 + n_syms) × 288` noise samples downstream, producing junk
bytes and false header-CRC errors.

What we want instead is a **truly free-running RX chain**:

- Once the FPGA is configured and the AD9364 is streaming, the RX chain
  runs autonomously.
- MAC does **not** issue per-packet `ap_start` pulses to any RX block.
- `sync_cfo` is the gatekeeper: it continuously watches the I/Q stream,
  detects the ZC preamble, and only then opens a gate forwarding samples
  to the rest of the chain.
- Frame length is derived from the in-band BPSK header (decoded by
  `ofdm_rx`) and fed back to the gate so it closes after exactly one
  packet.

MAC only retains optional reset / pause-clock control per block and
pure byte-domain responsibilities (header/FCS/filter/counters).

---

## Reference: what open-source OFDM receivers do

### 1. openwifi / openofdm (jhshi) — monolithic FSM + enable-level gating

Source: https://github.com/jhshi/openofdm (`verilog/dot11.v`)

One top module (`dot11.v`) owns a **13-state master FSM**. Relevant
states:

```
S_WAIT_POWER_TRIGGER ──► S_SYNC_SHORT ──► S_SYNC_LONG ──► S_DECODE_SIGNAL
        ▲                                                       │
        │                                                       ▼
        │                                               S_CHECK_SIGNAL
        │                                                       │
        │ error                                                 ▼
        ├──── S_SIGNAL_ERROR ◄──────┐                    S_DECODE_DATA
        │                           │                           │
        └──── S_DECODE_DONE ◄───────┴────── packet done ◄───────┘
```

- FSM drives explicit enable levels to downstream blocks:
  `sync_long_enable`, `equalizer_enable`, `ofdm_enable`. In
  `S_WAIT_POWER_TRIGGER` all three are 0 → downstream quiescent.
- Two-stage, cheap-then-expensive detection:
  1. **Power trigger** — continuous envelope > threshold comparator.
     One comparator. Rejects ~99% of noise cheaply.
  2. **Short-preamble autocorrelator** — only enabled when power
     trigger asserts. Detects STF pattern.
  3. **Long-preamble cross-correlator** — only enabled after STF
     detected. Does expensive LTF cross-correlation for precise
     symbol boundary.
- Frame length comes from decoded SIGNAL field → FSM stores as
  `pkt_len`, counts bytes in `S_DECODE_DATA`, asserts `fcs_out_strobe`
  at end, returns to `S_WAIT_POWER_TRIGGER`.
- **No feedback wire needed** — the master FSM has visibility into
  every sub-block.

Between packets: `S_WAIT_POWER_TRIGGER`, enables all 0, samples flow
only into the power detector. Everything else idle.

### 2. GNU Radio — tagged streams + demuxer state machine

Schmidl-Cox block is free-running, consumes samples every cycle,
emits two outputs:

- Frequency-corrected sample stream.
- **Trigger byte port** — pulses on detected frame boundaries.

Downstream **Header/Payload Demuxer (HPD)** is a 4-state FSM:

```
TRIGGER ──► HEADER ──► MESSAGE_WAIT ──► PAYLOAD ──► TRIGGER
                            ▲
                            │ PMT msg with packet_len
                            │ (asynchronous, from header decoder)
```

- TRIGGER: search for a `1` in the trigger input; drop everything else.
- HEADER: copy exactly 64 samples (one OFDM symbol) to header output.
- MESSAGE_WAIT: stall until header decoder posts a PMT message with
  `packet_len`.
- PAYLOAD: copy `packet_len` samples to payload output, return to
  TRIGGER.

Length flows via **async feedback** — PMT message, not a wire. GNU
Radio author notes: *"This isn't a real problem if you use a real SDR
that is always collecting samples, but can be hard to debug in a pure,
oneshot simulation."*

### Common pattern

Both designs have:

1. **Free-running synchronizer** consuming samples every cycle, never
   blocked.
2. **Threshold-gated output** — sync emits nothing downstream until a
   real preamble is locked.
3. **Length from header, fed back to the gate** (master FSM in
   openofdm, PMT message in GNU Radio).
4. **Cheap-then-expensive detection cascade** — envelope trigger first,
   correlator only when triggered.

What they do **not** do:
- Per-packet `ap_start` pulses from a supervisor.
- Fixed sample-count forwarding without knowing actual packet length.
- Open-loop blind reads of a noise buffer between packets.

---

## Refined architecture for hallycon ofdm-hls

Pick: **hybrid of openofdm discipline + HLS modularity.** Distributed
state machine per block with one feedback wire, matching the natural
shape of our 3-block RX pipeline.

```
                       ┌────────── sync_cfo (ap_ctrl_none, free-run) ──────────┐
                       │                                                        │
      iq_in ───►  power_trigger ─► sc_correlator ─► state_machine ─► gate ─► iq_out
      (from ADC       (envelope      (continuous     SEARCH /          (writes
       FIFO)           > POW_TH)     metric,         FWD_PREHDR /       only when
                                     SC_TH)          WAIT_NSYMS /       gated)
                                                     FWD_DATA
                                                          ▲
                       ┌──────────────────────────────────┘
                       │ n_syms_fb (ap_vld, wire from ofdm_rx)
                       │
                       └──── fed back from ofdm_rx after header decode
                       │
                       │
                       │          ┌────── ofdm_rx (ap_ctrl_none) ─────────┐
                       │          │                                       │
                       └──────────┤ rx_state_machine: WAIT_PREAMBLE ──►   │
                                  │   PROCESS_PREAMBLE ──► PROCESS_HEADER │
                                  │   ──► EMIT_N_SYMS_FB ──► PROCESS_DATA │
                                  │   ──► WAIT_PREAMBLE                   │
                                  └────────────────────────────────────────┘
                                              │
                                              │ modcod, n_syms (ap_none wires)
                                              ▼
                                  ┌────── fec_rx (ap_ctrl_none) ──────────┐
                                  │ loops bounded by n_syms; naturally    │
                                  │ stalls between packets as stream      │
                                  │ underflows.                           │
                                  └────────────────────────────────────────┘
                                              │
                                              ▼
                                  MAC (byte-domain, air-driven)
```

### sync_cfo FSM states

| State | Action | Transition |
|---|---|---|
| `SEARCH` (post-reset default) | Every cycle: read 1 sample from `iq_in` into 864-sample circular buffer. Update running power envelope. If `power > POW_THRESH`, also update Schmidl-Cox `P(t)` and `R(t)` running sums. Compute metric = `|P|² / R²`. No output. | `metric > SC_THRESH && power > POW_THRESH` → latch `best_t`, `cfo_est`. Go to `FWD_PREHDR`. |
| `FWD_PREHDR` | Emit preamble + header = 576 samples from `best_t` onward, with CFO correction applied inline. | After 576 samples emitted → `WAIT_NSYMS`. |
| `WAIT_NSYMS` | Keep reading samples into circular buffer (do NOT forward). Stall until `n_syms_fb_valid` pulses. | Latch `n_syms_fb` → `FWD_DATA`. |
| `FWD_DATA` | Emit `n_syms × 288` samples from the buffer (accounting for how many arrived during WAIT_NSYMS). | Counter hits 0 → `SEARCH`. |

### ofdm_rx changes

- `#pragma HLS INTERFACE ap_ctrl_none port=return`.
- Internal FSM:
  - `WAIT_PREAMBLE`: read from `iq_in` (will block until sync_cfo opens gate).
  - `PROCESS_PREAMBLE`: run FFT on preamble for channel estimate.
  - `PROCESS_HEADER`: FFT on header, decode BPSK, verify CRC-16, extract `modcod` and `n_syms`.
  - `EMIT_N_SYMS_FB`: pulse `n_syms_fb_valid` with `n_syms_fb = n_syms`. Drives sync_cfo back-channel.
  - `PROCESS_DATA`: for each of `n_syms` data symbols, FFT → equalize → demap → pack bits → write to `bits_out`.
  - Return to `WAIT_PREAMBLE`.
- New outputs: `n_syms_fb` (8-bit) + `n_syms_fb_valid` (1-bit), both `ap_vld`.
- Existing outputs `modcod_out`, `n_syms_out`, `header_err` remain.

### fec_rx changes

- `#pragma HLS INTERFACE ap_ctrl_none port=return`.
- Already takes modcod/n_syms as `ap_none` wires — no signature change.
- Internal loops naturally stop after `n_syms × bytes_per_sym` bytes;
  between packets, input stream stalls → fec_rx idles.

### MAC changes

- **Delete** `rx_arm_phy()` from `do_rx()`. MAC no longer touches any
  RX PHY CSR.
- RX path becomes purely byte-domain: `read_nb()` from `phy_rx_in`,
  parse MAC header, filter, forward to host, update counters.
- TX path unchanged — MAC still drives tx_chain + ofdm_tx per packet
  via m_axi.

### Reset / pause-clock control (secondary ask)

- Expose `rx_reset_mask[2:0]` in MAC CSR:
  - bit 0 → sync_cfo `ap_rst_n`
  - bit 1 → ofdm_rx `ap_rst_n`
  - bit 2 → fec_rx `ap_rst_n`
- Each bit ANDed with global `rst_n` at BD level. Driver can hard-reset
  individual blocks for recovery / bring-up.
- Clock gating not worth the complexity on Artix-7 — skip.

### Smartconnect topology (shrinks)

With no CSRs on sync_cfo / ofdm_rx / fec_rx (optionally retain
stats-only bundles), smartconnect drops from **2 SI × 6 MI** to
**2 SI × 3 MI** (host + MAC m_axi → tx_chain, ofdm_tx, ofdm_mac).
Address map updates accordingly.

Optional: keep stats-only `s_axilite` bundles on sync_cfo (`preamble_count`,
`last_metric_peak`, `last_power`) and ofdm_rx (`header_err_count`,
`crc_pass_count`) for bring-up diagnostics — these don't need
per-packet writes by MAC, only host reads.

---

## Design decisions to finalize before coding

1. **Power trigger (two-stage detection)**: add it as first-gate before
   the Schmidl-Cox correlator? Saves dynamic activity + matches openofdm.
   `POW_THRESH` as an s_axilite CSR on sync_cfo for runtime tuning.
   **Recommendation: yes.**

2. **Inline CFO correction into the new FSM?** The old `cfo_correct`
   sub-block has its own 256-entry sin/cos LUT and 4-DSP complex
   rotator. Current merged `sync_cfo` connects them via an internal
   FIFO. The new FSM can apply CFO correction directly in `FWD_PREHDR`
   and `FWD_DATA` states (only when output gate is open), saving one
   internal FIFO and simplifying the DATAFLOW region.
   **Recommendation: inline.**

3. **Stats-only s_axilite banks** on sync_cfo / ofdm_rx: keep?
   **Recommendation: yes on sync_cfo (bring-up essential), yes on
   ofdm_rx (link diagnostics), no on fec_rx.**

4. **Rename `sync_cfo` to something that signals "gate + CFO"** (e.g.,
   `rx_sync`, `rx_frontend`, `preamble_gate`), or keep the name for BD
   churn minimization?
   **Open — cosmetic choice.**

---

## Implementation scope (estimate)

| File | Change | LOC est |
|---|---|---|
| `src/sync_detect.cpp` | Full rewrite: sliding-window metric, 4-state FSM, power trigger, gated output. Likely absorbs cfo_correct inline. | ~300 |
| `src/sync_detect.h` + `sync_cfo.h` | Add `n_syms_fb_in`, `n_syms_fb_valid_in` ap_none inputs. Add optional stats CSRs. Remove `n_syms` arg. | ~15 |
| `src/ofdm_rx.{h,cpp}` | Internal FSM rework for self-syncing free-run. Add `n_syms_fb` / `n_syms_fb_valid` ap_vld outputs. Wire header decode to drive them. | ~40 |
| `src/fec_rx.{h,cpp}` | Add `ap_ctrl_none`. | ~3 |
| `src/ofdm_mac.{h,cpp}` | Delete `rx_arm_phy()`. Remove PHY CSR writes on RX path. Optional: add `rx_reset_mask` CSR. | -10 / +5 |
| `vivado/create_ofdm_bd.tcl` + `create_project.tcl` | New `n_syms_fb` + valid wire ofdm_rx → sync_cfo. Shrink smartconnect to 2 SI × 3 MI. Reset mask wiring. | ~40 |
| `litex/shell.py` | None (host-visible interface unchanged; MAC CSRs same). | 0 |

Total: ~400 LOC across 7 files, plus BD / LiteX glue.

---

## References

- openwifi-hw deepwiki (OpenOFDM receiver):
  https://deepwiki.com/open-sdr/openwifi-hw/4.5-openofdm-receiver
- openofdm (jhshi) deepwiki:
  https://deepwiki.com/jhshi/openofdm/1-openofdm-overview
- openofdm dot11.v (master FSM source):
  https://github.com/jhshi/openofdm/blob/master/verilog/dot11.v
- OpenOFDM docs — Symbol Alignment / sync_long:
  https://openofdm.readthedocs.io/en/latest/sync_long.html
- OpenOFDM docs — Packet Detection:
  https://openofdm.readthedocs.io/en/latest/detection.html
- GNU Radio wiki — Schmidl & Cox OFDM sync:
  https://wiki.gnuradio.org/index.php/Schmidl_%26_Cox_OFDM_synch.
- GNU Radio wiki — OFDM Receiver:
  https://wiki.gnuradio.org/index.php/OFDM_Receiver
- GNU Radio OFDM deep dive (E. Rong):
  https://esrh.me/posts/2022-07-25-gnuradio-ofdm
- Schmidl & Cox 1997 paper: *"Robust Frequency and Timing
  Synchronization for OFDM"* — foundational reference for the
  autocorrelation / CP-correlation detection approach.

---

## Status — RX GATING REWRITE COMPLETE

Final FSM diagram (matches `src/sync_detect.cpp`):

```
        ┌─────────────────────────────────────────────────────┐
        │  every cycle: read iq_in, write circ_buf, update    │
        │  pow_env (always), update P/R/Rl (only in SEARCH)   │
        └─────────────────────────────────────────────────────┘
                              │
                              ▼
    ┌────────────┐ gate_armed && sc_above && pow_above   ┌─────────────────┐
    │  SEARCH    │ ─────────────────────────────────────►│  FWD_PREHDR     │
    │ (metric    │                                        │  forward 576    │
    │  live,     │                                        │  samples with   │
    │  gate      │                                        │  CFO rotation   │
    │  closed)   │                                        └────────┬────────┘
    └────▲───────┘                                                 │ fwd=0
         │                                                          │
         │ deaf_counter==0                                          ▼
         │ && P/R/Rl reset                              ┌─────────────────┐
         │                                              │  WAIT_NSYMS     │
         │                                              │  gate closed,   │
         │                                              │  stall on       │
         │                                              │  n_syms_fb_vld  │
         │                                              └────────┬────────┘
         │                                                       │
         │                       n_syms_fb == 0 (hdr error) ─────┤
         │                                                       │ n_syms_fb > 0
         │                                                       ▼
         │                                              ┌─────────────────┐
         │ fwd=0 && P/R/Rl reset && deaf=SYNC_NL (288)  │  FWD_DATA       │
         └──────────────────────────────────────────────│  forward         │
                                                         │  n_syms×288     │
                                                         │  samples        │
                                                         └─────────────────┘
```

### Done (in code)
- `ofdm_mac` no longer controls RX PHY. `rx_arm_phy()` deleted. MAC's
  m_axi master address map restricted to TX blocks only (tx_chain +
  ofdm_tx). PHY base addresses for sync_cfo/ofdm_rx/fec_rx removed from
  `ofdm_mac.h`.
- `fec_rx` body wrapped in `while(1)` with per-iteration DATAFLOW.
  Expects ap_start tied high at BD.
- `ofdm_rx` body wrapped in `while(1)`. New `ap_vld` output
  `n_syms_fb` drives the gate-close feedback to sync_cfo; pulsed with
  `n_syms` on good header, 0 on header CRC error. Header CRC-error
  drain loop removed (not needed — sync_cfo gate is the boundary).
- `sync_cfo` signature: `n_syms` s_axilite CSR → `n_syms_fb` ap_none
  wire. sync_cfo still wraps sync_detect + cfo_correct in a DATAFLOW
  region as before — interim.
- `sync_detect` signature updated to take `n_syms_fb` as ap_none
  instead of s_axilite `n_syms`. Internal DSP unchanged so far.
- BD scripts updated:
  - New wire `ofdm_rx.n_syms_fb → sync_cfo.n_syms_fb`.
  - `xlconstant rx_ap_start_hi = 1` drives `ap_start` on sync_cfo,
    ofdm_rx, fec_rx, ofdm_mac. Host/MAC no longer pulse ap_start.
  - Smartconnect topology unchanged (2 SI × 6 MI) — RX block CSRs are
    still reachable by host for stats/debug, but not by MAC's m_axi.

### Additionally done this commit
- `src/sync_detect.cpp` v4: continuous sliding-window 4-state FSM with
  dual-threshold detection (power envelope × normalized SC metric),
  inline CFO rotation, back-pressure-safe `write_nb` output, 4096-deep
  circular buffer, 4096-cycle post-reset warmup, 288-cycle
  inter-packet deaf window.
- `src/sync_cfo.{h,cpp}`: **retired** (kept as tombstone stubs).  BD
  instantiates `sync_detect` directly.
- `src/free_run.h`: new shared header for `FREE_RUN_LOOP_BEGIN/END`
  (csim bounded / synthesis infinite) and `S_AXILITE_STAT_BUNDLE` name.
- `ofdm_rx` / `fec_rx`: pragma flip to `ap_ctrl_none`.  Any
  `s_axilite port=return` replaced with `ap_ctrl_none port=return`;
  existing stats registers on `header_err` moved to bundle `stat`.
- `vivado/create_ofdm_bd.tcl` + `create_project.tcl`: `sync_cfo_0` cell
  renamed to `sync_detect_0`, VLNV `hallycon.in:ofdm:sync_detect:1.0`.
  `rx_ap_start_hi` xlconstant removed; only `mac_ap_start_hi` remains
  (feeds `ofdm_mac.ap_start`).
- Build infrastructure: `tcl/synth_sync_cfo.tcl` replaced with a
  fail-loud tombstone; new `tcl/synth_sync_detect.tcl` builds the new
  top.  `synth_all_ips.sh` / `setup_vitis.sh` / `tcl/export_ip.tcl` /
  root `export_ip.tcl` all reference `sync_detect_proj`/`sync_detect`.

### Verification plan (next)
1. **Block-level HLS co-sim on `sync_detect` alone**, 10k-sample TB:
   - 2000 noise samples (pow_env below threshold, gate stays closed)
   - 1 good packet: preamble + header (valid modcod+n_syms) +
     n_syms×288 samples.  Feedback wire driven by TB.
   - 1 bad packet: preamble + header with CRC-fail bits.  Feedback
     wire asserted with n_syms_fb=0.
   - Expect: first packet emits preamble+header+data, second packet
     emits only preamble+header, then SEARCH resumes after 288-cycle
     deaf window.
2. **Full loopback**: TX side generates a ZC preamble via ofdm_tx,
   RX chain processes.  Observe `preamble_count` increments,
   `header_bad_count` stays at 0 for good packets.

### Known regression risks to probe
- **CRC-fail path**: WAIT_NSYMS → SEARCH transition when
  `n_syms_fb = 0`.  Sneaky because no samples are forwarded and no
  bytes exit fec_rx — failure would manifest as a silent deadlock.
- **Back-pressure during FWD**: if `iq_out.write_nb` ever returns
  false for a sustained period, `fwd_remaining` and `rd_ptr` must
  hold.  Test by starving downstream.
- **Warmup correctness**: first 4096 samples must not be able to
  trigger `FWD_PREHDR`.  Confirm in co-sim by feeding a preamble
  within the first 4096 samples and checking the gate stays closed.

### Optional (not in this commit)
- `rx_reset_mask` CSR in MAC for per-RX-block software reset via
  AND-gate on `ap_rst_n`.  Adds three 1-bit register slots + three
  BD AND-gates.  Useful for bring-up recovery.
- Ratio-thresholded power trigger (noise-floor EMA) — upgrade path
  once absolute-threshold version is characterised on the board.

---

## Session snapshot — where we stand before tools are ready

Recorded so we can pick up exactly where we left off once Vitis HLS
install completes on `/mnt/d/work/2025.2/` (see `setup_vitis.sh`
`XILINX_ROOT` update required).

### Shipped in this session

**PHY block merges (9 HLS IPs → 5, + 1 new MAC)**

| Block | Sources | Clock | Notes |
|---|---|---|---|
| `tx_chain` | scrambler + conv_enc + interleaver | 10 ns | DATAFLOW, 2×128-depth FIFOs |
| `ofdm_tx` | unchanged | 10 ns | 32-bit xfft iface fix, iq32_t struct |
| `sync_detect` | old sync_cfo + cfo_correct merged + rewritten | 10 ns | Free-running 4-state FSM, inline CFO, ap_ctrl_none |
| `ofdm_rx` | unchanged body + wrapper | 10 ns | Now ap_ctrl_none + while(1) free-run |
| `fec_rx` | interleaver(RX) + viterbi_dec v3 + scrambler | 5 ns (200 MHz) | unroll=64 ACS, ap_ctrl_none |
| `ofdm_mac` | new | 10 ns | Header + FCS + filter + TX PHY sequencer via m_axi |

**Air-driven control plane**

- `modcod_t` = 2-bit `{mod[1], rate[0]}` packed into BPSK header.
- `ofdm_rx.modcod_out` / `n_syms_out` / `n_syms_fb` drive `fec_rx`
  (modcod, n_syms as ap_none wires) and `sync_detect` (feedback
  close-gate wire) and `ofdm_mac` (visibility of last RX packet's
  PHY params for future FSM use).
- MAC only writes TX PHY CSRs (tx_chain + ofdm_tx at 0x0000 / 0x1000).
  Never touches RX PHY.
- RX chain is fully self-driven from air; no ap_start from anyone.

**BD infrastructure**

- Two clock domains: `clk` 100 MHz, `clk_fec` 200 MHz.
- Two AXIS clock converters: `fec_cc1` (100→200 before fec_rx),
  `fec_cc2` (200→100 after fec_rx).
- Smartconnect 2 SI × 6 MI: host ctrl_axi + MAC m_axi master as SI;
  tx_chain / ofdm_tx / sync_detect / ofdm_rx / fec_rx / ofdm_mac as MI.
- MAC m_axi address map restricted to {tx_chain, ofdm_tx}; host sees
  all 6 slaves for stats/diagnostics.
- `mac_ap_start_hi` xlconstant on `ofdm_mac.ap_start` only.

**LiteX**

- Stripped 300-line `OFDMLowerMAC` class.
- `OFDMChainWrapper` grew `clk_fec`/`rst_fec_n` ports, TLAST on host
  AXIS endpoints, MAC interrupt pins.
- `_CRG` added 200 MHz `ofdm_fec` clkout.
- MSI interrupt dict extended with `OFDM_MAC_TX_DONE` + `OFDM_MAC_RX_PKT`
  via `PulseSynchronizer`s into sys domain.

**Testbench**

- `tb/sync_detect_tb.cpp` — standalone 12k-sample sequence (noise →
  good pkts → 50-bad-header burst → good pkts → noise).  C-sim
  verifies single-packet happy path; full batch verification requires
  RTL cosim driver (see "Verification plan").

**Build infra**

- `synth_all_ips.sh` STEPS list updated to 6 merged projects.
- `setup_vitis.sh` new cases: `tx_chain_synth`, `sync_detect_synth`,
  `fec_rx_synth`, `ofdm_mac_synth`.
- `tcl/synth_*.tcl` for all 6 IPs + tombstone for retired sync_cfo.
- `tcl/export_ip.tcl` + root `export_ip.tcl` — 6 IP projects.

### Known outstanding items (ordered by urgency)

1. **Vitis install to `/mnt/d/work/2025.2/`** — ~78% complete at last
   check.  Once done, update `setup_vitis.sh:XILINX_ROOT` one-liner.

2. **HLS schedule report on `sync_detect`** — run `vitis_hls -f
   tcl/synth_sync_detect.tcl` first; read II and BRAM port usage.
   - If II=1 closes with 4 BRAM18: ship as-is.
   - If II=1 fails due to BRAM port conflict: duplicate `buf_i`/`buf_q`
     to serve `{new,new_N}` and `{old,old_N}` read pairs separately.
     Cost: 4 → 8 BRAM18 (still trivial at ~7% of Artix-50T BRAM).
   - If II=2 or II=4 at 100 MHz: acceptable for 20 MSPS throughput.

3. **CFO phase-step scaling sanity check** — `TWO24_OVER_2PI = 2670177`
   derivation is documented in `sync_detect.cpp`.  Verify against
   old `cfo_correct.cpp` output via csim: feed a synthetic CFO of
   e.g. ε_sc = 0.1 and check phase_acc advances by exactly
   0.1 × 2^24 = 1,677,721 per sample.  Regression catch for any
   off-by-shift error.

4. **RTL cosim driver for 55-packet batch** — C-sim cannot drive
   `n_syms_fb_vld` cycle-accurately (single-threaded).  Either:
   (a) write a small `tb/sync_detect_cosim_driver.sv` Verilog harness
   that watches `iq_out_tvalid` byte count and pulses `n_syms_fb_vld`
   at the right cycle, or
   (b) accept C-sim single-packet happy-path coverage and defer the
   50-bad-header regression to board bring-up.

5. **P&R timing closure on `fec_rx` @ 200 MHz** — analytical budget
   fits (2.7 ms compute vs 3.67 ms air budget at 16-QAM r=2/3, 255
   syms).  Vivado post-impl timing report has final word.  Fallback
   noted in `viterbi_dec.cpp` header: register the ACS add/compare
   into II=2 stages if II=1 at 5 ns misses.

6. **Bring-up calibration** — `sync_detect.pow_threshold` CSR is
   tunable in `stat` bundle.  Default 0.001 is a TB-friendly value;
   real-world number depends on AD9364 AGC setting and antenna gain.
   Known unknown until on bench.

7. **XDC update** — add:
   ```tcl
   create_clock -period 5 -name clk_fec [get_ports clk_fec]
   set_clock_groups -asynchronous \
       -group [get_clocks clk] \
       -group [get_clocks clk_fec]
   ```
   to `vivado/ofdm_top.xdc` before first P&R run.

8. **Deleted/retired files** — can be removed in a cleanup pass:
   - `src/sync_cfo.{h,cpp}` — tombstone stubs, safe to delete.
   - `src/cfo_correct.{h,cpp}` — orphaned (not referenced by any build);
     sin LUT and rotation math copied into `sync_detect.cpp`.
     Safe to delete but harmless kept as reference.

### Recommended first-bring-up sequence

Once Vitis install completes:

```
# 1. Update setup_vitis.sh XILINX_ROOT
sed -i 's|XILINX_ROOT="$HOME/Xilinx/2025.2"|XILINX_ROOT="/mnt/d/work/2025.2"|' setup_vitis.sh

# 2. Smoke test sync_detect HLS synthesis alone
./setup_vitis.sh sync_detect_synth
#    → Read schedule report; confirm II and BRAM count.
#    → If II>1 or resource issue, fix here before proceeding.

# 3. C-sim sync_detect TB
./setup_vitis.sh sync_detect_csim    # add this case to setup_vitis.sh if not yet

# 4. Full HLS synthesis for all 6 IPs + export
./synth_all_ips.sh

# 5. Vivado BD + synthesis
vivado -mode batch -source vivado/create_ofdm_bd.tcl    # for LiteX flow
# or
vivado -mode batch -source vivado/create_project.tcl     # for full P&R + reports

# 6. If P&R passes:
cd litex && python3 shell.py --build
```

Expected first-iteration failure modes:
- sync_detect II=1 closure — address per item 2 above.
- CFO drift in csim — address per item 3.
- fec_rx 200 MHz timing miss — address per item 5 (II=2 ACS fallback).

All three have documented remedies.  If none hit, we're into
bring-up land.
