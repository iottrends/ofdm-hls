# OFDM-HLS → Hallycon M.2 SDR Integration Guide

**Target:** XC7A50T-2CSG325I + AD9364 on Hallycon Artix-SDR M.2 B-key v1.0  
**Date:** 2026-04-09

---

## 1. Architecture Overview

```
                         ┌─────────────────────────────────────────────────────────┐
                         │                  XC7A50T  FPGA                          │
                         │                                                         │
  Host CPU (PCIe)        │   ┌──────────┐    ┌───────────┐    ┌──────────────┐    │    AD9364
  ◄═══════════════►      │   │ LitePCIe │    │ LiteX SoC │    │ ADI          │    │   LVDS
  PCIe x2 Gen2          │   │ DMA      │◄══►│ Wishbone   │◄══►│ axi_ad9364   │◄══►│   12-bit I/Q
                         │   │ Engine   │    │ + CSRs     │    │ IP           │    │   @ 20 MSPS
                         │   └────┬─────┘    └─────┬──────┘    └──────┬───────┘    │
                         │        │                │                   │            │
                         │        │  AXI-Stream    │  AXI-Lite         │ AXI-Stream │
                         │        ▼                ▼                   ▼            │
                         │   ┌─────────────────────────────────────────────┐        │
                         │   │              OFDM Subsystem                 │        │
                         │   │                                             │        │
                         │   │  TX path:                                   │        │
                         │   │  PCIe DMA ──► scrambler ──► conv_enc ──►    │        │
                         │   │  interleaver ──► ofdm_tx ──► [12b shim]──► AD9364   │
                         │   │                                             │        │
                         │   │  RX path:                                   │        │
                         │   │  AD9364 ──► [12b shim] ──► sync_detect ──►  │        │
                         │   │  cfo_correct ──► ofdm_rx ──► deinterleaver  │        │
                         │   │  ──► viterbi_dec ──► descrambler ──► DMA    │        │
                         │   │                                             │        │
                         │   │  CSRs: bypass, tx_enable, rx_enable,        │        │
                         │   │        mod, n_syms, status, frame_count     │        │
                         │   └─────────────────────────────────────────────┘        │
                         │                                                         │
                         └─────────────────────────────────────────────────────────┘
```

---

## 2. HLS IP Packaging (Vivado Flow)

### 2a. Export from Vitis HLS

Each HLS block needs to be exported as a Vivado IP with AXI-Stream and AXI-Lite interfaces. The blocks already have the correct pragmas (`#pragma HLS INTERFACE axis`, `#pragma HLS INTERFACE s_axilite`).

**Export procedure (per block):**

```tcl
# In Vitis HLS 2025.2
open_project ofdm_tx_proj
set_top ofdm_tx
add_files ofdm_tx.cpp
add_files ofdm_tx.h
open_solution "solution1" -flow_target vivado
set_part {xc7a50tcsg325-2}
create_clock -period 10 -name default    ;# 100 MHz system clock
csynth_design
export_design -format ip_catalog -output ./ip_repo/ofdm_tx
```

Repeat for: `ofdm_rx`, `sync_detect`, `cfo_correct`, `scrambler`, `interleaver`, `conv_enc`, `viterbi_dec`.

### 2b. IP Catalog Structure

```
ip_repo/
├── ofdm_tx/          # axis(bits_in, iq_out) + s_axilite(ctrl: mod, n_syms)
├── ofdm_rx/          # axis(iq_in, bits_out) + s_axilite(ctrl: header_err)
├── sync_detect/      # axis(iq_in, iq_out) + s_axilite(ctrl: cfo_est, n_syms)
├── cfo_correct/      # axis(iq_in, iq_out) + s_axilite(ctrl: cfo_est, n_syms)
├── scrambler/        # axis(data_in, data_out) + s_axilite(ctrl: n_bytes)
├── interleaver/      # axis(data_in, data_out) + s_axilite(ctrl: mod, n_syms, is_rx)
├── conv_enc/         # axis(data_in, coded_out) + s_axilite(ctrl: rate, n_data_bytes)
└── viterbi_dec/      # axis(coded_in, data_out) + s_axilite(ctrl: rate, n_data_bytes)
```

### 2c. Critical: Sample Width Adapter

