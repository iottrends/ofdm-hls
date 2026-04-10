# ofdm_top.xdc — Hallycon M2 SDR (XC7A50T-2CSG325)
# Pin assignments from hallycon_m2sdr_platform.py.
# AXI-Lite and AXI-Stream data buses connect to LiteX internally —
# no physical pins needed for those.

# ── Clock: 40 MHz TCXO, Bank 34, LVCMOS18 ─────────────────────
# Full design uses 100 MHz from system MMCM; standalone XDC defines
# port at 100 MHz for resource measurement (timing violations ok).
set_property PACKAGE_PIN P4        [get_ports clk]
set_property IOSTANDARD  LVCMOS18  [get_ports clk]
create_clock -period 10.000 -name clk [get_ports clk]

# ── Reset: PCIe PERST#, Bank 14, LVCMOS33, active-low ─────────
set_property PACKAGE_PIN T18       [get_ports rst_n]
set_property IOSTANDARD  LVCMOS33  [get_ports rst_n]

# ── Keep all logic: false-path on unplaced ports ───────────────
# Prevents constant-propagation from trimming the design when
# AXI-Lite/AXI-Stream ports have no physical pin assignments.
set_false_path -from [all_inputs]
set_false_path -to   [all_outputs]
