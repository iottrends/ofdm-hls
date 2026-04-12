#!/usr/bin/env python3
#
# shell.py — LiteX SoC shell around the existing ofdm_chain Vivado BD
#
# Path-B integration: the OFDM chain is a single opaque Verilog module
# (ofdm_chain_wrapper.v, generated from vivado/create_project.tcl).  This
# LiteX target instantiates that wrapper and wires it to S7PCIEPHY +
# LitePCIeDMA on the host side and AD9364Core on the RF side — no HLS
# block-level wrappers, no per-IP Instance() port maps.
#
# Build flow:
#   1. vivado -mode batch -source ../vivado/create_ofdm_bd.tcl
#      → emits vivado/ofdm_impl/.../ofdm_chain_wrapper.v (and the BD's
#        generated sub-IP sources) + a file list at ip_repo/ofdm_chain.f
#   2. python3 shell.py --build [--load]
#      → LiteX emits Verilog top; Vivado runs synth/impl/bit
#
# SPDX-License-Identifier: MIT
#

import os
import argparse

from migen import *

from hallycon_m2sdr_platform       import Platform
from hallycon_m2sdr_target_v2      import AD9364Core
from litex.soc.cores.clock         import S7PLL, S7IDELAYCTRL

from litex.gen                     import *
from litex.soc.cores.led           import LedChaser
from litex.soc.integration.soc_core import SoCCore, soc_core_args, soc_core_argdict
from litex.soc.integration.builder  import Builder, builder_args, builder_argdict
from litex.soc.integration.soc      import SoCRegion
from litex.soc.interconnect         import stream
from litex.soc.interconnect.csr     import CSRStorage, CSRStatus, AutoCSR
from litex.soc.cores.hyperbus       import HyperRAM as LiteHyperBus
from litex.soc.cores.icap           import ICAP
from litex.soc.cores.xadc           import XADC
from litex.soc.cores.dna            import DNA

from litepcie.phy.s7pciephy         import S7PCIEPHY
from litepcie.core                  import LitePCIeEndpoint, LitePCIeMSI
from litepcie.frontend.dma          import LitePCIeDMA
from litepcie.common                import dma_layout
from litepcie.software              import generate_litepcie_software


# =============================================================================
# OFDMChainWrapper — opaque Instance() of the Vivado BD wrapper
# =============================================================================
#
# Port map mirrors the external interfaces declared in
# vivado/create_project.tcl (make_bd_intf_pins_external):
#
#   host_tx_in    AXI-Stream,  8-bit  (host payload bytes → scrambler)
#   host_rx_out   AXI-Stream,  8-bit  (descrambler → host bytes)
#   rf_tx_out     AXI-Stream, 48-bit  (ofdm_tx iq_out → RFIC; HLS iq_t = {i:16,q:16,last:1})
#   rf_rx_in      AXI-Stream, 48-bit  (RFIC → adc_input_fifo → sync_detect; same iq_t layout)
#
# None of the four interfaces expose a separate TLAST — the HLS iq_t.last bit
# rides inside TDATA[32], and the byte streams have no packetisation.
#
# AXI-Lite control: not yet exposed at the BD boundary.  See README §4
# for the crossbar aggregation step needed to bring CSRs under CPU control.

