#
# This file is part of Hallycon M2 SDR.
#
# Copyright (c) 2026 Hallycon Ventures
# Based on LiteX-M2SDR platform by Enjoy-Digital <enjoy-digital.fr>
# SPDX-License-Identifier: BSD-2-Clause
#
# Board: Hallycon M2 SDR (M.2 2280)
# FPGA:  XC7A50T-2CSG325I
# RFIC:  AD9364BBCZ (pin-compatible with AD9361/AD9363)
# USB:   USB3320C-EZK-TR (ULPI PHY)
# RAM:   IS66WVH8M8ALL-166B1LI (HyperRAM 8MB)
# Flash: W25Q128JVSIQ (16MB QSPI)
# Clock: S2HO40000F3CHC-T (40MHz TCXO 2.5ppm)
# Power: LTC3370 (Quad Buck) + MIC47100 (1.3V LDO) + LM3880 (Sequencer)
#
# Bank Voltage Summary:
# - Bank 0:  3.3V (Configuration, JTAG)
# - Bank 14: 3.3V (QSPI Flash, M.2 control signals)
# - Bank 15: 2.5V (AD9364 LVDS data + SPI + control)
# - Bank 34: 1.8V (HyperRAM, USB3320 ULPI, SYS_CLK, USB_CLK)

from litex.build.generic_platform import *
from litex.build.xilinx         import Xilinx7SeriesPlatform
from litex.build.openfpgaloader  import OpenFPGALoader

# IOs ----------------------------------------------------------------------------------------------

