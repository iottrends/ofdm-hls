# OFDM MAC — Split Architecture (Driver + FPGA Lower MAC)

Design document for the MAC layer that sits between the userspace/kernel
networking stack and the opaque HLS OFDM PHY chain.

**Implementation status (2026-04-12):**
- Lower MAC (`OFDMLowerMAC` migen FSM) — implemented in `shell.py`
- `axi_smartconnect` crossbar (1:10) — implemented in `create_ofdm_bd.tcl`
- Full SoC bitstream — **built and verified** on XC7A50T (0 errors, timing met)
- Upper MAC driver (`ofdm_mac.c`) — TBD

## Goals

1. **Driver = control plane only**: policy decisions (MCS selection from
   RSSI, packet fragmentation, retransmit) live in C. No per-frame
   `ap_start` pulses from the CPU.
2. **Lower MAC = data-plane sequencer**: a small migen FSM on the FPGA
   reads driver-set CSRs and drives the HLS chain for an entire DMA burst
   autonomously. CPU writes a few CSRs, fires DMA, waits for a done
   interrupt.
3. **Zero HLS changes**: the 10 HLS blocks stay `ap_ctrl_hs`. The lower
   MAC drives their AXI-Lite slaves through a single
   `axi_smartconnect` inside the BD.
4. **MCS/rate is a per-burst parameter**: one DMA ring = one MCS + rate.
   Adaptation happens *between* bursts, never mid-burst.

## Layered picture