class OFDMChainWrapper(Module):
    """Opaque Instance() of ofdm_chain_wrapper.v, clocked in the `ofdm` domain
    (100 MHz — matches the BD's -freq_hz constraint).  CDC to/from the LiteX
    sys and rfic domains is handled by the caller via stream.ClockDomainCrossing.

    Also exposes the BD's axi_smartconnect CSR crossbar (`ctrl_axi`) as a bare
    AXI4-Lite slave bundle.  OFDMLowerMAC drives these signals to sequence the
    per-frame register writes + ap_start pulses."""
    def __init__(self):
        # All endpoints live in the ofdm (100 MHz) clock domain.
        self.host_tx  = stream.Endpoint([("data",  8)])
        self.host_rx  = stream.Endpoint([("data",  8)])
        self.rf_tx    = stream.Endpoint([("data", 48)])
        self.rf_rx    = stream.Endpoint([("data", 48)])

        # AXI-Lite slave bundle (driven by OFDMLowerMAC).  16-bit address
        # space covers the 10 × 4 KB windows laid out in create_ofdm_bd.tcl.
        self.ctrl_axi_awaddr  = Signal(16)
        self.ctrl_axi_awprot  = Signal(3)
        self.ctrl_axi_awvalid = Signal()
        self.ctrl_axi_awready = Signal()
        self.ctrl_axi_wdata   = Signal(32)
        self.ctrl_axi_wstrb   = Signal(4)
        self.ctrl_axi_wvalid  = Signal()
        self.ctrl_axi_wready  = Signal()
        self.ctrl_axi_bresp   = Signal(2)
        self.ctrl_axi_bvalid  = Signal()
        self.ctrl_axi_bready  = Signal()
        self.ctrl_axi_araddr  = Signal(16)
        self.ctrl_axi_arprot  = Signal(3)
        self.ctrl_axi_arvalid = Signal()
        self.ctrl_axi_arready = Signal()
        self.ctrl_axi_rdata   = Signal(32)
        self.ctrl_axi_rresp   = Signal(2)
        self.ctrl_axi_rvalid  = Signal()
        self.ctrl_axi_rready  = Signal()

        self.specials += Instance("ofdm_chain_wrapper",
            i_clk   = ClockSignal("ofdm"),
            i_rst_n = ~ResetSignal("ofdm"),

            # host_tx_in  (slave)
            i_host_tx_in_tdata  = self.host_tx.data,
            i_host_tx_in_tvalid = self.host_tx.valid,
            o_host_tx_in_tready = self.host_tx.ready,

            # host_rx_out (master)
            o_host_rx_out_tdata  = self.host_rx.data,
            o_host_rx_out_tvalid = self.host_rx.valid,
            i_host_rx_out_tready = self.host_rx.ready,

            # rf_tx_out   (master)
            o_rf_tx_out_tdata  = self.rf_tx.data,
            o_rf_tx_out_tvalid = self.rf_tx.valid,
            i_rf_tx_out_tready = self.rf_tx.ready,

            # rf_rx_in    (slave)
            i_rf_rx_in_tdata  = self.rf_rx.data,
            i_rf_rx_in_tvalid = self.rf_rx.valid,
            o_rf_rx_in_tready = self.rf_rx.ready,

            # ctrl_axi (AXI4-Lite slave, driven by OFDMLowerMAC)
            i_ctrl_axi_awaddr  = self.ctrl_axi_awaddr,
            i_ctrl_axi_awprot  = self.ctrl_axi_awprot,
            i_ctrl_axi_awvalid = self.ctrl_axi_awvalid,
            o_ctrl_axi_awready = self.ctrl_axi_awready,
            i_ctrl_axi_wdata   = self.ctrl_axi_wdata,
            i_ctrl_axi_wstrb   = self.ctrl_axi_wstrb,
            i_ctrl_axi_wvalid  = self.ctrl_axi_wvalid,
            o_ctrl_axi_wready  = self.ctrl_axi_wready,
            o_ctrl_axi_bresp   = self.ctrl_axi_bresp,
            o_ctrl_axi_bvalid  = self.ctrl_axi_bvalid,
            i_ctrl_axi_bready  = self.ctrl_axi_bready,
            i_ctrl_axi_araddr  = self.ctrl_axi_araddr,
            i_ctrl_axi_arprot  = self.ctrl_axi_arprot,
            i_ctrl_axi_arvalid = self.ctrl_axi_arvalid,
            o_ctrl_axi_arready = self.ctrl_axi_arready,
            o_ctrl_axi_rdata   = self.ctrl_axi_rdata,
            o_ctrl_axi_rresp   = self.ctrl_axi_rresp,
            o_ctrl_axi_rvalid  = self.ctrl_axi_rvalid,
            i_ctrl_axi_rready  = self.ctrl_axi_rready,
        )


# =============================================================================
# Small RF lane bridges (64-bit dma_layout ↔ 32-bit I/Q for channel A)
# =============================================================================

#
# Width-mismatch note (rf_rx / rf_tx are 48-bit, AD9364 is 64-bit dma_layout):
#
#   HLS iq_t side (48-bit)          AD9364 dma_layout side (64-bit)
#   ┌─────────────────────┐         ┌─────────────────────────────┐
#   │ [15: 0]   i   (s16) │ ◄────►  │ [15: 0]   ia  (s16)         │  lane A
#   │ [31:16]   q   (s16) │ ◄────►  │ [31:16]   qa  (s16)         │  lane A
#   │ [32]      last      │         │ [47:32]   ib  (s16)         │  lane B
#   │ [47:33]   HLS pad   │         │ [63:48]   qb  (s16)         │  lane B
#   └─────────────────────┘         └─────────────────────────────┘
#
# iq_t is 48-bit because HLS pads `struct iq_t {i:16, q:16, last:1}` up to the
# next byte boundary (6 bytes).  AD9364 is 64-bit because dma_layout carries
# both RF lanes (2R2T).  We're 1R1T and TLAST is meaningless for a continuous
# RFIC stream, so both "extras" are dropped — by omission on the read side
# (bits we just never wire up) and explicit zero on the write side.
#
# This wastes ~3 BRAM18 on adc_input_fifo (4096 × 48b vs 32b) and triggers
# Vivado "upper bits unused" warnings at BD boundary.  Cosmetic, not a
# correctness issue.  The proper fix is to change HLS iq_t from a packed
# struct to `ap_axiu<32,0,0,0>` so HLS emits clean 32-bit TDATA + sideband
# TLAST pins — deferred until after Phase 1 bring-up.