_io = [
    # System Clock (40 MHz TCXO on Bank 34, MRCC pin).
    ("clk40", 0, Pins("P4"), IOStandard("LVCMOS18")),  # SYS_CLK, IO_L12P_T1_MRCC_34.

    # JTAG (directly accessible via J5 header).
    ("jtag", 0,
        Subsignal("tdi", Pins("T9")),  # JTAG_TDI.
        Subsignal("tdo", Pins("T8")),  # JTAG_TDO.
        Subsignal("tms", Pins("R8")),  # JTAG_TMS.
        Subsignal("tck", Pins("F8")),  # JTAG_TCK.
        IOStandard("LVCMOS33"),
    ),

    # QSPI Flash (W25Q128JVSIQ, Bank 14, 3.3V).
    ("spiflash", 0,
        Subsignal("cs_n", Pins("L15")),    # FPGA_FLASH_CS.
        Subsignal("clk",  Pins("E8")),     # FPGA_FLASH_CLK (CCLK_0).
        Subsignal("mosi", Pins("K16")),    # FPGA_FLASH_DQ0.
        Subsignal("miso", Pins("L17")),    # FPGA_FLASH_DQ1.
        Subsignal("wp",   Pins("J15")),    # FPGA_FLASH_DQ2.
        Subsignal("hold", Pins("J16")),    # FPGA_FLASH_DQ3.
        IOStandard("LVCMOS33"),
    ),

    # LEDs (Bank 14, 3.3V).
    ("user_led", 0, Pins("N17"), IOStandard("LVCMOS33")), # LED_D3.
    ("user_led", 1, Pins("N18"), IOStandard("LVCMOS33")), # LED_D4.

    # PCIe Gen2 x2 (M.2 Connector, GTP transceivers).
    ("pcie_x1", 0,
        Subsignal("rst_n", Pins("T18"), IOStandard("LVCMOS33"), Misc("PULLUP=TRUE")),  # M.2_PERST#.
        Subsignal("clk_p", Pins("D6")),   # PCIE_CLK+ / MGTREFCLK0P_216.
        Subsignal("clk_n", Pins("D5")),   # PCIE_CLK- / MGTREFCLK0N_216.
        Subsignal("rx_p",  Pins("E4")),   # PCIE_RX0+ / MGTPRXP0_216.
        Subsignal("rx_n",  Pins("E3")),   # PCIE_RX0- / MGTPRXN0_216.
        Subsignal("tx_p",  Pins("H2")),   # PCIE_TX0+ / MGTPTXP0_216.
        Subsignal("tx_n",  Pins("H1")),   # PCIE_TX0- / MGTPTXN0_216.
    ),
    ("pcie_x2", 0,
        Subsignal("rst_n", Pins("T18"), IOStandard("LVCMOS33"), Misc("PULLUP=TRUE")),  # M.2_PERST#.
        Subsignal("clk_p", Pins("D6")),   # PCIE_CLK+.
        Subsignal("clk_n", Pins("D5")),   # PCIE_CLK-.
        Subsignal("rx_p",  Pins("E4 A4")),  # PCIE_RX0+ PCIE_RX1+.
        Subsignal("rx_n",  Pins("E3 A3")),  # PCIE_RX0- PCIE_RX1-.
        Subsignal("tx_p",  Pins("H2 F2")),  # PCIE_TX0+ PCIE_TX1+.
        Subsignal("tx_n",  Pins("H1 F1")),  # PCIE_TX0- PCIE_TX1-.
    ),

    # M.2 Control Signals (Bank 14, 3.3V).
    ("m2_clkreq_n", 0, Pins("R18"), IOStandard("LVCMOS33")),  # M.2_CLKREQ#.
    ("m2_perst_n",  0, Pins("T18"), IOStandard("LVCMOS33")),  # M.2_PERST#.
    ("m2_wake_n",   0, Pins("T17"), IOStandard("LVCMOS33")),  # M.2_WAKE#.

    # AD9364 RFIC (Bank 15, 2.5V LVDS + control).
    ("ad9364_rfic", 0,
        # RX Data Interface (LVDS, Bank 15, 2.5V).
        Subsignal("rx_clk_p",   Pins("E16"), IOStandard("LVDS_25"), Misc("DIFF_TERM=TRUE")),  # DATA_CLK_P, IO_L14P_T2_SRCC_15.
        Subsignal("rx_clk_n",   Pins("D16"), IOStandard("LVDS_25"), Misc("DIFF_TERM=TRUE")),  # DATA_CLK_N, IO_L14N_T2_SRCC_15.
        Subsignal("rx_frame_p", Pins("D8"),  IOStandard("LVDS_25"), Misc("DIFF_TERM=TRUE")),  # RX_FRAME+.
        Subsignal("rx_frame_n", Pins("C8"),  IOStandard("LVDS_25"), Misc("DIFF_TERM=TRUE")),  # RX_FRAME-.
        Subsignal("rx_data_p",  Pins("C16 E17 C17 H16 G15 G17"),
            IOStandard("LVDS_25"), Misc("DIFF_TERM=TRUE")),  # RX_D0+ RX_D1+ RX_D2+ RX_D3+ RX_D4+ RX_D5+.
        Subsignal("rx_data_n",  Pins("B17 D18 C18 G16 F15 F18"),
            IOStandard("LVDS_25"), Misc("DIFF_TERM=TRUE")),  # RX_D0- RX_D1- RX_D2- RX_D3- RX_D4- RX_D5-.

        # TX Data Interface (LVDS, Bank 15, 2.5V).
        Subsignal("tx_clk_p",   Pins("D13"), IOStandard("LVDS_25")),  # FB_CLK+.
        Subsignal("tx_clk_n",   Pins("C13"), IOStandard("LVDS_25")),  # FB_CLK-.
        Subsignal("tx_frame_p", Pins("B10"), IOStandard("LVDS_25")),  # TX_FRAME+.
        Subsignal("tx_frame_n", Pins("A10"), IOStandard("LVDS_25")),  # TX_FRAME-.
        Subsignal("tx_data_p",  Pins("B14 D11 B12 C11 B9 D9"),
            IOStandard("LVDS_25")),  # TX_D0+ TX_D1+ TX_D2+ TX_D3+ TX_D4+ TX_D5+.
        Subsignal("tx_data_n",  Pins("A15 C12 A12 B11 A9 C9"),
            IOStandard("LVDS_25")),  # TX_D0- TX_D1- TX_D2- TX_D3- TX_D4- TX_D5-.

        # Control Signals (Single-ended, Bank 15, 2.5V).
        Subsignal("rst_n",      Pins("C14"), IOStandard("LVCMOS25")),  # AD_RESET.
        Subsignal("enable",     Pins("H18"), IOStandard("LVCMOS25")),  # AD_ENABLE.
        Subsignal("txnrx",      Pins("F14"), IOStandard("LVCMOS25")),  # AD_TXRX.
        Subsignal("lock_detect",Pins("D10"), IOStandard("LVCMOS25")),  # CTRL_OUT[0]: BBPLL lock indicator.
        Subsignal("alert",      Pins("H14"), IOStandard("LVCMOS25")),  # CTRL_OUT[1]: fault/overtemp indicator.

        # Clock Output.
        Subsignal("clk_out", Pins("E15"), IOStandard("LVCMOS25")),  # AD_CLK_OUT.
    ),

    # AD9364 SPI (Bank 15, 2.5V — same as VDD_INTERFACE).
    ("ad9364_spi", 0,
        Subsignal("clk",   Pins("F17")),  # SPI_SCLK.
        Subsignal("cs_n",  Pins("E18")),  # SPI_CS.
        Subsignal("mosi",  Pins("D14")),  # SPI_MOSI.
        Subsignal("miso",  Pins("G14")),  # SPI_MISO.
        IOStandard("LVCMOS25"),
    ),

    # USB3320 ULPI PHY (Bank 34, 1.8V).
    ("ulpi", 0,
        Subsignal("clk",   Pins("R3"),  IOStandard("LVCMOS18")),  # USB_CLK (60MHz), IO_L14P_T2_SRCC_34.
        Subsignal("data",  Pins("V3 V2 T4 T3 U4 V4 P6 U6"),
            IOStandard("LVCMOS18")),  # ULPI_D0-D7.
        Subsignal("dir",   Pins("R7"),  IOStandard("LVCMOS18")),  # USB_DIR.
        Subsignal("nxt",   Pins("T7"),  IOStandard("LVCMOS18")),  # USB_NXT.
        Subsignal("stp",   Pins("U7"),  IOStandard("LVCMOS18")),  # USB_STP.
        Subsignal("rst",   Pins("V6"),  IOStandard("LVCMOS18")),  # USB_RESET.
    ),

    # HyperRAM (IS66WVH8M8ALL, Bank 34, 1.8V).
    ("hyperram", 0,
        Subsignal("clk_p",  Pins("M2"), IOStandard("LVCMOS18")),  # RAM_CLK+.
        Subsignal("clk_n",  Pins("M1"), IOStandard("LVCMOS18")),  # RAM_CLK-.
        Subsignal("cs_n",   Pins("M6"), IOStandard("LVCMOS18")),  # RAM_CS.
        Subsignal("rst_n",  Pins("N6"), IOStandard("LVCMOS18")),  # RAM_RESET.
        Subsignal("dq",     Pins("L2 K3 L3 K5 K6 J4 J5 L4"),
            IOStandard("LVCMOS18")),  # RAM_DQ0-DQ7.
        Subsignal("rwds",   Pins("K2"), IOStandard("LVCMOS18")),  # RAM_RWDS.
    ),
]

