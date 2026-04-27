# OFDM Waveform IP — Engineering Specification
## UAV Datalink for Low-Cost SDR Platforms

**Document:** Engineering Specification
**Project:** ofdm-hls
**Target application:** Bidirectional UAV C2 + telemetry datalink on low-cost SDR
**Target FPGA:** Xilinx Artix-7 XC7A50T-2CSG325
**Target RF front-end:** Analog Devices AD9364 (or compatible Zynq/AD936x SDR)
**Host interface:** PCIe (LiteX SoC with LitePCIe DMA)
**Status:** Post-implementation, timing closed, C/RTL co-sim validated (pre-v4); sync_detect v4 (sliding-window FSM) written and pending co-sim.
**Date:** 2026-04-27 (rev. 5 — sync_detect v5 with CFO removed, RX triad ap_ctrl_none, doc resource/throughput reconciled to litex SoC build)

---

## 1. Scope and Purpose

This document specifies the OFDM-based waveform IP that implements the complete PHY + lower-MAC for a half-duplex UAV datalink. The IP is distributed as a set of Vivado HLS cores plus a LiteX SoC shell; it targets low-cost Artix-7 class FPGAs paired with a wide-band zero-IF transceiver (AD9364) to form a compact ground/air datalink supporting command-and-control and sensor-/video-telemetry traffic.

The design priorities are:

1. **Spectral efficiency with robustness against multipath and CFO** — OFDM with preamble-aided channel estimation and pilot-based CPE tracking.
2. **Small FPGA footprint** — fits inside a 50 K-LUT device with headroom for PCIe, RF adapter, and mode-mux glue.
3. **Deterministic, well-bounded latency** for closed-loop C2.
4. **Adaptive link** — two modulations and two FEC rates selectable per-frame.

---

## 2. Waveform Parameters

| Parameter | Value | Notes |
|-----------|-------|-------|
| FFT size | 256 | Xilinx `xfft` v9.1, pipelined streaming, natural order |
| Cyclic prefix | 32 samples (1.6 µs @ 20 MSPS) | 12.5 % overhead |
| Symbol duration | 288 samples (14.4 µs) | FFT + CP |
| Data subcarriers | 200 | Modulated user data |
| Pilot subcarriers | 6 BPSK | Indices {50, 75, 100, 154, 179, 204}, phase-tracking |
| Guard / null subcarriers | 50 | Includes DC (bin 0) and Nyquist (bin 128) |
| Modulations | QPSK, 16-QAM (Gray-coded) | Runtime-selectable per frame |
| FEC | Convolutional K=7, G₀=0o133, G₁=0o171 | Rate 1/2 or 2/3 (802.11a puncture) |
| RF sample rate | 20 MSPS | AD9364 baseband clock |
| Occupied BW | ~15.6 MHz | 200 × (20 MHz / 256) |
| Preamble | Zadoff-Chu, root u=25, N=256 | 1 OFDM symbol, low PAPR |
| Header | BPSK on data subcarriers, CRC-16 protected | modcod (2 b) + n_syms (8 b) + CRC (16 b) |
| Max payload per frame | 4096 bytes | MAC layer limit |
| Raw net throughput (peak) | ≈ 4.17 Mbps | 16-QAM r2/3, 255 symbols/frame |
| Minimum net throughput | ≈ 1.39 Mbps | QPSK r1/2 |

---

## 3. Frame Structure

```
┌─────────────┬──────────────┬─────────────┬──────────────────────────────┐
│ Guard (288) │ Preamble ZC  │ Header BPSK │  Data symbols (N_syms × 288) │
│  zero samp  │  288 samples │  288 samples│  N_syms ∈ [1, 255]           │
└─────────────┴──────────────┴─────────────┴──────────────────────────────┘
```

- **Preamble (ZC, u=25):** constant-envelope OFDM symbol used for Schmidl-Cox timing acquisition, coarse CFO estimate, and one-shot channel estimation.
- **Header:** 26-bit BPSK-coded field carrying `modcod` (2 b), `n_syms` (8 b), and CRC-16. The receiver parses the header on-the-fly and reconfigures demapping and FEC for the payload.
- **Data:** up to 255 OFDM symbols per frame. 200 data subcarriers + 6 pilots per symbol. Pilots enable per-symbol common-phase-error (CPE) correction.

