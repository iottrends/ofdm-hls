# ============================================================
# synth_tx_chain.tcl — merged TX bit-chain HLS IP
#   top: tx_chain  =  scrambler → conv_enc → interleaver  (DATAFLOW)
#   clock: 10 ns (100 MHz system clock domain)
# ============================================================
open_project -reset tx_chain_proj
set_top tx_chain

add_files src/tx_chain.cpp
add_files src/tx_chain.h
# Sub-functions compiled into the same IP as DATAFLOW processes:
add_files src/scrambler.cpp
add_files src/scrambler.h
add_files src/conv_enc.cpp
add_files src/conv_fec.h
add_files src/interleaver.cpp
add_files src/interleaver.h
# tx_chain.h includes ofdm_tx.h for the mod_t typedef
add_files src/ofdm_tx.h

open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0

csynth_design

puts "\n=== tx_chain Synthesis Report ==="
set rpt [open tx_chain_proj/sol1/syn/report/tx_chain_csynth.rpt r]
puts [read $rpt]
close $rpt

close_project
exit
