open_project -reset cfo_correct_proj
set_top cfo_correct
add_files src/cfo_correct.cpp
add_files src/cfo_correct.h
add_files src/sync_detect.h
add_files src/ofdm_rx.h
add_files src/ofdm_tx.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
csynth_design
puts "\n=== cfo_correct Synthesis Report ==="
set rpt [open cfo_correct_proj/sol1/syn/report/cfo_correct_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