---

## 4. Transmit Signal Chain

```
Host bytes ─► ofdm_mac ─► tx_chain ─► ofdm_tx ─► xfft (IFFT) ─► RF DAC
                           │            │            │
                           │            │            └─ 256-pt, scale_sch=0xAA (÷256)
                           │            └─ unpack, map, pilot/preamble insert, CP add
                           └─ scramble → conv_enc → interleave
```

| Stage | HLS module | Function |
|-------|------------|----------|
| MAC TX | `ofdm_mac` | Prepends 14-byte Ethernet-style header [dst:6][src:6][len:2], appends CRC-32 (poly 0x04C11DB7, init/finalXOR = 0xFFFFFFFF) |
| Scrambler | `tx_chain` | 802.11a 7-stage LFSR, polynomial x⁷+x⁴+1, seed 0x7F |
| FEC encoder | `tx_chain` | K=7 convolutional, rate 1/2 (no puncture) or 2/3 (802.11a puncture matrix) |
| Interleaver | `tx_chain` | 802.11a two-step block interleaver, size N_CBPS = 400 (QPSK) or 800 (16-QAM) |
| Constellation mapper | `ofdm_tx` | Gray-coded QPSK (±0.7071 ± j0.7071) or 16-QAM (1/√10 scaling) |
| Pilot + guard insert | `ofdm_tx` | 6 BPSK pilots, 50 nulls, 200 data bins |
| IFFT | `xfft` v9.1 | 256-pt complex, ap_fixed<16,1>, scaling 0xAA |
| CP insert | `ofdm_tx` | Copies last 32 samples to front → 288-sample symbol |
| Preamble prepend | `ofdm_tx` | Injects ZC preamble + BPSK header ahead of first data symbol |

**TX output stream:** AXI-Stream `iq_t` (I: ap_fixed<16,1>, Q: ap_fixed<16,1>, TLAST), 40-bit TDATA @ 100 MHz.

**Measured TX EVM (C-sim, clean):** 0.056 % (≈ −65 dB) at 16-QAM — consistent with Q0.15 quantisation floor of the ap_fixed<16,1> datapath.

---

## 5. Receive Signal Chain

```
RF ADC ─► adc_input_fifo ─► sync_detect ─► ofdm_rx ─► xfft (FFT) ─► fec_cc1 ─► fec_rx ─► fec_cc2 ─► ofdm_mac ─► host
          4K × 40b            100 MHz        100 MHz                 100→200      200 MHz   200→100     100 MHz
                              (free-running,  (free-running,                       (free-running,
                               ap_ctrl_none)   ap_ctrl_none)                        ap_ctrl_none)
                                   ▲
                                   │ n_syms_fb + n_syms_fb_ap_vld
                                   └────────────────── (from ofdm_rx, per-packet feedback)
```

