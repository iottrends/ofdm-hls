# ============================================================
# synth_ofdm_mac.tcl — combined MAC HLS IP
#   top: ofdm_mac
#   clock: 10 ns (100 MHz, ofdm sys clock)
#
# Contains:
#   - datapath MAC (Ethernet header + CRC-32 FCS + address filter)
#   - PHY sequencer (AXI4 master via m_axi, replaces LiteX OFDMLowerMAC)
# ============================================================
open_project -reset ofdm_mac_proj
set_top ofdm_mac

add_files src/ofdm_mac.cpp
add_files src/ofdm_mac.h
add_files src/ofdm_tx.h

open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0

csynth_design

puts "\n=== ofdm_mac Synthesis Report ==="
set rpt [open ofdm_mac_proj/sol1/syn/report/ofdm_mac_csynth.rpt r]
puts [read $rpt]
close $rpt

close_project
exit
