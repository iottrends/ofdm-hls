open_project -reset interleaver_proj
set_top interleaver
add_files src/interleaver.cpp
add_files src/interleaver.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
csynth_design
puts "\n=== interleaver Synthesis Report ==="
set rpt [open interleaver_proj/sol1/syn/report/interleaver_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
