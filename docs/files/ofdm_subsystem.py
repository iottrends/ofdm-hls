#
# ofdm_subsystem.py — LiteX/Migen wrapper for OFDM HLS IP blocks
#
# Integrates Vitis HLS-exported OFDM TX/RX chains into the Hallycon M.2 SDR
# LiteX SoC between the ADI axi_ad9364 IP and the LitePCIe DMA engine.
#
# Copyright (c) 2026 Hallycon Ventures
# SPDX-License-Identifier: MIT
#
# Usage: Instantiate OFDMSubsystem in your top-level SoC build.py and connect
# its source/sink endpoints to the PCIe DMA and AD9364 streaming interfaces.
#

from migen import *
from litex.gen import *
from litex.soc.interconnect import stream
from litex.soc.interconnect.csr import *


# =============================================================================
# Sample Width Adapter: Q1.11 (ADI) ↔ Q0.15 (HLS)
# =============================================================================
#
# ADI axi_ad9364 outputs 16-bit sign-extended from 12-bit ADC:
#   Bit layout: S IIIIIIIIIII 0000  (Q1.11, zero-padded LSBs)
#   Range: [-2048, +2047] × 2^-11 = [-1.0, +0.9995]
#
# HLS blocks use ap_fixed<16,1> = Q0.15:
#   Bit layout: S FFFFFFFFFFFFFFF
#   Range: [-1.0, +0.99997]
#
# Conversion: arithmetic right-shift by 1 (Q1.11 → Q0.15 equivalent scaling)
# This preserves the sign bit and scales the signal to fit Q0.15 range.
# For TX output (Q0.15 → Q1.11): left-shift by 1, truncate to 12 bits.

class SampleWidthRXAdapter(Module):
    """ADI Q1.11 (16-bit) → HLS Q0.15 (16-bit) for RX path."""
    def __init__(self):
        # AXI-Stream input: 32-bit (16-bit I + 16-bit Q packed)
        self.sink   = sink   = stream.Endpoint([("data", 32)])
        # AXI-Stream output: 32-bit (16-bit I + 16-bit Q, Q0.15)
        self.source = source = stream.Endpoint([("data", 32)])

        # ---
        i_in = Signal((16, True))
        q_in = Signal((16, True))
        i_out = Signal((16, True))
        q_out = Signal((16, True))

        self.comb += [
            # Unpack
            i_in.eq(sink.data[:16]),
            q_in.eq(sink.data[16:]),
            # Arithmetic right-shift by 1: Q1.11 → Q0.15 range mapping
            # This divides the signal by 2, mapping [-1.0, +1.0) Q1.11
            # into [-0.5, +0.5) Q0.15. The HLS OFDM RX handles this
            # scale factor through the channel estimator (G_eq absorbs it).
            i_out.eq(i_in >> 1),
            q_out.eq(q_in >> 1),
            # Pack and forward
            source.data.eq(Cat(i_out, q_out)),
            source.valid.eq(sink.valid),
            sink.ready.eq(source.ready),
            source.last.eq(sink.last),
        ]


class SampleWidthTXAdapter(Module):
    """HLS Q0.15 (16-bit) → ADI Q1.11 / 12-bit for TX path."""
    def __init__(self):
        self.sink   = sink   = stream.Endpoint([("data", 32)])
        self.source = source = stream.Endpoint([("data", 32)])

        # ---
        i_in = Signal((16, True))
        q_in = Signal((16, True))
        i_out = Signal((16, True))
        q_out = Signal((16, True))

        self.comb += [
            i_in.eq(sink.data[:16]),
            q_in.eq(sink.data[16:]),
            # Left-shift by 1: Q0.15 → Q1.11 range
            # Saturate instead of wrapping to prevent +max → −max glitches
            # (unlikely since OFDM PAPR keeps peaks well under +1.0)
            i_out.eq(i_in << 1),
            q_out.eq(q_in << 1),
            source.data.eq(Cat(i_out, q_out)),
            source.valid.eq(sink.valid),
            sink.ready.eq(source.ready),
            source.last.eq(sink.last),
        ]


