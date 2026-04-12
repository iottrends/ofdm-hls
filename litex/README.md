# LiteX integration — Hallycon M2 SDR + OFDM HLS (Path B)

CPU-less LiteX SoC wrapping the **existing Vivado block design** for the OFDM
chain (`ofdm_chain_wrapper`) on XC7A50T.  LiteX provides PCIe Gen2 x2 +
LitePCIe DMA + AD9364 LVDS PHY; the OFDM chain is imported as flat Verilog
sources (HLS IP RTL) plus Xilinx IP-flow IPs (xfft, smartconnect) and spliced
between the DMA and the RFIC.

**Status: bitstream built and verified** (2026-04-12).  Synthesis, placement,
routing, and bitstream generation all pass with zero errors and timing met on
all paths.

```
                                    LiteX SoC (hallycon_m2sdr_platform)
 ┌──────────────────────────────────────────────────────────────────────────────────────────┐
 │                                                                                          │
 │  Host PC                                        sys clock domain (125 MHz)               │
 │  ════════                                                                                │
 │  ┌──────────┐  PCIe   ┌───────────┐  AXI-S   ┌──────────────┐  AXI-S   ┌────────────┐  │
 │  │          │  Gen2x2  │ S7PCIEPHY │ 64-bit   │ LitePCIeDMA  │ 64-bit   │ Converter  │  │
 │  │  litepcie│◄────────►│           │◄────────►│              │─────────►│  64b → 8b  │  │
 │  │  driver  │         │           │          │  reader/     │          │            │  │
 │  │          │         └───────────┘          │  writer      │◄─────────│ Converter  │  │
 │  └──────────┘                                └──────────────┘  64-bit   │  8b → 64b  │  │
 │       │                                             │                   └─────┬──────┘  │
 │       │ CSR (BAR0)                                  │ IRQ                     │          │
 │       │                                             │                   8-bit │ AXI-S   │
 │       │                                                                       │          │
 │  ─────┼──────────── Clock Domain Crossing ──────────────── AsyncFIFO ─────────┼──────    │
 │       │                    (sys ↔ ofdm)              host_tx_cdc              │          │
 │       │                                              host_rx_cdc              │          │
 │       │                                                                       │          │
 │       │                                        ofdm clock domain (100 MHz)    │          │
 │       │                                                                       │          │
 │       ▼                                                                       ▼          │
 │  ┌─────────────────────────────────────────────────────────────────────────────────────┐ │
 │  │  OFDMLowerMAC (migen FSM)                                                           │ │
 │  │                                                                                     │ │
 │  │  Driver CSRs:          AXI-Lite Master:                                             │ │
 │  │   ofdm_mcs [1:0]       ctrl_axi_aw{addr,valid,ready}                               │ │
 │  │   ofdm_rate [0]         ctrl_axi_w{data,strb,valid,ready}                           │ │
 │  │   ofdm_ring_bytes [23:0] ctrl_axi_b{resp,valid,ready}                               │ │
 │  │   ofdm_enable [0]      ctrl_axi_ar{addr,valid,ready}                               │ │
 │  │   ofdm_busy [0] (RO)   ctrl_axi_r{data,resp,valid,ready}                           │ │
 │  │   ofdm_frame_count [15:0] (RO)                                                     │ │
 │  │   ofdm_state [3:0] (RO)                                                            │ │
 │  └────────┬────────────────────────────────────────────────────────────────────────────┘ │
 │           │ ctrl_axi (AXI4-Lite, 16-bit addr, 32-bit data)                              │
 │           ▼                                                                              │
 │  ┌─────────────────────────────────────────────────────────────────────────────────────┐ │
 │  │  ofdm_chain_wrapper  (Vivado Block Design — ofdm clock domain)                      │ │
 │  │                                                                                     │ │
 │  │  ┌────────────────────────────────────────────────────────────────────────────────┐  │ │
 │  │  │  axi_smartconnect (1 master → 10 slaves)                                      │  │ │
 │  │  │  Address map: 0x0000–0x0FFF scrambler      0x5000–0x5FFF sync_detect          │  │ │
 │  │  │               0x1000–0x1FFF conv_enc        0x6000–0x6FFF cfo_correct          │  │ │
 │  │  │               0x2000–0x2FFF tx_interleaver  0x7000–0x7FFF ofdm_rx              │  │ │
 │  │  │               0x3000–0x3FFF ofdm_tx         0x8000–0x8FFF rx_interleaver       │  │ │
 │  │  │               0x4000–0x4FFF (reserved)      0x9000–0x9FFF viterbi_dec          │  │ │
 │  │  └────────────────────────────────────────────────────────────────────────────────┘  │ │
 │  │                                                                                     │ │
 │  │  TX Path (host_tx_in → rf_tx_out):                                                  │ │
 │  │                                                                                     │ │
 │  │  host_tx_in ──► scrambler ──► conv_enc ──► interleaver ──► ofdm_tx ──► xfft_ifft   │ │
 │  │  (8b AXI-S)    s_axi_ctrl   s_axi_ctrl    s_axi_ctrl     s_axi_ctrl   (64pt IFFT)  │ │
 │  │                                                                          │          │ │
 │  │                                                                          ▼          │ │
 │  │                                                                     rf_tx_out       │ │
 │  │                                                                     (48b AXI-S)     │ │
 │  │                                                                                     │ │
 │  │  RX Path (rf_rx_in → host_rx_out):                                                  │ │
 │  │                                                                                     │ │
 │  │  rf_rx_in ──► adc_fifo ──► sync_detect ──► cfo_correct ──► xfft_fft ──► ofdm_rx   │ │
 │  │  (48b AXI-S)               s_axi_ctrl     s_axi_ctrl      (64pt FFT)   s_axi_ctrl  │ │
 │  │                                                                          │          │ │
 │  │                     ┌────────────────────────────────────────────────────┘          │ │
 │  │                     ▼                                                               │ │
 │  │              rx_interleaver ──► viterbi_dec ──► rx_scrambler ──► host_rx_out        │ │
 │  │              s_axi_ctrl        s_axi_ctrl      s_axi_ctrl       (8b AXI-S)         │ │
 │  │                                                                                     │ │
 │  └─────────────────────────────────────────────────────────────────────────────────────┘ │
 │           │ rf_tx_out (48b AXI-S)                        ▲ rf_rx_in (48b AXI-S)         │
 │           │                                              │                              │
 │  ─────────┼──── Clock Domain Crossing ───────────────────┼──────                        │
 │           │         (ofdm ↔ sys)                         │                              │
 │           │         rf_tx_cdc                            │ rf_rx_cdc                    │
 │           ▼                                              │                              │
 │  ┌──────────────┐                              ┌──────────────┐                         │
 │  │ _RFTxBridge  │  48b→32b                     │ _RFRxBridge  │  32b→48b                │
 │  │ {i[15:0],    │  (pack I/Q                   │ {i[15:0],    │  (unpack I/Q            │
 │  │  q[15:0]}    │   into 32b)                  │  q[15:0]}    │   from 32b)             │
 │  └──────┬───────┘                              └──────┬───────┘                         │
 │         │ 32-bit                                      ▲ 32-bit                          │
 │         ▼                                             │                                 │
 │  ┌───────────────────────────────────────────────────────────────────────────────────┐  │
 │  │  AD9364Core (AD9364PHY + SPI + data path)                                         │  │
 │  │  rfic clock domain (from AD9364 DATA_CLK)                                         │  │
 │  │  12-bit I/Q samples, LVDS interface, 1R1T mode                                    │  │
 │  └───────────────────────────────────────────────────────────────────────────────────┘  │
 │         │                                             ▲                                 │
 │         ▼ TX LVDS (6 pairs)                           │ RX LVDS (6 pairs)               │
 │  ═══════╪═════════════════════════════════════════════╪═════════════════════════════     │
 └─────────┼─────────────────────────────────────────────┼─────────────────────────────────┘
           ▼                                             │
    ┌──────────────┐                              ┌──────────────┐
    │   AD9364      │          RF Front End        │   AD9364      │
    │   TX DAC      │◄────── Antenna / Cable ─────►│   RX ADC      │
    └──────────────┘         Loopback              └──────────────┘
```