```
+-----------------------------------------------------------------------+
|  Userspace app / kernel net stack                                     |
|  Sends/receives variable-length packets (e.g. 1500 B MTU)            |
+----------------------------------+------------------------------------+
                                   |
+----------------------------------v------------------------------------+
|  UPPER MAC  --  driver (software/kernel/ofdm_mac.c)         [TBD]    |
|                                                                       |
|  Periodic (every ~1 ms):                                              |
|    rssi     = read ad9364_spi[RSSI_REG]                               |
|    mcs,rate = adapt(rssi, per_history)                                |
|    write ofdm_mcs, ofdm_rate CSRs                                     |
|                                                                       |
|  Per TX burst:                                                        |
|    fragment packet(s) into PHY-frame-sized chunks                     |
|    wrap each frame with { seq | frag_id | frag_total | crc32 }        |
|    fill DMA ring with N total bytes                                   |
|    write ofdm_ring_bytes = N                                          |
|    write ofdm_enable = 1   (kick)                                     |
|    wait for IRQ / poll ofdm_busy == 0                                 |
|                                                                       |
|  Per RX frame:                                                        |
|    drain DMA RX ring                                                  |
|    parse header, check CRC32                                          |
|    drop / reassemble / deliver                                        |
+----------------------------------+------------------------------------+
                                   |
                          LiteX CSR fabric (PCIe BAR0)
                                   |
+----------------------------------v------------------------------------+
|  LOWER MAC  --  OFDMLowerMAC in shell.py (migen FSM)   [Implemented] |
|                                                                       |
|  CSRs exposed to driver (ofdm clock domain, auto-CDC by LiteX):      |
|  +---------------------------------------------------------------+   |
|  |  WR  ofdm_mcs          : 2b  mod (0=BPSK, 1=QPSK, 2=16QAM)  |   |
|  |  WR  ofdm_rate         : 1b  code rate (0=1/2, 1=2/3)        |   |
|  |  WR  ofdm_ring_bytes   :24b  total bytes in TX DMA burst      |   |
|  |  WR  ofdm_enable       : 1b  kick; auto-clears at done        |   |
|  |  RO  ofdm_busy         : 1b  FSM mid-burst                    |   |
|  |  RO  ofdm_frame_count  :16b  +1 per ofdm_tx.ap_done           |   |
|  |  RO  ofdm_state        : 4b  FSM state for debug              |   |
|  +---------------------------------------------------------------+   |
|                                                                       |
|  AXI-Lite master port (ctrl_axi):                                     |
|    awaddr[15:0], awprot[2:0], awvalid/awready                         |
|    wdata[31:0], wstrb[3:0], wvalid/wready                             |
|    bresp[1:0], bvalid/bready                                          |
|    araddr[15:0], arprot[2:0], arvalid/arready                         |
|    rdata[31:0], rresp[1:0], rvalid/rready                             |
|                                                                       |
|  FSM behavior on enable=1:                                            |
|     1. bytes_per_frame = lut[mcs, rate] * n_syms (=10)                |
|     2. num_frames      = ceil(ring_bytes / bytes_per_frame)           |
|     3. for f in 0..num_frames-1:                                      |
|          axi_lite_write  tx_scrambler.n_bytes   <- bytes_per_frame    |
|          axi_lite_write  tx_conv_enc.rate       <- rate               |
|          axi_lite_write  tx_conv_enc.n_data_bytes <- bytes_per_frame  |
|          axi_lite_write  tx_interleaver.mod     <- mcs                |
|          axi_lite_write  tx_interleaver.n_syms  <- 10                 |
|          axi_lite_write  ofdm_tx.mod            <- mcs                |
|          axi_lite_write  ofdm_tx.n_syms         <- 10                 |
|          axi_lite_write  <all 4>.ap_start       <- 1                  |
|          poll            ofdm_tx.AP_CTRL bit1 (ap_done)               |
|          frame_count++                                                |
|     4. busy <- 0;  fire done IRQ                                      |
+----------------------------------+------------------------------------+
                                   |
                           ctrl_axi (AXI4-Lite, 16-bit addr / 32-bit data)
                                   |
+----------------------------------v------------------------------------+
|  axi_smartconnect 1:10 (inside ofdm_chain_wrapper BD)  [Implemented] |
|                                                                       |
|  Address map (4 KB per slave):                                        |
|  0x0000 tx_scrambler    |  0x5000 sync_detect                         |
|  0x1000 tx_conv_enc     |  0x6000 cfo_correct                         |
|  0x2000 tx_interleaver  |  0x7000 ofdm_rx                             |
|  0x3000 ofdm_tx         |  0x8000 rx_interleaver                      |
|  0x4000 rx_scrambler    |  0x9000 viterbi_dec                         |
+----+------+------+------+------+------+------+------+------+----+----+
     |      |      |      |      |      |      |      |      |    |
     v      v      v      v      v      v      v      v      v    v
  s_axi_ctrl on each of the 10 HLS blocks
  (AP_CTRL @ +0x00, data regs @ +0x10..+0x20)

+-----------------------------------------------------------------------+
|  OFDM PHY Chain (AXI-Stream data path, all in ofdm clock domain)      |
|                                                                       |
|  TX Path:                                                             |
|  host_tx_in ---> scrambler ---> conv_enc ---> interleaver ---> ofdm_tx|
|  (8b AXI-S)                                                     |     |
|                                                                  v     |
|                                                             xfft(IFFT)|
|                                                                  |     |
|                                                                  v     |
|                                                             rf_tx_out |
|                                                             (48b)     |
|                                                                       |
|  RX Path:                                                             |
|  rf_rx_in ---> adc_fifo ---> sync_detect ---> cfo_correct ---> xfft  |
|  (48b AXI-S)                                                (FFT) |   |
|                                                                   v   |
|       ofdm_rx ---> rx_interleaver ---> viterbi_dec ---> rx_scrambler  |
|       (header                                                  |      |
|        decode)                                                 v      |
|                                                           host_rx_out |
|                                                           (8b AXI-S)  |
+-----------------------------------------------------------------------+
                |                                      ^
                | rf_tx_out (48b)                       | rf_rx_in (48b)
                |                                      |
    ============|== Clock Domain Crossing ==============|============
                |      (ofdm <-> sys)                  |
                |      rf_tx_cdc / rf_rx_cdc           |
                v                                      |
         +--------------+                       +--------------+
         | _RFTxBridge  |  48b->32b             | _RFRxBridge  |  32b->48b
         | {i[15:0],    |  (pack I/Q)           | {i[15:0],    |  (unpack)
         |  q[15:0]}    |                       |  q[15:0]}    |
         +------+-------+                       +------+-------+
                |  32-bit                              ^  32-bit
                v                                      |
         +-----------------------------------------------------+
         |  AD9364Core (AD9364PHY + SPI + data path)            |
         |  rfic clock domain (from AD9364 DATA_CLK)            |
         |  12-bit I/Q samples, LVDS interface, 1R1T mode       |
         +-----------------------------------------------------+
                |  TX LVDS (6 pairs)        ^  RX LVDS (6 pairs)
                v                           |
         +-----------+              +-----------+
         | AD9364    |   Antenna /  | AD9364    |
         | TX DAC    |<-- Cable -->| RX ADC    |
         +-----------+   Loopback   +-----------+
```