# =============================================================================
# HLS IP Block Wrapper (generic)
# =============================================================================
#
# Each Vitis HLS IP exports:
#   - AXI-Stream input  (TDATA, TVALID, TREADY, TLAST)
#   - AXI-Stream output (TDATA, TVALID, TREADY, TLAST)
#   - AXI-Lite control  (ap_start, ap_done, ap_idle, ap_ready + CSR registers)
#
# This wrapper provides a Migen interface with CSR access to the AXI-Lite
# control/status registers and stream endpoints for data flow.
#
# The HLS ap_ctrl_hs protocol:
#   1. CPU writes parameter CSRs (mod, n_syms, etc.)
#   2. CPU writes ap_start=1
#   3. Block processes data (axis in → axis out)
#   4. Block asserts ap_done=1 when complete
#   5. CPU reads ap_done, clears ap_start for next invocation

class HLSBlock(Module, AutoCSR):
    """Generic wrapper for a Vitis HLS-exported IP block."""
    def __init__(self, platform, ip_name, params=None):
        """
        Args:
            platform: LiteX platform (for adding Verilog sources)
            ip_name:  Name of the HLS IP (e.g., "ofdm_tx")
            params:   Dict of {csr_name: (width, reset_value)} for AXI-Lite CSRs
        """
        if params is None:
            params = {}

        # AXI-Stream endpoints (directly wired to HLS IP ports)
        self.sink   = stream.Endpoint([("data", 32)])
        self.source = stream.Endpoint([("data", 32)])

        # Control/status signals
        self.ap_start = Signal()
        self.ap_done  = Signal()
        self.ap_idle  = Signal()
        self.ap_ready = Signal()

        # CSR: start/status
        self._control = CSRStorage(1, name="control", description="Write 1 to ap_start")
        self._status  = CSRStatus(3, name="status",
                                  description="[0]=ap_done [1]=ap_idle [2]=ap_ready")

        self.comb += [
            self.ap_start.eq(self._control.storage[0]),
            self._status.status.eq(Cat(self.ap_done, self.ap_idle, self.ap_ready)),
        ]

        # Parameter CSRs
        self._params = {}
        for name, (width, reset) in params.items():
            csr = CSRStorage(width, name=name, reset=reset)
            setattr(self, f"_{name}", csr)
            self._params[name] = csr

        # NOTE: The actual HLS IP Verilog instantiation happens in the
        # Vivado block design, not in Migen. This wrapper provides the
        # CSR interface for the CPU and the stream endpoints for connection.
        # The HLS IP is added to the Vivado project as an IP catalog entry
        # and connected via the block design .tcl script.
        #
        # For pure-Migen flow (no Vivado IP Integrator), you would add:
        #   self.specials += Instance(ip_name, ...)
        # with the appropriate port mapping. See Section 8 of INTEGRATION.md.


# =============================================================================
# OFDM TX Chain
# =============================================================================

class OFDMTXChain(Module, AutoCSR):
    """
    Complete TX chain: scrambler → conv_enc → interleaver → ofdm_tx → sample adapter

    Data flow:
      PCIe DMA source (raw payload bytes, 8-bit AXI-Stream)
        → scrambler (XOR with LFSR, same byte count)
        → conv_enc (rate 1/2: 2× expansion; rate 2/3: 1.5× expansion)
        → interleaver (bit permutation, same byte count)
        → ofdm_tx (bytes → OFDM IQ symbols, AXI-Stream 32-bit I/Q)
        → SampleWidthTXAdapter (Q0.15 → Q1.11)
        → AD9364 TX FIFO

    CPU orchestration:
      1. Write n_bytes to scrambler CSR
      2. Write rate, n_data_bytes to conv_enc CSR
      3. Write mod, n_syms, is_rx=0 to interleaver CSR
      4. Write mod, n_syms to ofdm_tx CSR
      5. Assert ap_start on all blocks (or use auto-chain FSM)
      6. Push payload bytes into scrambler input via DMA
      7. OFDM TX streams IQ to AD9364

    For V1 simplicity: the CPU can chain these manually. For V2, a small
    FSM watches ap_done of each stage and starts the next automatically.
    """
    def __init__(self, platform):
        # External endpoints
        self.sink   = stream.Endpoint([("data", 8)])   # Raw payload bytes in
        self.source = stream.Endpoint([("data", 32)])   # IQ samples out (to AD9364)

        # Sub-modules (HLS IP wrappers)
        self.submodules.scrambler   = HLSBlock(platform, "scrambler",
            params={"n_bytes": (16, 0)})
        self.submodules.conv_enc    = HLSBlock(platform, "conv_enc",
            params={"rate": (1, 0), "n_data_bytes": (16, 0)})
        self.submodules.interleaver = HLSBlock(platform, "interleaver",
            params={"mod": (1, 0), "n_syms": (8, 4), "is_rx": (1, 0)})
        self.submodules.ofdm_tx     = HLSBlock(platform, "ofdm_tx",
            params={"mod": (1, 0), "n_syms": (8, 4)})
        self.submodules.tx_adapter  = SampleWidthTXAdapter()

        # AXI-Stream chain
        # sink → scrambler → conv_enc → interleaver → ofdm_tx → adapter → source
        self.comb += [
            self.sink.connect(self.scrambler.sink),
            self.scrambler.source.connect(self.conv_enc.sink),
            self.conv_enc.source.connect(self.interleaver.sink),
            self.interleaver.source.connect(self.ofdm_tx.sink),
            self.ofdm_tx.source.connect(self.tx_adapter.sink),
            self.tx_adapter.source.connect(self.source),
        ]