AD9364 outputs 12-bit signed I/Q. The ADI `axi_ad9364` IP sign-extends to 16-bit. Your HLS blocks use `ap_fixed<16,1>` (Q0.15: sign + 15 fractional bits).

The ADI IP outputs Q1.11 (1 sign + 1 integer + 11 fractional + 3 zero-padded LSBs when sign-extended to 16 bits). Your blocks expect Q0.15.

**Adapter needed at RX input:**
```
ADI 16-bit (Q1.11 sign-extended) → right-shift 1 → Q0.15 (ap_fixed<16,1>)
```

This is a 1-bit arithmetic right shift — one line of Verilog or a Migen `>>` operation. Without it you get 6 dB signal level error and potential overflow in the FFT.

**Adapter needed at TX output:**
```
Q0.15 (ap_fixed<16,1>) → left-shift 1 → Q1.11 → truncate to 12-bit for AD9364
```

Or configure the ADI IP to accept Q0.15 directly if your `ad9361_phy` driver version supports it. Check the `axi_ad9364` DATA_FORMAT register.

---

## 3. LiteX/Migen Integration Wrapper

The wrapper file below connects HLS IP blocks into the LiteX SoC streaming path. It replaces the direct AD9364→PCIe path with AD9364→OFDM→PCIe.

See: `ofdm_subsystem.py` (generated alongside this document)

Key design decisions:

1. **Bypass mode** — CSR bit routes AD9364 directly to PCIe DMA, bypassing OFDM entirely. Essential for board bring-up (verify AD9364 produces sane IQ before adding OFDM complexity).

2. **TX/RX separate invocations** — The HLS blocks are "call-return" style (ap_ctrl_hs protocol): CPU writes CSRs, asserts `ap_start`, waits for `ap_done`. This maps naturally to CSR-triggered packet TX/RX.

3. **Clock domain** — All HLS blocks run in `sys` domain (100 MHz from PLL). AD9364 data clock (20 MHz LVDS) is in the `rfic` domain. CDC FIFOs at the boundary are provided by the ADI `axi_ad9364` IP.

4. **Chaining** — TX chain: `scrambler → conv_enc → interleaver → ofdm_tx` connected via AXI-Stream FIFOs. RX chain: `sync_detect → cfo_correct → ofdm_rx → interleaver(is_rx=1) → viterbi_dec → scrambler`. The CPU orchestrates by writing CSRs to each block and asserting start in sequence (or a small FSM chains ap_done→ap_start automatically).

---

## 4. Resource Budget (Updated)

Post-synthesis numbers from the OFDM_HLS_ANALYSIS.md, plus LiteX baseline:

| Block              | LUT    | FF     | DSP | BRAM18 |
|--------------------|--------|--------|-----|--------|
| OFDM TX            | 16,387 | 16,828 | 18  | 28     |
| OFDM RX            | 19,022 | 29,174 | 69  | 31     |
| Scrambler (×2)     | ~40    | ~14    | 0   | 0      |
| Conv encoder       | ~200   | ~100   | 0   | 0      |
| Viterbi decoder    | ~800   | ~500   | 0   | 2      |
| Interleaver (×2)   | ~400   | ~200   | 0   | 0      |
| Sync detect        | ~500   | ~300   | 4   | 4      |
| CFO correct        | ~3,000 | ~2,000 | 25  | 0      |
| **OFDM subtotal**  | ~40,349| ~49,116| 116 | 65     |
| LiteX+PCIe+AD9364  | ~4,000 | ~3,000 | 12  | 6      |
| **Grand total**    | ~44,349| ~52,116| 128 | 71     |
| **XC7A50T avail**  | 32,600 | 65,200 | 120 | 150    |
| **Utilization**    | **136%** | 80%  |**107%**| 47% |

### The Problem: LUT and DSP Overflow

Combined TX+RX exceeds the 50T. Two paths forward:

**Option A — Uni-directional per chip (recommended for V1 drone deployment):**
- Drone: TX-only FPGA (16,387 LUT, 18 DSP) + sync_detect stub → **fits easily**
- Ground: RX-only FPGA (19,022 LUT, 69 DSP) + CFO correct → **fits with margin**
- This is the natural topology for a UAV video downlink anyway

**Option B — Shared FFT (if bidirectional needed):**
- TX and RX share one pipelined FFT IP instance via time-multiplexing
- Saves ~40 DSPs and ~5,000 LUT
- Requires a mux/demux FSM and careful scheduling
- Achievable since TX and RX don't operate simultaneously in TDD

