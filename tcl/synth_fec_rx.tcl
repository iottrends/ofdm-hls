# ============================================================
# synth_fec_rx.tcl — merged RX FEC HLS IP
#   top: fec_rx  =  interleaver(is_rx=1) → viterbi_dec → scrambler  (DATAFLOW)
#   clock: 5 ns (200 MHz — separate clock domain from ofdm_rx).
#
# Rationale: viterbi_dec v3 (unroll=64 ACS, 1 cycle/trellis stage) at
# 200 MHz keeps up with 16-QAM rate-2/3 real-time decoding.  Upstream
# ofdm_rx runs in the 100 MHz ADC sample-clock domain; the byte stream
# crosses into this block via an AXI-Stream Clock Converter in the BD.
# ============================================================
open_project -reset fec_rx_proj
set_top fec_rx

add_files src/fec_rx.cpp
add_files src/fec_rx.h
add_files src/interleaver.cpp
add_files src/interleaver.h
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
add_files src/scrambler.cpp
add_files src/scrambler.h
add_files src/ofdm_tx.h

open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 5
config_compile -pipeline_loops 0

csynth_design

puts "\n=== fec_rx Synthesis Report ==="
set rpt [open fec_rx_proj/sol1/syn/report/fec_rx_csynth.rpt r]
puts [read $rpt]
close $rpt

close_project
exit