## Frame math

`n_syms` is hard-coded to **10** in the lower MAC for Phase 1. Payload
bytes per PHY frame (excluding preamble + header symbols, which are
overhead the PHY generates internally):

| MCS     | Bits/SC | Coded bits/sym | Rate | Uncoded B/sym | B per PHY frame (n_syms=10) |
|---------|--------:|---------------:|-----:|--------------:|----------------------------:|
| BPSK    |       1 |            200 |  1/2 |       12.5 ->  12 |                         120 |
| QPSK    |       2 |            400 |  1/2 |            25 |                         250 |
| QPSK    |       2 |            400 |  2/3 |        ~33    |                         330 |
| 16-QAM  |       4 |            800 |  1/2 |            50 |                         500 |
| 16-QAM  |       4 |            800 |  2/3 |        ~66    |                         660 |

(200 data subcarriers x bits/SC x rate, rounded to whole bytes per symbol.
Exact values go into a tiny LUT inside the FSM -- mis-alignment cases
like BPSK fractional bytes round down.)

Airtime per PHY frame (preamble + header + 10 payload = 13 symbols x 272
samples @ 30.72 MSPS) = **115 us**.

Upper MAC burst sizing example -- 16-QAM 1/2, MTU 1500 B packet:
- MAC header + CRC32 = 16 B per frame -> net payload = 484 B/frame
- `ceil(1500 / 484)` = 4 PHY frames
- Burst airtime = 4 x 115 us = **460 us**
- `ofdm_ring_bytes` = 4 x 500 = **2000**

## Why split the MAC this way

### vs. driver-only MAC (Option A)

A pure driver MAC means the CPU pulses `ap_start` per HLS block per
frame -- that's 10 CSR writes x N frames x PCIe round-trip latency. At
PCIe Gen2 x2 ~1 us RTT, a 4-frame burst needs 40 CSR writes = ~40 us
of CPU stall per burst, *on top of* the 460 us airtime. Doable for
bring-up, painful for throughput.

Lower MAC cuts that to **one** kick CSR write (`enable=1`) plus
polling. Effectively free.

### vs. pure hardware MAC (Option C)

A full hardware MAC would include the CRC32, fragmentation, and
retransmit state machines in the BD. That's ~1-2k LUT of rigid logic
that's hard to change when the protocol evolves. Retransmit logic
especially: window sizes, backoff timers, RTT estimation -- all things
we don't know yet and will want to tune in software.

Split MAC keeps those policies in C where they belong.

### What stays in hardware

Lower MAC only handles the mechanical stuff that *has* to be fast:
- Sequencing AXI-Lite writes to HLS registers (~10 writes/frame).
- Polling `ap_done` at clock speed (nanoseconds, not microseconds).
- Counting frames.

Everything else -- CRC, framing, reassembly, retransmit, MCS selection
-- is software.

## Register offsets (s_axi_ctrl slaves)

All offsets are relative to each block's `s_axi_ctrl` base address (the
address the `axi_smartconnect` master port is mapped to). Sourced from
`ip_repo/<block>/drivers/<block>_v1_0/src/x<block>_hw.h`.

### Common to every block

| Offset | Name     | Width | Access   | Meaning                                |
|-------:|----------|------:|----------|----------------------------------------|
| `0x00` | AP_CTRL  | 32    | RW       | bit0 ap_start, bit1 ap_done, bit2 ap_idle, bit3 ap_ready, bit7 auto_restart |
| `0x04` | GIE      | 32    | RW       | global interrupt enable                |
| `0x08` | IER      | 32    | RW       | interrupt enable (ap_done / ap_ready)  |
| `0x0c` | ISR      | 32    | RW (TOW) | interrupt status                       |

### Per-block data registers

