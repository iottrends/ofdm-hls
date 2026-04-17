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
from litex.soc.interconnect.axi     import (
    AXILiteInterface, AXIInterface, AXI2AXILite, AXILiteInterconnectShared,
    AXILiteClockDomainCrossing,
)
from litex.compat.soc_core          import mem_decoder
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
#   host_tx_in    AXI-Stream,  8-bit + TLAST  (host payload → ofdm_mac)
#   host_rx_out   AXI-Stream,  8-bit + TLAST  (ofdm_mac → host)
#   rf_tx_out     AXI-Stream, 48-bit          (ofdm_tx iq_out → RFIC; HLS iq_t)
#   rf_rx_in      AXI-Stream, 40-bit          (RFIC → adc_fifo → sync_detect)
#   ctrl_tx_chain / ctrl_ofdm_tx / ctrl_sync_det / ctrl_ofdm_rx / ctrl_ofdm_mac
#       AXI4-Lite slaves (one per HLS block; 4 KB addr space each)
#   mac_csr_master   AXI3 full-AXI master (ofdm_mac.m_axi_csr_master)
#   mac_tx_done_pulse / mac_rx_pkt_pulse  (MSI interrupt strobes, ofdm domain)
#
# The combined ofdm_mac HLS block handles MAC framing (header/FCS/filter)
# AND per-frame PHY CSR sequencing (via its m_axi master, routed by LiteX).