| Stage | HLS module | Function |
|-------|------------|----------|
| Timing sync | `sync_detect` **v5** | **Free-running 4-state FSM** (`SEARCH → FWD_PREHDR → WAIT_NSYMS → FWD_DATA → SEARCH`) running on a 4096-sample circular buffer with per-cycle sliding-window running sums for P, R, Rl, and pow_env (64-sample window). **Dual-threshold trigger:** AND of (`pow_env > POW_TH`, s_axilite-tunable, CSR-readback `stat_pow_env`) and (`\|P\|² > SC_TH² · R · Rl`, SC_TH² = 0.49 as ap_ufixed<16,0>, cross-multiply — no divide). Back-pressure-safe output via `iq_out.write_nb()`. 4096-sample warmup after reset; 288-sample deaf window on `FWD_DATA → SEARCH` re-entry. Feedback from `ofdm_rx` via `n_syms_fb` (ap_vld) — sticky-latched in WAIT_NSYMS; `0` aborts to SEARCH, `n_syms > 0` transitions to FWD_DATA. |
| CFO correction | — | **Removed in v5.** AD9364 RX-LO calibration leaves sub-ppm residual; per-symbol pilot CPE in `ofdm_rx` absorbs whatever drifts through. Both `sync_cfo` and `cfo_correct` are retired (tombstone stub `src/sync_cfo.h` re-exports `sync_detect.h` for legacy includes). |
| CP removal | `ofdm_rx` | Discards first 32 of every 288 samples |
| FFT | `xfft` v9.1 | 256-pt, scale 0xAA, natural output order |
| Channel estimation | `ofdm_rx` | ZC correlation on preamble → per-SC gain G[k]; pre-computes G_eq = conj(G)/\|G\|² (ap_fixed<32,10>) so runtime equaliser is multiply-only |
| Pilot CPE track | `ofdm_rx` | 6 BPSK pilots → fixed-point CORDIC atan2 (16 iter, < 0.01°) → CPE derotation |
| Equaliser | `ofdm_rx` | Y_eq[k] = Y[k] × G_eq[k] |
| Demapper | `ofdm_rx` | Hard-decision QPSK / 16-QAM → packed bytes (50 B/sym QPSK, 100 B/sym 16-QAM) |
| Header parse | `ofdm_rx` | Decodes BPSK header, validates CRC-16, exposes `modcod_out`, `n_syms_out` (ap_vld) |
| Deinterleaver | `fec_rx` | Inverse 802.11a permutation |
| Viterbi decoder | `fec_rx` | K=7, 64 states, ACS unroll=64, sliding-window traceback WIN_LEN=128 (≥ 5·K), hard decision, II=1 @ 200 MHz |
| Descrambler | `fec_rx` | Identical LFSR seeded 0x7F |
| MAC RX | `ofdm_mac` | CRC-32 verify, optional destination MAC filter, payload to host |

**Clock-domain crossing:** Only the Viterbi needs 200 MHz to meet 16-QAM r2/3 real-time throughput; everything else runs at 100 MHz. Two `axis_clock_converter` instances (`fec_cc1`: 100→200, `fec_cc2`: 200→100) span the boundary.

---

## 6. FEC Details

| Attribute | Value |
|-----------|-------|
| Code | Convolutional, K=7, rate-1/2 mother code |
| Generator polynomials | G₀ = 0o133 (0x5B), G₁ = 0o171 (0x79) — 802.11a-compatible |
| Puncturing | None (r = 1/2), or 802.11a matrix [[1,1],[1,0]] (r = 2/3) |
| Viterbi architecture | 64-state sliding-window, ACS fully unrolled (64 butterflies/cycle) |
| Traceback depth | 128 bits (WIN_LEN) — ≥ 5 × K |
| Decision type | Hard (single-bit per symbol from demapper) |
| Clock | 200 MHz |
| Decode latency (255-sym 16-QAM r2/3) | ≈ 2.7 ms (meets 3.67 ms frame time at 20 MSPS) |

A soft-decision upgrade path is available (demapper LLR outputs) but not enabled in the current build; switching to soft decision is expected to recover 2 – 3 dB at high SNR.

---

## 7. MAC Layer

| Feature | Value |
|---------|-------|
| Frame format | `[dst_mac(6)][src_mac(6)][len_be16(2)][payload(N)][CRC32_be(4)]` |
| `len` field | Carries payload length only (not including 14 B header or 4 B FCS). Deliberate divergence from Ethernet `ethertype` — used for RX packet-boundary detection. |
| CRC-32 | Poly 0x04C11DB7, init = 0xFFFFFFFF, final-XOR = 0xFFFFFFFF, transmitted big-endian (standard Ethernet FCS) |
| Max payload | 4096 bytes (`MAC_MAX_PAYLOAD`); max on-air frame 4114 bytes |
| Addressing | 48-bit `my_mac_addr` CSR + destination-MAC filter; `promisc` bit overrides filter |
| Enable | `mac_enable` gates both TX and RX paths |
| Modcod selection | Per-frame TX modcod via MAC `s_axilite`; MAC programs TX PHY CSRs (tx_chain, ofdm_tx) through its own `m_axi` master. RX modcod/n_syms are decoded from the air header by `ofdm_rx` and fed directly as ap_none wires to `fec_rx` (no MAC-mediated programming). |
| Duplexing | Half-duplex, single in-flight frame (one packet per `ap_start`). Upgrade path: split into `ofdm_mac_tx` / `ofdm_mac_rx` with independent m_axi masters. |
| Execution | `ap_ctrl_hs` top; LiteX shim ties `ap_start = ~ap_done` for self-retrigger. Non-blocking probe of host TX → PHY RX each iteration. |
| Observability | 5 stats counters, last-RX header snapshot registers, 1-cycle interrupt pulses (`tx_done_pulse`, `rx_pkt_pulse`) for host ISR. |
| Host interface | LiteX / LitePCIe AXI-Stream (8 b + TLAST, 100 MHz) both directions — via LiteX AsyncFIFO + width converter. |

