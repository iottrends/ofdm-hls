open_project ofdm_tx_proj
set_top ofdm_tx
add_files ofdm_tx.cpp
add_files ofdm_tx.h
add_files -tb ofdm_tx_tb.cpp
open_solution sol1
set_part xc7a50tcsg324-1
create_clock -period 10
csim_design
close_project
exit
