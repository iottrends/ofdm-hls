open_project -reset conv_enc_proj
set_top conv_enc
add_files src/conv_enc.cpp
add_files src/conv_fec.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
csynth_design
puts "\n=== conv_enc Synthesis Report ==="
set rpt [open conv_enc_proj/sol1/syn/report/conv_enc_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