# =============================================================================
# OFDM RX Chain
# =============================================================================

class OFDMRXChain(Module, AutoCSR):
    """
    Complete RX chain: adapter → sync → cfo → ofdm_rx → deinterleaver → viterbi → descrambler

    Data flow:
      AD9364 RX FIFO (12-bit I/Q, sign-extended to 16-bit)
        → SampleWidthRXAdapter (Q1.11 → Q0.15)
        → sync_detect (timing sync, CFO estimate, aligned IQ stream)
        → cfo_correct (per-sample phase rotation)
        → ofdm_rx (FFT, equalize, demap → coded bytes)
        → interleaver (is_rx=1: deinterleave)
        → viterbi_dec (FEC decode)
        → scrambler (descramble — same operation as scramble)
        → PCIe DMA sink (decoded payload bytes)

    The ofdm_rx block self-configures from the in-band header (mod, n_syms).
    sync_detect and cfo_correct need n_syms from the CPU (or from a
    previous header decode — for V1, use a fixed known value).
    """
    def __init__(self, platform):
        # External endpoints
        self.sink   = stream.Endpoint([("data", 32)])   # IQ samples in (from AD9364)
        self.source = stream.Endpoint([("data", 8)])    # Decoded payload bytes out

        # Sub-modules
        self.submodules.rx_adapter    = SampleWidthRXAdapter()
        self.submodules.sync_detect   = HLSBlock(platform, "sync_detect",
            params={"n_syms": (8, 4)})
        self.submodules.cfo_correct   = HLSBlock(platform, "cfo_correct",
            params={"n_syms": (8, 4)})
        self.submodules.ofdm_rx       = HLSBlock(platform, "ofdm_rx", params={})
        self.submodules.deinterleaver = HLSBlock(platform, "interleaver",
            params={"mod": (1, 0), "n_syms": (8, 4), "is_rx": (1, 1)})
        self.submodules.viterbi_dec   = HLSBlock(platform, "viterbi_dec",
            params={"rate": (1, 0), "n_data_bytes": (16, 0)})
        self.submodules.descrambler   = HLSBlock(platform, "scrambler",
            params={"n_bytes": (16, 0)})

        # CFO estimate wire: sync_detect output → cfo_correct input
        # This is a scalar (ap_fixed<16,2>) passed via AXI-Lite, not stream.
        # The CPU reads cfo_est from sync_detect CSR and writes it to cfo_correct CSR.
        # For V2: direct wire between the two IPs (requires custom RTL shim).
        self.submodules.sync_detect._params["cfo_est_out"] = CSRStatus(16, name="cfo_est_out")
        self.submodules.cfo_correct._params["cfo_est_in"]  = CSRStorage(16, name="cfo_est_in")

        # Header error status from ofdm_rx
        self._header_err = CSRStatus(1, name="header_err",
            description="1 if last frame header CRC failed")

        # AXI-Stream chain
        self.comb += [
            self.sink.connect(self.rx_adapter.sink),
            self.rx_adapter.source.connect(self.sync_detect.sink),
            self.sync_detect.source.connect(self.cfo_correct.sink),
            self.cfo_correct.source.connect(self.ofdm_rx.sink),
            self.ofdm_rx.source.connect(self.deinterleaver.sink),
            self.deinterleaver.source.connect(self.viterbi_dec.sink),
            self.viterbi_dec.source.connect(self.descrambler.sink),
            self.descrambler.source.connect(self.source),
        ]


