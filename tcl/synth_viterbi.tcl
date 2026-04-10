open_project -reset viterbi_proj
set_top viterbi_dec
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
csynth_design
puts "\n=== viterbi_dec Synthesis Report ==="
set rpt [open viterbi_proj/sol1/syn/report/viterbi_dec_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