class _RFRxBridge(Module):
    """AD9364 (64b dma_layout) → ofdm_chain.rf_rx_in (48b HLS iq_t). 1R1T → lane A."""
    def __init__(self):
        self.sink   = stream.Endpoint(dma_layout(64))           # ia|qa|ib|qb
        self.source = stream.Endpoint([("data", 48)])           # iq_t {i,q,last+pad}
        self.comb += [
            self.source.valid.eq(self.sink.valid),
            self.source.data[ 0:16].eq(self.sink.data[ 0:16]),  # i ← ia
            self.source.data[16:32].eq(self.sink.data[16:32]),  # q ← qa
            self.source.data[32:48].eq(0),                      # iq_t.last + HLS pad: tied 0
            # sink.data[63:32] (lane B ib|qb) dropped — 1R1T
            self.sink.ready.eq(self.source.ready),
        ]


class _RFTxBridge(Module):
    """ofdm_chain.rf_tx_out (48b HLS iq_t) → AD9364 (64b dma_layout). 1R1T → lane A."""
    def __init__(self):
        self.sink   = stream.Endpoint([("data", 48)])           # iq_t {i,q,last+pad}
        self.source = stream.Endpoint(dma_layout(64))           # ia|qa|ib|qb
        self.comb += [
            self.source.valid.eq(self.sink.valid),
            self.source.data[ 0:16].eq(self.sink.data[ 0:16]),  # ia ← i
            self.source.data[16:32].eq(self.sink.data[16:32]),  # qa ← q
            # sink.data[47:32] (iq_t.last + HLS pad) dropped — AD9364 streams continuously
            self.source.data[32:64].eq(0),                      # lane B (ib|qb): tied 0, 1R1T
            self.sink.ready.eq(self.source.ready),
        ]


# =============================================================================
# OFDMLowerMAC — autonomous per-frame CSR sequencer
# =============================================================================
#
# Driver writes four CSRs (mcs / rate / ring_bytes / enable) and polls two
# (busy / frame_count).  On enable rising edge, this FSM:
#
#   1. Latches mcs + rate (snapshot — mid-burst driver updates are ignored).
#   2. Computes bytes_per_frame from a 4-entry LUT.
#   3. For each frame (until ring_bytes_remaining == 0):
#        a. Walks a table of ~23 AXI-Lite writes → all HLS s_axi_ctrl slaves
#           (scrambler.n_bytes, conv_enc.rate, conv_enc.n_data_bytes,
#           interleaver.mod, interleaver.n_syms, ofdm_tx.mod, ofdm_tx.n_syms,
#           and the RX-side mirror — sync_detect.n_syms, viterbi_dec.rate
#           etc., plus 10 ap_start pulses).
#        b. Polls ofdm_tx.AP_CTRL bit1 (ap_done).
#        c. Increments frame_count; subtracts bytes_per_frame from remaining.
#   4. Parks in BURST_DONE until driver writes enable=0, then → IDLE.
#
# Clock domain: everything lives in `ofdm` (100 MHz).  The driver-facing
# CSRs auto-CDC via LiteX's CSRStorage/CSRStatus infrastructure.

