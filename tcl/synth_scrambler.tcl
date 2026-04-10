open_project -reset scrambler_proj
set_top scrambler
add_files src/scrambler.cpp
add_files src/scrambler.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
csynth_design
puts "\n=== scrambler Synthesis Report ==="
set rpt [open scrambler_proj/sol1/syn/report/scrambler_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
