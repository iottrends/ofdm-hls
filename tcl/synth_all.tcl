open_project -reset ofdm_tx_proj
set_top ofdm_tx
add_files src/ofdm_tx.cpp
add_files src/ofdm_tx.h
add_files -tb tb/ofdm_tx_tb.cpp -cflags "-I./src"
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
# C simulation
csim_design
# C synthesis
csynth_design
# Export report
puts "=== Synthesis Report ==="
puts [read [open ofdm_tx_proj/sol1/syn/report/ofdm_tx_csynth.rpt]]
close_project
exit