class OFDMLowerMAC(Module, AutoCSR):
    """Free-running per-frame sequencer sitting on ctrl_axi."""

    # ── Block base addresses in ctrl_axi space (must match create_ofdm_bd.tcl) ──
    BASE_TX_SCRAM = 0x0000
    BASE_TX_CONV  = 0x1000
    BASE_TX_INTLV = 0x2000
    BASE_OFDM_TX  = 0x3000
    BASE_SYNC     = 0x4000
    BASE_CFO      = 0x5000
    BASE_OFDM_RX  = 0x6000
    BASE_RX_INTLV = 0x7000
    BASE_VIT      = 0x8000
    BASE_RX_SCRAM = 0x9000

    # ── Register offsets within each HLS s_axi_ctrl slave (from mac.md) ──
    AP_CTRL         = 0x00
    MOD             = 0x10     # ofdm_tx, interleaver
    N_SYMS_TX       = 0x18     # ofdm_tx, interleaver  (NOTE: different from sync!)
    IS_RX           = 0x20     # interleaver only
    N_SYMS_SYNC     = 0x10     # sync_detect — different offset from ofdm_tx!
    RATE            = 0x10     # conv_enc, viterbi_dec
    N_DATA_BYTES    = 0x18     # conv_enc, viterbi_dec (32b)
    N_BYTES         = 0x10     # scrambler (16b, used by both tx_/rx_)

    # ── Hardcoded parameters (Phase 1) ──
    N_SYMS = 10   # payload OFDM symbols per PHY frame

    def __init__(self, chain):
        """chain: OFDMChainWrapper instance whose ctrl_axi_* signals we drive."""

        # -------- Driver-visible CSRs --------
        self.mcs         = CSRStorage(1,  description="Modulation: 0=QPSK, 1=16QAM")
        self.rate        = CSRStorage(1,  description="Code rate: 0=1/2, 1=2/3")
        self.ring_bytes  = CSRStorage(24, description="Total bytes in current TX DMA burst — must be an integer multiple of bytes_per_frame(mcs,rate)")
        self.enable      = CSRStorage(1,  description="Write 1 to kick; driver polls busy=0 then writes 0 to rearm")
        self.busy        = CSRStatus(1,  description="FSM is mid-burst")
        self.frame_count = CSRStatus(16, description="Cumulative frames since reset")
        self.state       = CSRStatus(4,  description="FSM state (0=IDLE .. 8=DONE)")

        # -------- Internal signals --------
        latched_mcs  = Signal()
        latched_rate = Signal()
        bytes_per_fr = Signal(16)
        bytes_remain = Signal(24)
        boot_done    = Signal()

        # bytes_per_frame LUT (from mac.md frame math):
        #   QPSK  1/2 → 25 × 10 = 250
        #   QPSK  2/3 → 33 × 10 = 333  (HLS truncates 33.33)
        #   16QAM 1/2 → 50 × 10 = 500
        #   16QAM 2/3 → 66 × 10 = 666  (HLS truncates 66.67)
        self.comb += Case(Cat(latched_rate, latched_mcs), {
            0b00: bytes_per_fr.eq(250),
            0b01: bytes_per_fr.eq(333),
            0b10: bytes_per_fr.eq(500),
            0b11: bytes_per_fr.eq(666),
        })

        # n_data_bytes for conv_enc / viterbi_dec = same as bytes_per_frame,
        # zero-extended to 32 bits.
        n_data_bytes_32 = Signal(32)
        self.comb += n_data_bytes_32.eq(bytes_per_fr)

        latched_mcs_32  = Signal(32)
        latched_rate_32 = Signal(32)
        self.comb += [
            latched_mcs_32.eq(latched_mcs),
            latched_rate_32.eq(latched_rate),
        ]

        # -------- AXI-Lite master helper signals --------
        wr_launch = Signal()
        wr_addr   = Signal(16)
        wr_data   = Signal(32)
        wr_done   = Signal()

        rd_launch = Signal()
        rd_addr   = Signal(16)
        rd_data   = Signal(32)
        rd_done   = Signal()

        # Inner AXI-Lite master FSM (runs one AW→W→B or AR→R cycle per launch).
        self.submodules.axi = axi = FSM(reset_state="IDLE")
        axi.act("IDLE",
            If(wr_launch, NextState("WR_AW")),
            If(rd_launch, NextState("RD_AR")),
        )
        axi.act("WR_AW",
            chain.ctrl_axi_awvalid.eq(1),
            chain.ctrl_axi_awaddr.eq(wr_addr),
            If(chain.ctrl_axi_awready, NextState("WR_W")),
        )
        axi.act("WR_W",
            chain.ctrl_axi_wvalid.eq(1),
            chain.ctrl_axi_wdata.eq(wr_data),
            chain.ctrl_axi_wstrb.eq(0xF),
            If(chain.ctrl_axi_wready, NextState("WR_B")),
        )
        axi.act("WR_B",
            chain.ctrl_axi_bready.eq(1),
            If(chain.ctrl_axi_bvalid,
                wr_done.eq(1),
                NextState("IDLE"),
            ),
        )
        axi.act("RD_AR",
            chain.ctrl_axi_arvalid.eq(1),
            chain.ctrl_axi_araddr.eq(rd_addr),
            If(chain.ctrl_axi_arready, NextState("RD_R")),
        )
        axi.act("RD_R",
            chain.ctrl_axi_rready.eq(1),
            If(chain.ctrl_axi_rvalid,
                NextValue(rd_data, chain.ctrl_axi_rdata),
                rd_done.eq(1),
                NextState("IDLE"),
            ),
        )

        # -------- Per-frame write table --------
        # Each entry: (ctrl_axi addr, 32-bit data source).
        # Step counter indexes this table; addr/data mux below.
        AP_START = Constant(1, 32)
        N_SYMS_C = Constant(self.N_SYMS, 32)

        write_table = [
            # TX config
            (self.BASE_TX_SCRAM + self.N_BYTES,      bytes_per_fr),
            (self.BASE_TX_CONV  + self.RATE,         latched_rate_32),
            (self.BASE_TX_CONV  + self.N_DATA_BYTES, n_data_bytes_32),
            (self.BASE_TX_INTLV + self.MOD,          latched_mcs_32),
            (self.BASE_TX_INTLV + self.N_SYMS_TX,    N_SYMS_C),
            (self.BASE_OFDM_TX  + self.MOD,          latched_mcs_32),
            (self.BASE_OFDM_TX  + self.N_SYMS_TX,    N_SYMS_C),
            # RX config (Phase 1 loopback: mirror of TX)
            (self.BASE_RX_SCRAM + self.N_BYTES,      bytes_per_fr),
            (self.BASE_VIT      + self.RATE,         latched_rate_32),
            (self.BASE_VIT      + self.N_DATA_BYTES, n_data_bytes_32),
            (self.BASE_RX_INTLV + self.MOD,          latched_mcs_32),
            (self.BASE_RX_INTLV + self.N_SYMS_TX,    N_SYMS_C),
            (self.BASE_SYNC     + self.N_SYMS_SYNC,  N_SYMS_C),
            # AP_START pulses — TX chain upstream→downstream, then RX chain
            (self.BASE_TX_SCRAM + self.AP_CTRL,      AP_START),
            (self.BASE_TX_CONV  + self.AP_CTRL,      AP_START),
            (self.BASE_TX_INTLV + self.AP_CTRL,      AP_START),
            (self.BASE_OFDM_TX  + self.AP_CTRL,      AP_START),
            (self.BASE_SYNC     + self.AP_CTRL,      AP_START),
            (self.BASE_CFO      + self.AP_CTRL,      AP_START),
            (self.BASE_OFDM_RX  + self.AP_CTRL,      AP_START),
            (self.BASE_RX_INTLV + self.AP_CTRL,      AP_START),
            (self.BASE_VIT      + self.AP_CTRL,      AP_START),
            (self.BASE_RX_SCRAM + self.AP_CTRL,      AP_START),
        ]
        N_STEPS = len(write_table)

        step      = Signal(max=N_STEPS + 1)
        step_addr = Signal(16)
        step_data = Signal(32)
        self.comb += [
            Case(step, {i: step_addr.eq(addr) for i, (addr, _) in enumerate(write_table)}),
            Case(step, {i: step_data.eq(data) for i, (_, data) in enumerate(write_table)}),
        ]

        # -------- Main sequencer FSM --------
        self.submodules.main = main = FSM(reset_state="IDLE")

        main.act("IDLE",
            self.state.status.eq(0),
            If(self.enable.storage,
                If(boot_done,
                    NextState("BURST_START"),
                ).Else(
                    NextState("BOOT_IS_RX_LAUNCH"),
                ),
            ),
        )
        # Boot-time one-shot: rx_interleaver.is_rx = 1
        main.act("BOOT_IS_RX_LAUNCH",
            self.state.status.eq(1),
            self.busy.status.eq(1),
            wr_addr.eq(self.BASE_RX_INTLV + self.IS_RX),
            wr_data.eq(1),
            wr_launch.eq(1),
            NextState("BOOT_IS_RX_WAIT"),
        )
        main.act("BOOT_IS_RX_WAIT",
            self.state.status.eq(1),
            self.busy.status.eq(1),
            wr_addr.eq(self.BASE_RX_INTLV + self.IS_RX),
            wr_data.eq(1),
            If(wr_done,
                NextValue(boot_done, 1),
                NextState("BURST_START"),
            ),
        )
        main.act("BURST_START",
            self.state.status.eq(2),
            self.busy.status.eq(1),
            NextValue(latched_mcs,  self.mcs.storage),
            NextValue(latched_rate, self.rate.storage),
            NextValue(bytes_remain, self.ring_bytes.storage),
            NextState("FRAME_CHECK"),
        )
        main.act("FRAME_CHECK",
            self.state.status.eq(3),
            self.busy.status.eq(1),
            If(bytes_remain == 0,
                NextState("BURST_DONE"),
            ).Else(
                NextValue(step, 0),
                NextState("WR_LAUNCH"),
            ),
        )
        main.act("WR_LAUNCH",
            self.state.status.eq(4),
            self.busy.status.eq(1),
            wr_addr.eq(step_addr),
            wr_data.eq(step_data),
            wr_launch.eq(1),
            NextState("WR_WAIT"),
        )
        main.act("WR_WAIT",
            self.state.status.eq(4),
            self.busy.status.eq(1),
            wr_addr.eq(step_addr),
            wr_data.eq(step_data),
            If(wr_done,
                If(step == N_STEPS - 1,
                    NextState("POLL_LAUNCH"),
                ).Else(
                    NextValue(step, step + 1),
                    NextState("WR_LAUNCH"),
                ),
            ),
        )
        main.act("POLL_LAUNCH",
            self.state.status.eq(5),
            self.busy.status.eq(1),
            rd_addr.eq(self.BASE_OFDM_TX + self.AP_CTRL),
            rd_launch.eq(1),
            NextState("POLL_WAIT"),
        )
        main.act("POLL_WAIT",
            self.state.status.eq(5),
            self.busy.status.eq(1),
            rd_addr.eq(self.BASE_OFDM_TX + self.AP_CTRL),
            If(rd_done,
                If(rd_data[1],  # ap_done bit
                    NextState("FRAME_END"),
                ).Else(
                    NextState("POLL_LAUNCH"),
                ),
            ),
        )
        main.act("FRAME_END",
            self.state.status.eq(6),
            self.busy.status.eq(1),
            NextValue(self.frame_count.status, self.frame_count.status + 1),
            If(bytes_remain <= bytes_per_fr,
                NextValue(bytes_remain, 0),
            ).Else(
                NextValue(bytes_remain, bytes_remain - bytes_per_fr),
            ),
            NextState("FRAME_CHECK"),
        )
        main.act("BURST_DONE",
            # busy=0 here signals the driver "done".  We park in this state
            # until the driver writes enable=0, then → IDLE to rearm.
            self.state.status.eq(8),
            self.busy.status.eq(0),
            If(~self.enable.storage, NextState("IDLE")),
        )