## Files

| File | Role |
|------|------|
| `hallycon_m2sdr_platform.py` | Board pin map (copied from `docs/files/`) |
| `hallycon_m2sdr_target_v2.py` | Reference v2 target — source of `AD9364Core` and `_CRG` |
| `shell.py` | **Integrated top** — LiteX SoC instantiating `ofdm_chain_wrapper` + `OFDMLowerMAC` FSM |
| `../vivado/create_ofdm_bd.tcl` | Creates the BD with `axi_smartconnect` crossbar, generates Verilog wrapper + filelist |
| `mac.md` | MAC layer split architecture design document |
| `README.md` | This file |

## Build outputs

After a successful build, the following artifacts are in
`build/hallycon_m2sdr_platform/gateware/`:

| File | Description |
|------|-------------|
| `hallycon_m2sdr_platform.bit` | JTAG-programmable bitstream (1.6 MB compressed) |
| `hallycon_m2sdr_platform.bin` | Quad-SPI flash binary for persistent programming |
| `hallycon_m2sdr_platform_route.dcp` | Post-route Vivado checkpoint (for incremental builds) |
| `hallycon_m2sdr_platform_utilization_synth.rpt` | Post-synthesis utilization report |
| `hallycon_m2sdr_platform_timing.rpt` | Post-route timing summary |
| `hallycon_m2sdr_platform_power.rpt` | Estimated power report |
| `../software/` | LitePCIe driver sources + userspace tools |
| `../csr.csv` / `csr.json` | CSR register map (ofdm_mac at region 2) |