---

## 8. Interfaces

**AXI-Stream (internal):**
- Host TX/RX: 8-bit + TLAST @ 100 MHz (LitePCIe → LiteX AsyncFIFO/width-conv → `ofdm_mac`)
- RF-facing baseband I/Q (`iq_t`): 40-bit TDATA (I[15:0] + Q[15:0] + 1-bit TLAST, padded) @ 100 MHz — carries DAC/ADC packet boundary
- xfft-facing baseband I/Q (`iq32_t`): **32-bit TDATA (I[15:0] + Q[15:0], NO TLAST)** @ 100 MHz. The external xfft v9.1 is configured with fixed frame length (FFT_SIZE = 256) and generates its own output TLAST, so HLS does not drive one in. This is the interface between `ofdm_tx` / `ofdm_rx` and the Xilinx xfft IPs in the BD.
- PHY byte streams (tx_chain↔ofdm_mac, fec_rx↔ofdm_mac): plain 8-bit `ap_uint<8>` streams (no AXIS TLAST required — packet length carried in CSR)
- Inter-block FEC (100 ↔ 200 MHz boundary): 8-bit + TLAST `axis_byte_t` via two `axis_clock_converter` instances (`fec_cc1`: 100→200, `fec_cc2`: 200→100)

**AXI4-Lite register map** (4 KB per block, smartconnect 2-SI × 6-MI fabric):

| Base | Block | Access | Control protocol | Key registers / ports |
|------|-------|--------|-----------------|----------------------|
| 0x0000 | `tx_chain` | MAC m_axi + host | `ap_ctrl_hs`, `ctrl` bundle | AP_CTRL, `n_data_bytes` (16-bit, +0x10), `modcod` (2-bit, +0x18), `n_syms` (8-bit, +0x20) |
| 0x1000 | `ofdm_tx`  | MAC m_axi + host | `ap_ctrl_hs`, `ctrl` bundle | AP_CTRL, `modcod` (+0x10), `n_syms` (+0x18) |
| (host stats only) | `sync_detect` | host readback only | **`ap_ctrl_none`** — no `ap_start`, no `ap_done`. Free-running. | s_axilite on `stat` bundle (no `ctrl` bundle, no `return` register). Inputs: `n_syms_fb` + `n_syms_fb_ap_vld` (ap_none wires driven by `ofdm_rx`), `pow_threshold` (s_axilite). Stats readback: `stat_preamble_count`, `stat_header_bad_count`, `stat_pow_env`. |
| (host stats only) | `ofdm_rx`  | host readback only | **`ap_ctrl_none`** — free-running `FREE_RUN_LOOP_BEGIN/END` wrap. | Outputs: `modcod_out` (ap_vld), `n_syms_out` (ap_vld), `n_syms_fb` (ap_vld — feedback to `sync_detect`). `header_err` moved to the `stat` s_axilite bundle. |
| (host stats only) | `fec_rx`   | host readback only | **`ap_ctrl_none`** — free-running `FREE_RUN_LOOP_BEGIN/END` wrap with per-iteration `#pragma HLS DATAFLOW`. | `modcod` and `n_syms` are ap_none wires driven from `ofdm_rx`. `n_data_bytes` derived internally. Optional stats on `stat` bundle. |
| 0x5000 | `ofdm_mac` | Host only | `ap_ctrl_hs` with `ap_start = ~ap_done` self-retrigger via `mac_ap_start_hi` xlconstant. | `my_mac_addr` (48-bit), `promisc`, `mac_enable`, TX-side `modcod`, stats counters, last-RX-header snapshot, interrupt pulses. |