| Block          | Register      | Offset | Width | Notes                               |
|----------------|---------------|-------:|------:|-------------------------------------|
| `scrambler`    | `n_bytes`     | `0x10` |    16 | Used by both `tx_scrambler` and `rx_scrambler` -- payload byte count for this invocation. |
| `conv_enc`     | `rate`        | `0x10` |     1 | 0 = 1/2, 1 = 2/3                    |
| `conv_enc`     | `n_data_bytes`| `0x18` |    32 | Uncoded input byte count            |
| `interleaver`  | `mod`         | `0x10` |     1 | 0 = QPSK, 1 = 16-QAM                |
| `interleaver`  | `n_syms`      | `0x18` |     8 | OFDM payload symbols per frame      |
| `interleaver`  | `is_rx`       | `0x20` |     1 | **0 for TX, 1 for RX** -- set once at boot |
| `ofdm_tx`      | `mod`         | `0x10` |     1 | 0 = QPSK, 1 = 16-QAM                |
| `ofdm_tx`      | `n_syms`      | `0x18` |     8 | OFDM payload symbols per frame      |
| `sync_detect`  | `n_syms`      | `0x10` |     8 | WARNING: different offset from ofdm_tx/interleaver |
| `cfo_correct`  | -- none --    |      -- |     -- | Only AP_CTRL; no data registers     |
| `ofdm_rx`      | `header_err`  | `0x10` |     1 | Read-only -- header CRC fail flag    |
| `viterbi_dec`  | `rate`        | `0x10` |     1 | 0 = 1/2, 1 = 2/3                    |
| `viterbi_dec`  | `n_data_bytes`| `0x18` |    32 | Decoded output byte count           |

### Key observations

1. **`mod` is 1 bit, not 2.** Only two modulations are exposed at
   runtime: QPSK (0) and 16-QAM (1). BPSK is not runtime-selectable
   even though the HLS code may implement it. The driver CSR `ofdm_mcs`
   is therefore effectively 1 bit too.
2. **`n_syms` lives at a different offset on `sync_detect`** (`0x10`)
   than on `ofdm_tx` / `interleaver` (`0x18`). Easy to mis-type in the
   FSM address LUT.
3. **`ofdm_rx` auto-detects `mod` from the OTA header symbol** and has
   no `mod`/`n_syms` CSRs of its own. It signals header-parse failure
   via the read-only `header_err` flag at `0x10`. Downstream RX blocks
   (`rx_interleaver`, `viterbi_dec`, `rx_scrambler`) still need `mod` /
   `rate` / `n_syms` / `n_bytes` programmed from outside.
4. **Loopback RX config = mirror of TX config.** For Phase 1 loopback
   bring-up the lower MAC can just write the same `mcs`/`rate` values
   to both TX-side and RX-side blocks. For real OTA this breaks -- the
   receiver doesn't know the transmitter's MCS ahead of time, so
   either: (a) `ofdm_rx` must export the decoded mod/rate as sideband
   wires the lower MAC reroutes to the downstream blocks, or (b) a
   fixed MCS is negotiated via a slow control channel. Out of scope
   for Phase 1.
5. **`cfo_correct` has no data CSRs** -- lower MAC only pulses its
   `ap_start` and waits for `ap_done`, nothing else.
6. **`conv_enc.n_data_bytes`** (uncoded input) and
   **`viterbi_dec.n_data_bytes`** (decoded output) are *the same
   number*: whatever the raw payload length is after the scrambler.
   Both are 32-bit, allowing very large frames (4 GB theoretical).
7. **`scrambler.n_bytes`** is 16-bit, limiting a single frame to 64 KB
   after scrambling -- not a practical limit.

### Per-frame write sequence (TX side, pseudocode)

```
# bytes_per_frame = n_syms * bytes_per_symbol(mcs, rate)
#   n_syms = 10 (hardcoded)
#   bytes_per_symbol: QPSK 1/2 =25, QPSK 2/3 =33, 16QAM 1/2 =50, 16QAM 2/3 =66

write tx_scrambler.n_bytes     = bytes_per_frame
write tx_scrambler.AP_CTRL     = 1   (ap_start)

write tx_conv_enc.rate         = rate
write tx_conv_enc.n_data_bytes = bytes_per_frame
write tx_conv_enc.AP_CTRL      = 1

write tx_interleaver.mod       = mcs
write tx_interleaver.n_syms    = 10
write tx_interleaver.is_rx     = 0   (set once at boot actually)
write tx_interleaver.AP_CTRL   = 1

write ofdm_tx.mod              = mcs
write ofdm_tx.n_syms           = 10
write ofdm_tx.AP_CTRL          = 1

poll  ofdm_tx.AP_CTRL bit1 (ap_done) until set
frame_count++
```

Four `ap_start` pulses per TX frame (scrambler, conv_enc, interleaver,
ofdm_tx), plus the `sync_detect`/`cfo_correct`/`ofdm_rx`/`rx_*` chain
on the RX side. Total = 10 CSR writes x ~2 cycles each = **~20 clock
cycles of setup per frame**, negligible next to a ~115 us frame airtime.