## Build flow

### Prerequisites

1. Vitis HLS 2025.2 + Vivado 2025.2, sourced in the current shell.
2. Python packages: `migen`, `litex`, `litepcie`, `litedram`.
3. HLS IP catalog already populated at `ip_repo/` (the 8 per-block IPs).
   Run `setup_vitis.sh export` on master if you need to regenerate them.

### Step 1 — Export the OFDM BD wrapper

```bash
cd /mnt/d/work/ofdm_litex
vivado -mode batch -source vivado/create_ofdm_bd.tcl
```

This creates `vivado/ofdm_bd/…/ofdm_chain_wrapper.v` plus a file list at
`ip_repo/ofdm_chain.f` that `shell.py` reads during LiteX elaboration.

### Step 2 — Build the LiteX bitstream

```bash
source /home/abhinavb/Xilinx/2025.2/Vivado/settings64.sh   # Vivado on PATH
cd litex
python3 shell.py --build                # with OFDM chain spliced in
python3 shell.py --build --no-ofdm      # v2-equivalent direct DMA↔RFIC
python3 shell.py --build --load         # ... then JTAG program
```

LiteX emits:
- Top Verilog + Vivado project tcl → `build/hallycon_m2sdr_platform/gateware/`
- LitePCIe driver + userspace tools → `build/hallycon_m2sdr_platform/software/`

### How sources are loaded

`shell.py:_add_ofdm_chain()` uses a hybrid source-loading strategy:

1. **BD wrapper + simple IP stubs** — read from `ip_repo/ofdm_chain.f` filelist
   via `platform.add_source()` (flat `read_verilog` in the generated TCL).
2. **HLS IP RTL** — globbed from `ip_repo/*/hdl/verilog/*.v` and `*.vh`
   (~197 Verilog + 12 header files). These provide the actual HLS module
   implementations that the BD synth stubs instantiate.
3. **Xilinx encrypted/VHDL IPs** — `xfft` (×2) and `axi_smartconnect` (×1)
   are loaded via `platform.add_ip()` which emits `read_ip` + `generate_target`
   + `synth_ip` in the TCL. These run OOC (out-of-context) synthesis before
   top-level `synth_design`.

IPs handled through the IP flow are excluded from the flat filelist to prevent
dual-add errors (`ip_flow_ips` filter in the source loader).

## Runtime control

The OFDM chain's AXI-Lite CSRs are exposed through an `axi_smartconnect`
crossbar (1 master → 10 slaves) inside the BD. The crossbar's S00_AXI port
is wired out as `ctrl_axi` on `ofdm_chain_wrapper` and connected to the
`OFDMLowerMAC` FSM in `shell.py`.

The lower MAC FSM autonomously sequences `ap_start` pulses and programs
`mod`/`rate`/`n_syms` on the TX-side HLS blocks for each frame in a DMA
burst. The driver only writes a few CSRs (`ofdm_mcs`, `ofdm_rate`,
`ofdm_ring_bytes`, `ofdm_enable`) and waits for completion.