**Option C — Replace `hls::sincosf` in cfo_correct:**
- Float CORDIC → ~25 DSP, ~3,000 LUT
- Fixed-point quarter-wave LUT CORDIC → ~4 DSP, ~400 LUT
- Saves ~21 DSP and ~2,600 LUT — may be enough to fit

**Recommended V1 approach: Option A + Option C.** TX on drone, RX on ground. Replace float sincosf. This gives clear margin on both ends.

---

## 5. Hardware Bring-Up Sequence

### Phase 0: Board Alive (no OFDM)

1. Load LiteX SoC with bypass=1 (OFDM disabled)
2. Verify PCIe enumeration on host
3. Verify AD9364 SPI access (read CHIP_ID register = 0x0A)
4. Configure AD9364 for 20 MSPS, 2.4 GHz center, LVDS DDR mode
5. Verify raw IQ loopback: TX test tone → cable → RX → PCIe DMA → host → check spectrum

### Phase 1: TX-Only (drone end)

1. Load OFDM TX IP
2. Host pushes test payload via PCIe DMA → scrambler → FEC → interleaver → ofdm_tx → AD9364
3. Capture TX output with external SDR (RTL-SDR, HackRF, or another AD9364)
4. Verify with Python reference: `ofdm_reference.py --compare` on captured IQ

### Phase 2: RX-Only (ground end)

1. Load OFDM RX IP + sync_detect + cfo_correct
2. Feed TX output from Phase 1 into RX FPGA via cable (no RF yet)
3. Verify sync lock, header decode, BER=0 on clean channel
4. Add attenuator, verify graceful degradation matches BER sweep

### Phase 3: Over-the-Air

1. Connect TX to antenna on drone
2. Connect RX to antenna on ground
3. Start with short range (1m), increase
4. Monitor header_err rate, BER, CFO estimate
5. Tune CFO threshold if needed (currently 0.01 SC)

---

## 6. Host Software Stack

```
┌─────────────────────────────┐
│  MAVLink Application        │   (mavlink-routerd, QGroundControl)
├─────────────────────────────┤
│  hallycon_drv.c (NIC)       │   Your custom driver: MAVLink priority,
│                             │   RS FEC, RSSI-based MCS adaptation
├─────────────────────────────┤
│  LitePCIe kernel driver     │   DMA engine, interrupt handling
├─────────────────────────────┤
│  PCIe x2 Gen2               │   ~1 GB/s theoretical, ~500 MB/s practical
└─────────────────────────────┘
```

The `hallycon_drv.c` (v0.2) maps naturally here: the NIC driver pushes/pulls raw payload bytes via the PCIe DMA. The OFDM PHY is transparent — the host just sees "bytes in, bytes out" with the FPGA handling scrambler/FEC/OFDM/sync/equalization.

CSR access for runtime MCS changes (mod, rate) goes through the LitePCIe CSR BAR — the driver writes `mod` and `n_syms` before each packet TX.

---

## 7. File Checklist

| File | Status | Notes |
|------|--------|-------|
| `hallycon_m2sdr_platform.py` | ✓ Done | Pin maps verified, needs final schematic cross-check |
| `ofdm_subsystem.py` | **Generated** | Migen wrapper, see below |
| `ofdm_tx.cpp` | ✓ Done | Synth-verified |
| `ofdm_rx.cpp` | ✓ Done | Synth-verified |
| `sync_detect.cpp` | ✓ Done | C-sim verified |
| `cfo_correct.cpp` | ✓ Done | C-sim verified; needs sincosf→fixed CORDIC |
| `conv_enc.cpp` | ✓ Done | C-sim verified |
| `viterbi_dec.cpp` | ✓ Done | C-sim verified |
| `scrambler.cpp` | ✓ Done | C-sim verified |
| `interleaver.cpp` | ✓ Done | C-sim verified |
| `sample_width_adapter.v` | **TODO** | Q1.11 ↔ Q0.15 shim |
| `cfo_cordic_fixed.cpp` | **TODO** | Replace hls::sincosf |
| `hallycon_drv.c` | v0.2 | Needs PCIe DMA integration |
| Vivado block design | **TODO** | IP Integrator .tcl |
| Vitis HLS export .tcl | **Generated** | See below |