# =============================================================================
# CRG — sys (125 MHz) + idelay (200 MHz) + ofdm (100 MHz) + usb (60 MHz)
# =============================================================================
#
# Re-implemented locally (rather than reusing hallycon_m2sdr_target_v2._CRG) so
# we can add the 100 MHz `ofdm` clkout that drives the OFDM BD wrapper.  The
# rfic domain is driven inside AD9364PHY by the AD9364 DATA_CLK input buffer,
# so we don't declare it here.

class _CRG(Module):
    def __init__(self, platform, sys_clk_freq, ulpi_pads):
        from migen.genlib.resetsync import AsyncResetSynchronizer
        self.rst = Signal()
        self.clock_domains.cd_sys    = ClockDomain()
        self.clock_domains.cd_rfic   = ClockDomain()
        self.clock_domains.cd_idelay = ClockDomain()
        self.clock_domains.cd_ofdm   = ClockDomain()
        self.clock_domains.cd_usb    = ClockDomain()

        clk40 = platform.request("clk40")
        self.submodules.pll = pll = S7PLL(speedgrade=-2)
        self.comb += pll.reset.eq(self.rst)
        pll.register_clkin(clk40, 40e6)
        pll.create_clkout(self.cd_sys,    sys_clk_freq)
        pll.create_clkout(self.cd_idelay, 200e6)
        pll.create_clkout(self.cd_ofdm,   100e6)          # matches ofdm_chain BD
        self.submodules.idelayctrl = S7IDELAYCTRL(self.cd_idelay)

        # USB 60 MHz (same as v2 CRG)
        self.specials += Instance("BUFG", i_I=ulpi_pads.clk, o_O=ClockSignal("usb"))
        platform.add_period_constraint(ulpi_pads.clk, 1e9 / 60e6)