**Control-plane key facts:**
- `m_axi_csr_master` on `ofdm_mac` covers **only `tx_chain` (0x0000) + `ofdm_tx` (0x1000)**. `PHY_BASE_SYNC_CFO` / `PHY_BASE_OFDM_RX` / `PHY_BASE_FEC_RX` and the `rx_arm_phy()` helper are deleted.
- The RX triad (`sync_detect`, `ofdm_rx`, `fec_rx`) is `ap_ctrl_none`: **no `ap_start` pins exist**. The former `rx_ap_start_hi` xlconstant and its four connections are deleted from `vivado/create_ofdm_bd.tcl`. Only `mac_ap_start_hi` remains (for `ofdm_mac`).
- `sync_cfo` is retired. `src/sync_cfo.{h,cpp}` are tombstone stubs (`sync_cfo.h` just re-exports `sync_detect.h` for any lingering includes); `tcl/synth_sync_cfo.tcl` fail-louds if invoked; `tcl/synth_sync_detect.tcl` replaces it in the build. BD cell `sync_cfo_0` renamed to `sync_detect_0` (VLNV `hallycon.in:ofdm:sync_detect:1.0`). `synth_all_ips.sh` STEPS list and `setup_vitis.sh` dispatch updated (`sync_cfo_synth` → `sync_detect_synth`). Root `export_ip.tcl` and `tcl/export_ip.tcl` renamed `sync_cfo_proj` → `sync_detect_proj`.
- A new `src/free_run.h` centralises `FREE_RUN_LOOP_BEGIN` / `FREE_RUN_LOOP_END` (infinite under `__SYNTHESIS__`, bounded by `FREE_RUN_ITERS = 200000` default in csim, overridable per-TB via `-DFREE_RUN_ITERS=…`) and the canonical `S_AXILITE_STAT_BUNDLE = stat` name used across the RX triad.

**FIFOs:** `adc_input_fifo` 4 K × 40 b; `rx_output_fifo` 32 K × 8 b.

---

## 9. SoC Integration (LiteX + Vivado)

- Block diagram built by `vivado/create_ofdm_bd.tcl`; wrapper instantiated opaquely in `litex/shell.py` as a single `Instance("ofdm_chain_wrapper")` (Path-B integration — no per-IP Migen Instances).
- LiteX provides S7PCIEPHY + LitePCIeDMA for host traffic and the AD9364 adapter core for the RF interface; **no soft CPU** is present in the SoC — the HLS `ofdm_mac` replaces the former LiteX `OFDMLowerMAC` gateware sequencer.
- Two clock domains (`ofdm` = 100 MHz, `ofdm_fec` = 200 MHz) are generated by the platform PLL.
- **Control-plane split (TX vs RX):**
  - **TX path** is host/MAC-programmed. `ofdm_mac`'s `m_axi_csr_master` drives `tx_chain` + `ofdm_tx` s_axilite slaves through the smartconnect. The host sets modcod / MAC CSRs; the MAC programs the per-frame TX parameters and pulses `ap_start` on the TX blocks.
  - **RX path is entirely free-running.** `sync_detect`, `ofdm_rx`, and `fec_rx` are all `ap_ctrl_none` (no `ap_start` pin). They auto-start from reset. The MAC has **zero** write access to RX PHY CSRs.
  - Per-packet RX sequencing is done entirely in hardware:
    `ofdm_rx` decodes the BPSK header → drives `modcod_out` / `n_syms_out` as ap_none wires into `fec_rx`, and pulses `n_syms_fb` (ap_vld) back to `sync_detect` (n_syms on good header → forward that many data symbols; 0 on header-CRC error → return to SEARCH cleanly, no drain loop needed).
- Top-level free-running bodies:
  - `sync_detect`: `FREE_RUN: while(1) { … }`
  - `ofdm_rx`:     `FREE_RUN: while(1) { … }`
  - `fec_rx`:      `FREE_RUN: while(1) { #pragma HLS DATAFLOW … }`
