open_project -reset ofdm_tx_proj
set_top ofdm_tx
add_files src/ofdm_tx.cpp
add_files src/ofdm_tx.h
open_solution sol1
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
csynth_design
close_project
exit
