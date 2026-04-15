# ============================================================
# synth_sync_detect.tcl — free-running preamble gate + inline CFO
#   top: sync_detect (absorbs old sync_cfo + cfo_correct)
#   clock: 10 ns (100 MHz, ofdm sys clock)
# ============================================================
open_project -reset sync_detect_proj
set_top sync_detect
add_files src/sync_detect.cpp
add_files src/sync_detect.h
add_files src/free_run.h
add_files src/ofdm_rx.h
add_files src/ofdm_tx.h
# NOTE: cfo_correct.cpp is NOT added — its functionality is now inline
# inside sync_detect.cpp.
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
config_compile -pipeline_loops 0
csynth_design
puts "\n=== sync_detect Synthesis Report ==="
set rpt [open sync_detect_proj/sol1/syn/report/sync_detect_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