# Platform -----------------------------------------------------------------------------------------

class Platform(Xilinx7SeriesPlatform):
    default_clk_name   = "clk40"
    default_clk_period = 1e9/40e6

    def __init__(self):
        device = "xc7a50t"
        Xilinx7SeriesPlatform.__init__(self, f"{device}-2csg325", _io, toolchain="vivado")
        self.rfic_clk_freq = 245.76e6

        self.toolchain.bitstream_commands = [
            "set_property BITSTREAM.CONFIG.UNUSEDPIN Pulldown [current_design]",
            "set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]",
            "set_property BITSTREAM.CONFIG.CONFIGRATE 33 [current_design]",
            "set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]",
            "set_property CFGBVS VCCO [current_design]",
            "set_property CONFIG_VOLTAGE 3.3 [current_design]",
        ]

        self.toolchain.additional_commands = [
            "write_cfgmem -force -format bin -interface spix4 -size 16 -loadbit \"up 0x0 "
            "{build_name}.bit\" -file {build_name}.bin",
        ]

    def create_programmer(self):
        return OpenFPGALoader(cable="digilent_hs2", fpga_part="xc7a50tcsg325", freq=10e6)

    def do_finalize(self, fragment):
        Xilinx7SeriesPlatform.do_finalize(self, fragment)
        # 40 MHz TCXO system clock.
        self.add_period_constraint(
            self.lookup_request("clk40", 0, loose=True), 1e9/40e6)
        # AD9364 LVDS RX sample clock.
        self.add_period_constraint(
            self.lookup_request("ad9364_rfic:rx_clk_p", 0, loose=True), 1e9/self.rfic_clk_freq)
        # AsyncFIFO CDC false paths: sys (125 MHz) <-> rfic (245.76 MHz).
        # Gray-coded pointers are safe across clock domains; suppress false timing violations.
        # Must use pre_placement_commands (post-synthesis TCL) so clocks are already defined.
        # sys (125 MHz) <-> rfic (245.76 MHz)
        self.toolchain.pre_placement_commands.add(
            "set_max_delay -datapath_only 4.0 "
            "-from [get_clocks main_crg_clkout0] "
            "-to   [get_clocks ad9364_rfic_rx_clk_p]"
        )
        self.toolchain.pre_placement_commands.add(
            "set_max_delay -datapath_only 8.0 "
            "-from [get_clocks ad9364_rfic_rx_clk_p] "
            "-to   [get_clocks main_crg_clkout0]"
        )
        # sys (125 MHz) <-> usb (60 MHz) — AsyncFIFO gray pointer CDC paths.
        self.toolchain.pre_placement_commands.add(
            "set_max_delay -datapath_only 8.0 "
            "-from [get_clocks main_crg_clkout0] "
            "-to   [get_clocks ulpi0_clk]"
        )
        self.toolchain.pre_placement_commands.add(
            "set_max_delay -datapath_only 8.0 "
            "-from [get_clocks ulpi0_clk] "
            "-to   [get_clocks main_crg_clkout0]"
        )
        # clk40 input → sys: suppress false path from PLL feedback analysis.
        self.toolchain.pre_placement_commands.add(
            "set_false_path "
            "-from [get_clocks main_crg_clkout0] "
            "-to   [get_clocks clk40]"
        )