See `mac.md` for full architecture details.

## Bring-up order

1. **Phase 0 — board alive.** `--no-ofdm`. Verify PCIe enumeration, AD9364
   SPI (CHIP_ID=0x0A), TX tone loopback via external SDR.
2. **Phase 1 — OFDM TX.** `shell.py --build`. Host pushes raw payload bytes
   via LitePCIe DMA; TX chain (scrambler → conv_enc → interleaver → ofdm_tx)
   emits IQ at AD9364. Capture off-air, validate with
   `sim/ofdm_reference.py --compare`.
3. **Phase 2 — OFDM RX.** Cable-loopback (or 2× boards). Validate BER=0 on
   clean channel.  Needs the CSR exposure work to tune sync thresholds.
4. **Phase 3 — OTA.** Drone TX ↔ ground RX.

## Known gaps / remaining work

- ~~**AXI-Lite CSR aggregation.**~~ Done. `axi_smartconnect` crossbar added
  in `create_ofdm_bd.tcl`, `OFDMLowerMAC` FSM in `shell.py`.
- ~~**No auto-chain FSM.**~~ Done. `OFDMLowerMAC` sequences all HLS blocks
  per-frame autonomously.
- **Sample width scaling.** `_RFRxBridge` / `_RFTxBridge` today forward
  `data[0:32]` untouched. The AD9364 12-bit samples come sign-extended to
  Q1.11 but the OFDM chain expects Q0.15 — without a 1-bit shift you lose
  6 dB at the FFT input. Fine for bring-up, must fix before Phase 2.
- **1R1T only.** Bridges drop lane B. The drone downlink is 1R1T; 2R2T
  needs a second OFDM chain instance or a lane mux.
- **Upper MAC driver.** `software/kernel/ofdm_mac.c` is TBD — needs
  fragmentation, CRC32, reassembly, and MCS adaptation. See `mac.md`.
- **Clocking — solved.** The BD runs in its own 100 MHz `ofdm` domain
  (new PLL clkout from `_CRG` in `shell.py`). `stream.ClockDomainCrossing`
  AsyncFIFOs sit on all four streams (host_tx / host_rx / rf_tx / rf_rx):
  host side crosses sys↔ofdm, RF side crosses sys↔ofdm after the
  AD9364Core's existing rfic↔sys CDC. Gray-pointer paths are declared
  false via `set_max_delay -datapath_only 8.0` in `pre_placement_commands`.

## Resource utilization (actual, post-synthesis 2026-04-12)

| Resource | Used | Available | Util% |
|----------|-----:|----------:|------:|
| Slice LUTs | 22,739 | 32,600 | **69.75%** |
| — LUT as Logic | 20,808 | 32,600 | 63.83% |
| — LUT as Memory | 1,931 | 9,600 | 20.11% |
| Slice Registers | 27,365 | 65,200 | 41.97% |
| Block RAM Tiles | 60.5 | 75 | **80.67%** |
| — RAMB36E1 | 43 | 75 | 57.33% |
| — RAMB18E1 | 35 | 150 | 23.33% |
| DSP48E1 | 94 | 120 | **78.33%** |
| F7 Muxes | 174 | 16,300 | 1.07% |

**Timing:** All paths met. WNS positive on every clock-domain crossing.
Zero failing setup or hold endpoints post-route.

**Headroom assessment:**
- **BRAM is the tightest resource** at 81%. Any new IP with significant
  buffer requirements needs careful budgeting.
- LUTs at 70% leave ~10k for future additions (upper MAC logic, debug ILA).
- DSP at 78% — 26 slices free, enough for minor additions but not a
  second OFDM chain instance.
- Registers at 42% are comfortable.

### Build time breakdown (WSL2, Ryzen 7 PRO 7840U)

| Stage | Elapsed |
|-------|--------:|
| OOC synth: smartconnect | ~3 min |
| OOC synth: xfft (×2) | ~2 min each |
| OOC synth: PCIe 7x | ~1 min |
| Top-level synth_design | 5:24 |
| opt_design | ~10 sec |
| place_design | 2:50 |
| route_design | 1:10 |
| write_bitstream | 0:33 |
| **Total (Vivado)** | **~18 min** |