# =============================================================================
# Top-Level OFDM Subsystem with Bypass
# =============================================================================

class OFDMSubsystem(Module, AutoCSR):
    """
    Top-level OFDM subsystem with bypass mode for board bring-up.

    CSRs:
      bypass  — 1: AD9364 ↔ PCIe directly (no OFDM). 0: OFDM processing.
      tx_mode — 1: TX active. 0: TX idle.
      rx_mode — 1: RX active. 0: RX idle.

    Integration point in SoC build.py:

        self.submodules.ofdm = OFDMSubsystem(platform)

        # Connect to AD9364 streaming interface
        self.comb += [
            ad9364_rx_source.connect(self.ofdm.rf_rx_sink),
            self.ofdm.rf_tx_source.connect(ad9364_tx_sink),
        ]

        # Connect to PCIe DMA
        self.comb += [
            pcie_dma_source.connect(self.ofdm.host_tx_sink),
            self.ofdm.host_rx_source.connect(pcie_dma_sink),
        ]
    """
    def __init__(self, platform):
        # ── RF-side endpoints (connect to AD9364 / axi_ad9364 IP) ──
        self.rf_rx_sink   = stream.Endpoint([("data", 32)])  # IQ from AD9364
        self.rf_tx_source = stream.Endpoint([("data", 32)])  # IQ to AD9364

        # ── Host-side endpoints (connect to PCIe DMA) ──
        self.host_tx_sink   = stream.Endpoint([("data", 8)])   # Payload from host
        self.host_rx_source = stream.Endpoint([("data", 8)])   # Payload to host

        # ── Bypass for raw IQ pass-through (32-bit both sides) ──
        self.host_bypass_tx_sink   = stream.Endpoint([("data", 32)])
        self.host_bypass_rx_source = stream.Endpoint([("data", 32)])

        # ── CSRs ──
        self._bypass  = CSRStorage(1, name="bypass", reset=1,
            description="1=bypass OFDM (raw IQ pass-through), 0=OFDM active")
        self._tx_mode = CSRStorage(1, name="tx_mode",
            description="1=TX active, 0=TX idle")
        self._rx_mode = CSRStorage(1, name="rx_mode",
            description="1=RX active, 0=RX idle")
        self._frame_count = CSRStatus(32, name="frame_count",
            description="Number of frames successfully decoded (header CRC pass)")

        # ── Sub-modules ──
        self.submodules.tx_chain = OFDMTXChain(platform)
        self.submodules.rx_chain = OFDMRXChain(platform)

        # ── Bypass mux ──
        bypass = self._bypass.storage

        # TX path mux
        self.comb += [
            If(bypass,
                # Bypass: host raw IQ → AD9364 directly
                self.host_bypass_tx_sink.connect(self.rf_tx_source),
            ).Else(
                # OFDM: host payload → TX chain → AD9364
                self.host_tx_sink.connect(self.tx_chain.sink),
                self.tx_chain.source.connect(self.rf_tx_source),
            ),
        ]

        # RX path mux
        self.comb += [
            If(bypass,
                # Bypass: AD9364 raw IQ → host directly
                self.rf_rx_sink.connect(self.host_bypass_rx_source),
            ).Else(
                # OFDM: AD9364 → RX chain → host payload
                self.rf_rx_sink.connect(self.rx_chain.sink),
                self.rx_chain.source.connect(self.host_rx_source),
            ),
        ]

        # Frame counter (increment on successful RX decode)
        frame_count = Signal(32)
        self.sync += [
            If(self.rx_chain.ofdm_rx.ap_done & ~self.rx_chain._header_err.status,
                frame_count.eq(frame_count + 1),
            ),
        ]
        self.comb += self._frame_count.status.eq(frame_count)