- Merged HLS IPs (reduce Vivado BD IP count):
  - `tx_chain`    = scrambler + conv_enc + interleaver (one DATAFLOW block)
  - `sync_detect` **v5** = sliding-window FSM, no CFO (CFO and `cfo_correct.cpp` removed from build)
  - `fec_rx`      = deinterleaver + viterbi_dec + descrambler (one DATAFLOW block, 200 MHz)
  - `ofdm_mac`    = MAC + TX-path PHY sequencer (one ap_ctrl_hs block)

---

## 10. Resource Utilisation (Post-Implementation, XC7A50T)

Full SoC build (LiteX + PCIe + AD9361 wrapper + OFDM PHY chain), routed and
bitstream-generated. Source: `litex/build/.../utilization_place.rpt`.

| Resource | Used | Available | % |
|----------|------|-----------|----|
| Slice LUT     | 25 913 | 32 600 | **79.5 %** |
| Slice FF      | 32 450 | 65 200 | 49.8 % |
| DSP48E1       |     81 |    120 | **67.5 %** |
| BRAM tile     |     67 |     75 | **89.3 %** *(tightest)* |
| Bonded IOB    |     57 |    150 | 38.0 % |

All paths meet timing at 100 MHz (main) and 200 MHz (FEC) with ≈ 10 % slack
margin in Vivado 2025.2. BRAM is the binding resource and sets the practical
ceiling for future features (additional buffering, ARQ retry queue, larger
sync window).

---

## 11. Performance Summary

**Throughput (255 symbols/frame, 20 MSPS, continuous TX):**

PHY-layer gross = `200 SCs × bps × code_rate × 69,444 sym/sec × 99.2%` (preamble+header overhead).
Net = PHY − 10–15 % MAC + framing overhead.

| Modcod         | PHY (gross) | Net (after MAC)   |
|----------------|-------------|-------------------|
| QPSK   r=1/2   | 13.8 Mbps   | 11.7 – 12.4 Mbps  |
| QPSK   r=2/3   | 18.4 Mbps   | 15.6 – 16.5 Mbps  |
| 16-QAM r=1/2   | 27.6 Mbps   | 23.4 – 24.8 Mbps  |
| **16-QAM r=2/3** | **36.7 Mbps** | **31 – 33 Mbps** |

**Latency:**
- TX: ~5 – 10 µs (preamble + header + IFFT pipeline)
- RX: ~20 – 30 µs (sync search 864 samples + CFO rotate + FFT + equalise)
- Frame-to-frame: ~3.67 ms at 255-symbol maximum

**BER / SNR operating points (simulation):**
- QPSK r1/2: usable ≥ 15 dB AWGN, ≥ 20 dB with multipath
- 16-QAM r2/3: usable ≥ 20 dB AWGN, ≥ 25 dB worst case
- 4 – 7 dB implementation loss vs. theory (ap_fixed<16,1> quantisation, preamble-only CE, hard-decision Viterbi)

---

## 12. Verification

- **Level 1 — C-sim:** Algorithmic match to Python floating-point reference (`sim/ofdm_reference.py`). Measured EVM −65 dB.
- **Level 2 — RTL co-sim:** Per-IP Vivado HLS cosim; EVM/BER match to C-sim.
- **Level 3 — Full-chain loopback:** `run_loopback.sh`, `run_loopback_noisy.sh` drive TX→channel→RX and verify byte equality / BER.
- **Level 4 — BER sweep:** `run_ber_sweep.sh` sweeps SNR ∈ [5, 25] dB across AWGN, phase-noise, 2-tap multipath, and combined channels for all four modcods; results plotted by `sim/plot_ber.py`.

Reference testbenches: `tb/ofdm_tx_tb.cpp`, `tb/ofdm_rx_tb.cpp`.

---

## 13. UAV Deployment Considerations