# =============================================================================
# BaseSoC
# =============================================================================

class BaseSoC(SoCCore):
    def __init__(self, sys_clk_freq=int(125e6), with_ofdm=True, **kwargs):
        platform = Platform()

        kwargs["cpu_type"]             = None
        kwargs["integrated_sram_size"] = 0
        kwargs["with_uart"]            = False
        kwargs["ident_version"]        = True

        ulpi_pads = platform.request("ulpi", 0)
        self.submodules.crg = _CRG(platform, sys_clk_freq, ulpi_pads)
        SoCCore.__init__(self, platform, sys_clk_freq,
            ident="Hallycon M2 SDR + OFDM HLS Chain", **kwargs)

        # ── Housekeeping ──
        self.submodules.leds = LedChaser(
            pads         = platform.request_all("user_led"),
            sys_clk_freq = sys_clk_freq,
        )
        self.submodules.hyperram = LiteHyperBus(platform.request("hyperram"))
        self.bus.add_slave("main_ram", self.hyperram.bus,
            SoCRegion(origin=0x40000000, size=0x800000))

        # ── PCIe Gen2 x2 + DMA ──
        self.submodules.pcie_phy = S7PCIEPHY(
            platform, platform.request("pcie_x2"),
            data_width=64, bar0_size=0x20000)
        self.submodules.pcie_endpoint = LitePCIeEndpoint(self.pcie_phy)
        self.submodules.pcie_dma0 = LitePCIeDMA(
            self.pcie_phy, self.pcie_endpoint,
            with_buffering=True, buffering_depth=8192, with_loopback=False)
        self.add_csr("pcie_dma0")

        # ── AD9364 RFIC ──
        self.submodules.ad9364 = AD9364Core(
            platform.request("ad9364_rfic"),
            platform.request("ad9364_spi"),
        )
        self.add_csr("ad9364")

        if with_ofdm:
            self._add_ofdm_chain(platform)
        else:
            # Direct DMA ↔ RFIC (v2-equivalent, for board bring-up).
            self.comb += [
                self.pcie_dma0.source.connect(self.ad9364.sink),
                self.ad9364.source.connect(self.pcie_dma0.sink),
            ]

        # ── On-chip utilities + JTAGBone + MSI ──
        self.icap = ICAP(); self.icap.add_reload()
        self.xadc = XADC(); self.dna = DNA()
        self.icap.add_timing_constraints(platform, sys_clk_freq, self.crg.cd_sys.clk)
        self.dna .add_timing_constraints(platform, sys_clk_freq, self.crg.cd_sys.clk)
        self.add_jtagbone()

        self.submodules.pcie_msi = LitePCIeMSI()
        self.comb += self.pcie_msi.source.connect(self.pcie_phy.msi)
        self.interrupts = {
            "PCIE_DMA0_WRITER": self.pcie_dma0.writer.irq,
            "PCIE_DMA0_READER": self.pcie_dma0.reader.irq,
        }
        for i, (name, irq) in enumerate(sorted(self.interrupts.items())):
            self.comb += self.pcie_msi.irqs[i].eq(irq)
            self.add_constant(name + "_INTERRUPT", i)

    # -------------------------------------------------------------------------
    def _add_ofdm_chain(self, platform):
        """Instantiate ofdm_chain_wrapper and splice it between DMA and AD9364."""

        # ── Add the BD's generated Verilog sources to the Vivado project ──
        # The Vivado BD script (create_ofdm_bd.tcl) writes a file list to
        # ip_repo/ofdm_chain.f that points at ofdm_chain_wrapper.v and all
        # of its sub-IP BD generated sources.  We splice that list into the
        # LiteX platform so `launch_runs synth_1` picks them up.
        repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        filelist  = os.path.join(repo_root, "ip_repo", "ofdm_chain.f")
        if not os.path.isfile(filelist):
            raise RuntimeError(
                f"ofdm_chain.f not found at {filelist}.\n"
                "Run `vivado -mode batch -source vivado/create_ofdm_bd.tcl` "
                "first to generate the BD wrapper and file list.")
        # IPs handled via read_ip/synth_ip — skip their flat Verilog stubs.
        ip_flow_ips = ["ofdm_rx_fft", "ofdm_tx_ifft", "ctrl_xbar"]
        with open(filelist) as f:
            for line in f:
                src = line.strip()
                if src and not src.startswith("#"):
                    if any(name in src for name in ip_flow_ips):
                        continue
                    platform.add_source(src)

        # HLS IP RTL — the BD synth stubs instantiate the actual HLS modules
        # (e.g. cfo_correct, ofdm_tx, scrambler …) so we must add every .v/.vh
        # from ip_repo/*/hdl/verilog/ as flat Verilog sources.
        import glob as _glob
        ip_repo = os.path.join(repo_root, "ip_repo")
        for ext in ("*.v", "*.vh"):
            for src in sorted(_glob.glob(os.path.join(ip_repo, "*", "hdl", "verilog", ext))):
                platform.add_source(src)

        # Xilinx xfft and smartconnect are VHDL / encrypted — they must go
        # through the Vivado IP flow (read_ip + synth_ip).  Other BD sub-IPs
        # (HLS blocks, xlconstant, axis_data_fifo) work as flat Verilog.
        bd_ip_dir = os.path.join(repo_root, "vivado", "ofdm_bd",
            "ofdm_bd.srcs", "sources_1", "bd", "ofdm_chain", "ip")
        for xci in sorted(_glob.glob(os.path.join(bd_ip_dir, "*", "*.xci"))):
            if any(name in os.path.basename(xci) for name in ip_flow_ips):
                platform.add_ip(xci)

        # ── Instantiate OFDM chain (in the 100 MHz `ofdm` domain) ──
        self.submodules.ofdm = ofdm = OFDMChainWrapper()

        # ── Lower MAC — autonomous per-frame CSR sequencer ──
        # Lives in the ofdm domain so its AXI-Lite master directly drives
        # ofdm_chain_wrapper.ctrl_axi (also ofdm-clocked via the BD's
        # smartconnect NUM_CLKS=1 config).  LiteX handles CSR CDC for the
        # CSRStorage/CSRStatus registers exposed to the sys-domain CSR bus.
        self.submodules.ofdm_mac = ClockDomainsRenamer("ofdm")(OFDMLowerMAC(ofdm))
        self.add_csr("ofdm_mac")

        # ── Host side:  sys (125 MHz) ↔ ofdm (100 MHz) ──
        # DMA is 64-bit @ sys; OFDM chain is 8-bit @ ofdm.  Width-convert
        # first (sys-domain Converter), then cross clocks via AsyncFIFO.
        self.submodules.dma_to_byte = stream.Converter(64, 8)
        self.submodules.byte_to_dma = stream.Converter( 8, 64)

        self.submodules.host_tx_cdc = stream.ClockDomainCrossing(
            layout=[("data", 8)], cd_from="sys", cd_to="ofdm", depth=32)
        self.submodules.host_rx_cdc = stream.ClockDomainCrossing(
            layout=[("data", 8)], cd_from="ofdm", cd_to="sys", depth=32)

        self.comb += [
            # Host → RFIC: DMA(64) → Conv(8) → CDC(sys→ofdm) → ofdm.host_tx
            self.pcie_dma0.source.connect(self.dma_to_byte.sink),
            self.dma_to_byte.source.connect(self.host_tx_cdc.sink),
            self.host_tx_cdc.source.connect(ofdm.host_tx),
            # RFIC → Host: ofdm.host_rx → CDC(ofdm→sys) → Conv(8→64) → DMA
            ofdm.host_rx.connect(self.host_rx_cdc.sink),
            self.host_rx_cdc.source.connect(self.byte_to_dma.sink),
            self.byte_to_dma.source.connect(self.pcie_dma0.sink),
        ]

        # ── RF side:  rfic (245.76 MHz) ↔ ofdm (100 MHz) ──
        # AD9364Core already does rfic↔sys CDC for dma_layout(64).  We take
        # its sys-domain endpoints, lane-reduce to 32 bits (channel A only),
        # then cross sys→ofdm.  Cleaner than touching AD9364Core internals.
        self.submodules.rf_rx_br = rx_br = _RFRxBridge()
        self.submodules.rf_tx_br = tx_br = _RFTxBridge()

        self.submodules.rf_rx_cdc = stream.ClockDomainCrossing(
            layout=[("data", 48)], cd_from="sys", cd_to="ofdm", depth=2048)
        self.submodules.rf_tx_cdc = stream.ClockDomainCrossing(
            layout=[("data", 48)], cd_from="ofdm", cd_to="sys", depth=2048)

        self.comb += [
            # AD9364 → OFDM: ad9364.source(sys,64b) → RxBridge → CDC(sys→ofdm) → ofdm.rf_rx
            self.ad9364.source.connect(rx_br.sink),
            rx_br.source.connect(self.rf_rx_cdc.sink),
            self.rf_rx_cdc.source.connect(ofdm.rf_rx),
            # OFDM → AD9364: ofdm.rf_tx → CDC(ofdm→sys) → TxBridge → ad9364.sink
            ofdm.rf_tx.connect(self.rf_tx_cdc.sink),
            self.rf_tx_cdc.source.connect(tx_br.sink),
            tx_br.source.connect(self.ad9364.sink),
        ]

        # ── sys ↔ ofdm false paths (AsyncFIFO gray pointers are safe) ──
        platform.toolchain.pre_placement_commands.add(
            "set_max_delay -datapath_only 8.0 "
            "-from [get_clocks main_crg_clkout0] "
            "-to   [get_clocks main_crg_clkout2]")
        platform.toolchain.pre_placement_commands.add(
            "set_max_delay -datapath_only 8.0 "
            "-from [get_clocks main_crg_clkout2] "
            "-to   [get_clocks main_crg_clkout0]")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Hallycon M2 SDR LiteX shell wrapping the ofdm_chain BD")
    parser.add_argument("--build",   action="store_true")
    parser.add_argument("--load",    action="store_true")
    parser.add_argument("--no-ofdm", action="store_true",
        help="Omit ofdm_chain_wrapper — direct DMA↔RFIC (v2 equivalent)")
    builder_args(parser)
    soc_core_args(parser)
    args = parser.parse_args()

    soc = BaseSoC(
        sys_clk_freq = int(125e6),
        with_ofdm    = not args.no_ofdm,
        **soc_core_argdict(args),
    )
    builder = Builder(soc, **builder_argdict(args))
    builder.build(run=args.build)

    generate_litepcie_software(soc, os.path.join(builder.output_dir, "software"))

    if args.load:
        prog = soc.platform.create_programmer()
        prog.load_bitstream(os.path.join(builder.gateware_dir, soc.build_name + ".bit"))


if __name__ == "__main__":
    main()