class OFDMChainWrapper(Module):
    """Opaque Instance() of ofdm_chain_wrapper.v.

    Two clock domains:
      - `ofdm`     (100 MHz) drives BD port `clk`
      - `ofdm_fec` (200 MHz) drives BD port `clk_fec` (fec_rx only)

    Host-facing AXIS byte endpoints carry TLAST (packet framing).  RF-side
    48-bit IQ streams do not (continuous sample streams).

    Control plane (post-2026-04-17 architecture — see mac.md §Phase 1.5):
    the BD exposes each HLS block's s_axi_ctrl / s_axi_stat as a separate
    AXI-Lite slave on the wrapper, plus ofdm_mac's m_axi_csr_master as a
    full-AXI master.  shell.py's BaseSoC instantiates an
    AXILiteInterconnectShared (pure Verilog; no Xilinx xbar IP) to route
    2 masters (host PCIe BAR + MAC m_axi) to the 5 CSR slaves."""
    def __init__(self):
        # Host byte streams (ofdm domain, 8-bit; TLAST via built-in `last` field)
        self.host_tx  = stream.Endpoint([("data",  8)])
        self.host_rx  = stream.Endpoint([("data",  8)])
        # RF IQ streams (ofdm domain; no TLAST sideband)
        # TX: 48-bit (ofdm_tx iq_t, to be reduced to 40-bit)
        # RX: 40-bit (sync_detect iq_in via 5-byte adc_input_fifo)
        self.rf_tx    = stream.Endpoint([("data", 48)])
        self.rf_rx    = stream.Endpoint([("data", 40)])

        # MAC interrupt pulses out of the BD (ofdm domain)
        self.mac_tx_done_pulse = Signal()
        self.mac_rx_pkt_pulse  = Signal()

        # Per-block CSR ports (post-2026-04-17 architecture — BD has no xbar).
        # Each HLS block's s_axi_ctrl/s_axi_stat is a 4 KB AXI-Lite slave on
        # the wrapper.  shell.py's AXILiteInterconnectShared routes host +
        # MAC traffic to these slaves; see mac.md §Phase 1.5.
        #
        # clock_domain="ofdm" — all wrapper CSR ports run on the wrapper's
        # `clk` input = 100 MHz ofdm clock. Matters for LiteX bus CDC inference.
        self.ctrl_tx_chain  = AXILiteInterface(data_width=32, address_width=12, clock_domain="ofdm")
        self.ctrl_ofdm_tx   = AXILiteInterface(data_width=32, address_width=12, clock_domain="ofdm")
        self.ctrl_sync_det  = AXILiteInterface(data_width=32, address_width=12, clock_domain="ofdm")
        self.ctrl_ofdm_rx   = AXILiteInterface(data_width=32, address_width=12, clock_domain="ofdm")
        self.ctrl_ofdm_mac  = AXILiteInterface(data_width=32, address_width=12, clock_domain="ofdm")

        # MAC's m_axi_csr_master — full AXI (aximm), version=axi3 to match the
        # 2-bit AWLOCK we saw in the earlier BD validation. id_width=1 is the
        # HLS default. MAC lives in ofdm domain. Shell.py wraps this with
        # AXI2AXILite for the xbar.
        self.mac_csr_master = AXIInterface(
            data_width=32, address_width=16, id_width=1, version="axi3",
            clock_domain="ofdm")

        # Helper: Instance() port map for an AXI-Lite SLAVE on the wrapper.
        # Wrapper receives aw/w/ar (inputs) and sends b/r + per-channel ready
        # back to the master. Covers all 19 AXI-Lite signals.
        def _axil_slave_ports(prefix, iface):
            # HLS-generated s_axi_ctrl slaves do NOT expose awprot/arprot —
            # Vivado strips them at make_bd_intf_pins_external time. We keep
            # iface.aw.prot internally in LiteX for protocol compliance, but
            # don't wire it through the Instance.
            return {
                f"i_{prefix}_awaddr":  iface.aw.addr,
                f"i_{prefix}_awvalid": iface.aw.valid,
                f"o_{prefix}_awready": iface.aw.ready,
                f"i_{prefix}_wdata":   iface.w.data,
                f"i_{prefix}_wstrb":   iface.w.strb,
                f"i_{prefix}_wvalid":  iface.w.valid,
                f"o_{prefix}_wready":  iface.w.ready,
                f"o_{prefix}_bresp":   iface.b.resp,
                f"o_{prefix}_bvalid":  iface.b.valid,
                f"i_{prefix}_bready":  iface.b.ready,
                f"i_{prefix}_araddr":  iface.ar.addr,
                f"i_{prefix}_arvalid": iface.ar.valid,
                f"o_{prefix}_arready": iface.ar.ready,
                f"o_{prefix}_rdata":   iface.r.data,
                f"o_{prefix}_rresp":   iface.r.resp,
                f"o_{prefix}_rvalid":  iface.r.valid,
                f"i_{prefix}_rready":  iface.r.ready,
            }

        # Helper: Instance() port map for an AXI3 full-AXI MASTER on the wrapper.
        # Wrapper drives aw/w/ar (outputs) and receives b/r (inputs).
        # AXI3 has WID (dropped on AXI4); AWLOCK/ARLOCK are 2-bit (vs 1-bit on AXI4).
        def _axi_master_ports(prefix, iface):
            return {
                # aw (outputs)
                f"o_{prefix}_awid":    iface.aw.id,
                f"o_{prefix}_awaddr":  iface.aw.addr,
                f"o_{prefix}_awlen":   iface.aw.len,
                f"o_{prefix}_awsize":  iface.aw.size,
                f"o_{prefix}_awburst": iface.aw.burst,
                f"o_{prefix}_awlock":  iface.aw.lock,
                f"o_{prefix}_awcache":  iface.aw.cache,
                f"o_{prefix}_awprot":   iface.aw.prot,
                f"o_{prefix}_awqos":    iface.aw.qos,
                f"o_{prefix}_awregion": iface.aw.region,
                f"o_{prefix}_awvalid":  iface.aw.valid,
                f"i_{prefix}_awready":  iface.aw.ready,
                # w (outputs)
                f"o_{prefix}_wid":     iface.w.id,       # AXI3 only; HLS emits it
                f"o_{prefix}_wdata":   iface.w.data,
                f"o_{prefix}_wstrb":   iface.w.strb,
                f"o_{prefix}_wlast":   iface.w.last,
                f"o_{prefix}_wvalid":  iface.w.valid,
                f"i_{prefix}_wready":  iface.w.ready,
                # b (inputs)
                f"i_{prefix}_bid":     iface.b.id,
                f"i_{prefix}_bresp":   iface.b.resp,
                f"i_{prefix}_bvalid":  iface.b.valid,
                f"o_{prefix}_bready":  iface.b.ready,
                # ar (outputs)
                f"o_{prefix}_arid":    iface.ar.id,
                f"o_{prefix}_araddr":  iface.ar.addr,
                f"o_{prefix}_arlen":   iface.ar.len,
                f"o_{prefix}_arsize":  iface.ar.size,
                f"o_{prefix}_arburst": iface.ar.burst,
                f"o_{prefix}_arlock":  iface.ar.lock,
                f"o_{prefix}_arcache":  iface.ar.cache,
                f"o_{prefix}_arprot":   iface.ar.prot,
                f"o_{prefix}_arqos":    iface.ar.qos,
                f"o_{prefix}_arregion": iface.ar.region,
                f"o_{prefix}_arvalid":  iface.ar.valid,
                f"i_{prefix}_arready":  iface.ar.ready,
                # r (inputs)
                f"i_{prefix}_rid":     iface.r.id,
                f"i_{prefix}_rdata":   iface.r.data,
                f"i_{prefix}_rresp":   iface.r.resp,
                f"i_{prefix}_rlast":   iface.r.last,
                f"i_{prefix}_rvalid":  iface.r.valid,
                f"o_{prefix}_rready":  iface.r.ready,
            }

        self.specials += Instance("ofdm_chain_wrapper",
            i_clk       = ClockSignal("ofdm"),
            i_clk_fec   = ClockSignal("ofdm_fec"),
            i_rst_n     = ~ResetSignal("ofdm"),
            i_rst_fec_n = ~ResetSignal("ofdm_fec"),

            # host_tx_in  (slave, with TLAST + TKEEP/TSTRB)
            i_host_tx_in_tdata  = self.host_tx.data,
            i_host_tx_in_tlast  = self.host_tx.last,
            i_host_tx_in_tkeep  = self.host_tx.valid,  # byte valid when stream valid
            i_host_tx_in_tstrb  = self.host_tx.valid,   # data (not position) byte
            i_host_tx_in_tvalid = self.host_tx.valid,
            o_host_tx_in_tready = self.host_tx.ready,

            # host_rx_out (master, with TLAST). HLS ties tkeep/tstrb high whenever
            # tvalid is asserted; we receive them into unused Signals so Vivado
            # doesn't warn about unconnected wrapper outputs.
            o_host_rx_out_tdata  = self.host_rx.data,
            o_host_rx_out_tlast  = self.host_rx.last,
            o_host_rx_out_tkeep  = Signal(),
            o_host_rx_out_tstrb  = Signal(),
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

            # MAC interrupt pulses (ofdm domain)
            o_mac_tx_done_pulse = self.mac_tx_done_pulse,
            o_mac_rx_pkt_pulse  = self.mac_rx_pkt_pulse,

            # 5 AXI-Lite CSR slaves (driven by shell.py's AXILiteInterconnectShared)
            **_axil_slave_ports("ctrl_tx_chain", self.ctrl_tx_chain),
            **_axil_slave_ports("ctrl_ofdm_tx",  self.ctrl_ofdm_tx),
            **_axil_slave_ports("ctrl_sync_det", self.ctrl_sync_det),
            **_axil_slave_ports("ctrl_ofdm_rx",  self.ctrl_ofdm_rx),
            **_axil_slave_ports("ctrl_ofdm_mac", self.ctrl_ofdm_mac),

            # MAC's m_axi master (AXI3 full AXI; shell.py adapts to AXI-Lite)
            **_axi_master_ports("mac_csr_master", self.mac_csr_master),
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
# both RF lanes (2R2T).  We're 1R1T so lane B is dropped/zeroed.

class _RFRxBridge(Module):
    """AD9364 (64b dma_layout) → ofdm_chain.rf_rx_in (40b). 1R1T → lane A."""
    def __init__(self):
        self.sink   = stream.Endpoint(dma_layout(64))           # ia|qa|ib|qb
        self.source = stream.Endpoint([("data", 40)])           # {i:16, q:16, pad:8}
        self.comb += [
            self.source.valid.eq(self.sink.valid),
            self.source.data[ 0:16].eq(self.sink.data[ 0:16]),  # i ← ia
            self.source.data[16:32].eq(self.sink.data[16:32]),  # q ← qa
            self.source.data[32:40].eq(0),                      # pad byte: tied 0
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
# CRG — sys (125 MHz) + idelay (200 MHz) + ofdm (100 MHz) + ofdm_fec (200 MHz) + usb (60 MHz)
# =============================================================================
#
# `ofdm`     (100 MHz) drives the OFDM BD wrapper's `clk` port.
# `ofdm_fec` (200 MHz) drives the BD wrapper's `clk_fec` port for fec_rx
#            (viterbi v3 runs at 5 ns to keep up with 16-QAM rate-2/3).

class _CRG(Module):
    def __init__(self, platform, sys_clk_freq, ulpi_pads):
        from migen.genlib.resetsync import AsyncResetSynchronizer
        self.rst = Signal()
        self.clock_domains.cd_sys      = ClockDomain("sys")
        self.clock_domains.cd_rfic     = ClockDomain("rfic")
        self.clock_domains.cd_idelay   = ClockDomain("idelay")
        self.clock_domains.cd_ofdm     = ClockDomain("ofdm")
        self.clock_domains.cd_ofdm_fec = ClockDomain("ofdm_fec")
        self.clock_domains.cd_usb      = ClockDomain("usb")

        clk40 = platform.request("clk40")
        self.submodules.pll = pll = S7PLL(speedgrade=-2)
        self.comb += pll.reset.eq(self.rst)
        pll.register_clkin(clk40, 40e6)
        pll.create_clkout(self.cd_sys,      sys_clk_freq)
        pll.create_clkout(self.cd_idelay,   200e6)
        pll.create_clkout(self.cd_ofdm,     100e6)  # ofdm_chain BD clk
        pll.create_clkout(self.cd_ofdm_fec, 200e6)  # ofdm_chain BD clk_fec (fec_rx)
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
        if with_ofdm:
            # MAC TX/RX interrupts surfaced by OFDMChainWrapper (already CDC'd to sys).
            self.interrupts["OFDM_MAC_TX_DONE"] = self.ofdm_mac_tx_irq
            self.interrupts["OFDM_MAC_RX_PKT"]  = self.ofdm_mac_rx_irq
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
        # (ctrl_xbar removed 2026-04-17 — BD no longer contains a smartconnect;
        # LiteX routes CSRs natively via AXILiteInterconnectShared. See mac.md
        # §Phase 1.5 and create_ofdm_bd.tcl for the post-pivot architecture.)
        ip_flow_ips = ["ofdm_rx_fft", "ofdm_tx_ifft"]
        with open(filelist) as f:
            for line in f:
                src = line.strip()
                if src and not src.startswith("#"):
                    if any(name in src for name in ip_flow_ips):
                        continue
                    if "/sim/" in src:
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

        # Xilinx xfft IPs are encrypted — they must go through the Vivado
        # IP flow (read_ip + synth_ip).  Other BD sub-IPs (HLS blocks,
        # xlconstant, axis_data_fifo, smartconnect) work as flat Verilog.
        bd_ip_dir = os.path.join(repo_root, "vivado", "ofdm_bd",
            "ofdm_bd.srcs", "sources_1", "bd", "ofdm_chain", "ip")
        for xci in sorted(_glob.glob(os.path.join(bd_ip_dir, "*", "*.xci"))):
            if any(name in os.path.basename(xci) for name in ip_flow_ips):
                platform.add_ip(xci)

        # ── Instantiate OFDM chain (in the 100 MHz `ofdm` domain) ──
        self.submodules.ofdm = ofdm = OFDMChainWrapper()

        # ── PHY CSR interconnect (LiteX-native, replaces the BD smartconnect) ──
        # Routes 2 masters → 5 slaves over AXI-Lite:
        #   Master 0: host (PCIe BAR → LiteX wb bus → auto Wishbone2AXILite)
        #   Master 1: ofdm_mac.m_axi_csr_master (AXI3 full → AXI2AXILite adapted)
        #   Slave N : ctrl_<block>  at   0x_N000  (4 KB each)
        # See mac.md §Phase 1.5 for architecture rationale.

        # Interconnect lives in ofdm domain (same as MAC + all CSR slaves).
        # Host path crosses sys→ofdm via an explicit AXILiteClockDomainCrossing.

        # (a) Adapt MAC's full AXI3 to AXI-Lite for the interconnect.
        # Both sides in ofdm domain — no CDC needed here.
        # AXILiteInterface is a signal bundle — plain attribute, NOT submodule.
        mac_csr_axil = AXILiteInterface(
            data_width=32, address_width=16, clock_domain="ofdm")
        self.mac_csr_axil = mac_csr_axil
        self.submodules.mac_axi2axil = AXI2AXILite(ofdm.mac_csr_master, mac_csr_axil)

        # (b) Host-side AXI-Lite endpoint (sys domain — from wishbone).
        # `self.bus.add_slave` auto-inserts Wishbone2AXILite in sys.
        # address_width=32 matches the wishbone bus; strip_origin=True
        # subtracts 0x5000_0000 so local 0x0000..0x7FFF offsets work.
        phy_csr_host_sys = AXILiteInterface(
            data_width=32, address_width=32, clock_domain="sys")
        self.phy_csr_host_sys = phy_csr_host_sys
        self.bus.add_slave("phy_csr", phy_csr_host_sys,
            SoCRegion(origin=0x5000_0000, size=0x8000),
            strip_origin=True)

        # (c) Cross the host path from sys (125 MHz) → ofdm (100 MHz) so both
        # masters feed the interconnect in the same clock domain. Async FIFO
        # depth is LiteX's default (safe for occasional CPU-triggered writes).
        phy_csr_host_ofdm = AXILiteInterface(
            data_width=32, address_width=32, clock_domain="ofdm")
        self.phy_csr_host_ofdm = phy_csr_host_ofdm
        self.submodules.phy_csr_host_cdc = AXILiteClockDomainCrossing(
            master=phy_csr_host_sys, slave=phy_csr_host_ofdm,
            cd_from="sys", cd_to="ofdm")

        # (d) 2×5 shared interconnect, all in ofdm domain.
        # Address map mirrors the old BD smartconnect layout:
        #   0x0000 tx_chain   0x3000 ofdm_rx
        #   0x1000 ofdm_tx    0x4000 ofdm_mac
        #   0x2000 sync_det
        self.submodules.phy_csr_xbar = AXILiteInterconnectShared(
            masters = [phy_csr_host_ofdm, mac_csr_axil],
            slaves  = [
                (mem_decoder(0x0000, size=0x1000), ofdm.ctrl_tx_chain),
                (mem_decoder(0x1000, size=0x1000), ofdm.ctrl_ofdm_tx),
                (mem_decoder(0x2000, size=0x1000), ofdm.ctrl_sync_det),
                (mem_decoder(0x3000, size=0x1000), ofdm.ctrl_ofdm_rx),
                (mem_decoder(0x4000, size=0x1000), ofdm.ctrl_ofdm_mac),
            ],
        )

        # ── MAC interrupts → MSI ──
        # Two new IRQ lines from the HLS ofdm_mac block are pulse-synchronised
        # into `sys` for the PCIe MSI aggregator (which is sys-domain).
        from migen.genlib.cdc import PulseSynchronizer
        self.submodules.mac_tx_done_ps = ps_tx = PulseSynchronizer("ofdm", "sys")
        self.submodules.mac_rx_pkt_ps  = ps_rx = PulseSynchronizer("ofdm", "sys")
        self.comb += [
            ps_tx.i.eq(ofdm.mac_tx_done_pulse),
            ps_rx.i.eq(ofdm.mac_rx_pkt_pulse),
        ]
        self.ofdm_mac_tx_irq = ps_tx.o
        self.ofdm_mac_rx_irq = ps_rx.o

        # ── Host side:  sys (125 MHz) ↔ ofdm (100 MHz) ──
        # DMA is 64-bit @ sys; OFDM chain is 8-bit @ ofdm.  Width-convert
        # first (sys-domain Converter), then cross clocks via AsyncFIFO.
        # Both directions preserve TLAST (packet framing) end-to-end.
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
            layout=[("data", 40)], cd_from="sys", cd_to="ofdm", depth=2048)
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
            "-from [get_clocks main_crg_s7pll0] "
            "-to   [get_clocks main_crg_s7pll4]")
        platform.toolchain.pre_placement_commands.add(
            "set_max_delay -datapath_only 8.0 "
            "-from [get_clocks main_crg_s7pll4] "
            "-to   [get_clocks main_crg_s7pll0]")

        # ── ofdm ↔ ofdm_fec: fully async (axis_clock_converter pair) ──
        # fec_cc1 (100→200) and fec_cc2 (200→100) are the only crossings; their
        # xpm_fifo_axis internals already scope CDC exceptions via ASYNC_REG,
        # but a top-level clock group prevents phys_opt_design from retiming
        # across the boundary and covers any paths the XPM scope misses.
        platform.toolchain.pre_placement_commands.add(
            "set_clock_groups -asynchronous "
            "-group [get_clocks main_crg_s7pll4] "
            "-group [get_clocks main_crg_s7pll6]")


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