### Boot-time one-shot writes

Set once after `rst_n` release, before `enable=1` is first seen:

```
write rx_interleaver.is_rx = 1     # critical -- default is 0 (TX mode)
write tx_interleaver.is_rx = 0     # explicit, default is already 0
```

## Open questions / Phase 2+

- **RX path symmetry.** This doc focuses on TX. The RX lower MAC needs
  to count `ofdm_rx.ap_done`, copy decoded bytes into the RX DMA ring,
  and signal the driver. Design is mirror-image but not yet sketched.
  For Phase 1 we can rely on `rx_output_fifo` + DMA streaming with no
  FSM on the RX side -- frames get pushed through as fast as they
  arrive, driver sorts them out via CRC after the fact.
- **Backpressure safety.** If the driver sets `ring_bytes` but the DMA
  engine hasn't yet placed all those bytes in the ring by the time the
  FSM kicks `ap_start`, the HLS block will stall on `tready`. That's
  safe (no data loss) but costs latency. Simple mitigation: driver
  starts DMA *before* writing `enable=1`.
- **`n_syms` as a CSR** (Phase 3). When adaptive MCS kicks in, the rate
  controller may also want to shrink frames on bad channels. At that
  point `n_syms` graduates from a hard-coded 10 to a 3rd driver CSR.
- **Retransmit / ARQ.** Out of scope for this doc. Will live entirely in
  driver C code once the basic pipe works.
- **Interrupt vs polling.** Phase 1 uses `ofdm_busy` polling. Phase 2
  can hook the FSM's done signal to a spare MSI line for IRQ-driven
  completion.

## Build verification (2026-04-12)

The full SoC including `OFDMLowerMAC` + `axi_smartconnect` crossbar + all
10 HLS blocks + 2x xfft + PCIe + AD9364PHY + HyperRAM has been built to
bitstream on XC7A50T-2CSG325.

### Resource impact of MAC additions

| Component | Approx LUT | Notes |
|-----------|--------:|-------|
| `OFDMLowerMAC` FSM | ~200 | AXI-Lite master FSM + CSR registers |
| `axi_smartconnect` 1:10 | ~800 | Crossbar routing + arbitration |
| **MAC overhead total** | ~1,000 | <5% of total LUT budget |

The MAC additions fit comfortably. The full design uses 22,739 LUT (70%),
94 DSP (78%), 60.5 BRAM tiles (81%).

### Full design resource utilization

| Resource | Used | Available | Util% |
|----------|-----:|----------:|------:|
| Slice LUTs | 22,739 | 32,600 | **69.75%** |
| Slice Registers | 27,365 | 65,200 | 41.97% |
| Block RAM Tiles | 60.5 | 75 | **80.67%** |
| DSP48E1 | 94 | 120 | **78.33%** |

### Verified build stages

1. OOC IP synthesis (smartconnect, xfft x2, PCIe) -- all passed
2. Top-level synthesis -- 0 errors, all HLS modules resolved
3. Placement -- 0 errors, completed in 2:50
4. Routing -- 0 errors, all overlaps resolved, timing met (WNS > 0)
5. Bitstream -- `hallycon_m2sdr_platform.bit` (1.6 MB compressed)

### Critical warnings (benign)

- `synth_ip not supported in project mode` (x4) -- expected; Vivado uses
  `generate_target` instead, IPs synthesize via OOC `synth_design`.
- `overwriting previous definition of module` (x10) -- BD synth stubs
  overwrite sim stubs; synth stubs are the correct ones to use.

## File map

| Path | Role | Status |
|------|------|--------|
| `litex/shell.py` | `OFDMLowerMAC` submodule + `_add_ofdm_chain` source loader | Implemented |
| `vivado/create_ofdm_bd.tcl` | `axi_smartconnect` + `ctrl_axi` external port | Implemented |
| `build/.../hallycon_m2sdr_platform.bit` | JTAG bitstream | Generated |
| `build/.../hallycon_m2sdr_platform.bin` | SPI flash binary | Generated |
| `build/.../csr.csv` | CSR register map (ofdm_mac at region 2) | Generated |
| `software/kernel/ofdm_mac.c` | Upper MAC (fragmentation, CRC, reassembly) | **TBD** |
| `litex/mac.md` | This document | Current |
