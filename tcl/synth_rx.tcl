open_project -reset ofdm_rx_proj
set_top ofdm_rx
add_files src/ofdm_rx.cpp
add_files src/ofdm_rx.h
add_files src/ofdm_tx.h
open_solution sol1
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
csynth_design
puts "\n=== RX Synthesis Report ==="
set rpt [open ofdm_rx_proj/sol1/syn/report/ofdm_rx_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