- **Link budget:** 15.6 MHz occupied BW + QPSK r1/2 gives a ~20 dB SNR margin at typical 200 mW, 2.4-GHz class radios for line-of-sight ranges of several km; sufficient for telemetry + C2. 16-QAM r2/3 trades ~5 dB sensitivity for 3× the throughput, usable for short-range HD-video bursts.
- **Adaptivity:** modcod is selectable per frame, enabling a simple outer-loop rate adaptation driven by the MAC based on CRC-pass rate.
- **CFO tolerance:** ±0.5 subcarrier (≈ ±39 kHz at 20 MSPS / 256). This is comfortably inside the frequency error of typical TCXO-grade AD9364 boards; no AFC servo is required on top of the per-frame estimate.
- **Doppler:** At 2.4 GHz and a 40 m/s UAV, max Doppler ≈ 320 Hz — well below one subcarrier (78 kHz), fully absorbed by the preamble CFO estimate and pilot CPE tracking.
- **Half-duplex timing:** A single in-flight frame and a 3.67 ms worst-case symbol time give a conservative TDD round-trip budget; ARQ should therefore be implemented in the host stack, not inside the MAC.
- **Regulatory:** ~15.6 MHz bandwidth fits 20 MHz ISM channels (2.4 GHz, 5.8 GHz); spectral mask shaping (windowing / filtering) is left to the RF front-end / PA stage.

---

## 14. Known Limitations and Roadmap

- **No CFO correction in HLS chain** (yet). Removed in `sync_detect` v5 — HLS relies on AD9364 sub-ppm RX-LO + per-symbol pilot CPE. Python reference (`sim/ofdm_reference.py:sync_detect_reference` + the CFO derotator in `decode_full`) now has Schmidl-Cox preamble CFO estimate + time-domain derotator implemented and verified — at CFO = 0.13 SC injection (Ku-band Doppler / ±2 ppm TCXO regime), the chain recovers the cliff to within frame-variance of the CFO=0 baseline. HLS port pending: separate `cfo_correct` block behind a CSR enable, algorithm spec from the Python reference.
- **Hard-decision Viterbi** in HLS — soft-decision upgrade pending (+2–3 dB). Python reference (`sim/ofdm_reference.py:viterbi_decode_soft`) measured the gain; HLS port to use Xilinx Viterbi v9.1 IP rather than hand-rolled.
- **One-shot (preamble-only) channel estimate.** Python reference now has DFT-based MMSE smoothing (`_smooth_channel_dft`, ~2 dB win measured); HLS port pending.
- **Header has CRC-16 only, no FEC.** Single bit flip voids the frame. Planned: rate-1/3 K=7 conv on the 26-bit header (~50 LUT, +3 dB header CRC pass-rate).
- **Half-duplex MAC only**, single in-flight frame, no ARQ, no QoS, no priority queues. Roadmap: split into `ofdm_mac_tx` / `ofdm_mac_rx`, two priority classes, selective ARQ on high-priority class, ACM closed loop.
- **No AES / link-layer encryption** — to be added in the host or as a bump-in-the-wire block ahead of `ofdm_mac`.
- **Output pulse-shaping / spectral-mask windowing** is deferred to RF-side logic.

---

## 15. File Map (Source of Truth)

| Area | Files |
|------|-------|
| TX front-end | `src/tx_chain.{cpp,h}`, `src/scrambler.*`, `src/conv_enc.cpp`, `src/interleaver.*` |
| TX OFDM | `src/ofdm_tx.{cpp,h}` |
| Sync (no CFO) | `src/sync_detect.{cpp,h}` (v5, no CFO). `src/sync_cfo.{cpp,h}` retained as tombstone stub re-exporting `sync_detect.h`; `src/cfo_correct.{cpp,h}` no longer in build. `src/free_run.h` — shared `FREE_RUN_LOOP_*` macros and `S_AXILITE_STAT_BUNDLE`. |
| RX OFDM | `src/ofdm_rx.{cpp,h}` |
| RX FEC  | `src/fec_rx.{cpp,h}`, `src/viterbi_dec.cpp`, `src/conv_fec.h` |
| MAC | `src/ofdm_mac.{cpp,h}` |
| SoC integration | `vivado/create_ofdm_bd.tcl`, `vivado/create_project.tcl`, `litex/shell.py` |
| Build / export | `tcl/export_ip.tcl`, `tcl/synth_*.tcl`, `synth_all_ips.sh`, `setup_vitis.sh` |
| Verification | `tb/ofdm_tx_tb.cpp`, `tb/ofdm_rx_tb.cpp`, `run_loopback*.sh`, `run_ber_sweep.sh`, `sim/` |

---

*End of specification.*
