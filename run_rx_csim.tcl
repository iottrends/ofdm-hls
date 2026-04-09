# run_rx_csim.tcl  —  Vitis HLS project for OFDM RX
#
# Usage (from Vitis HLS Tcl console, or vitis_hls -f run_rx_csim.tcl):
#   source run_rx_csim.tcl
#
# Steps performed:
#   1. Create project ofdm_rx_proj / sol1
#   2. Add source + testbench
#   3. Run C simulation  → tb_rx_decoded_hls.bin
#   4. Run C synthesis   → RTL

set prj_dir  [file normalize [file dirname [info script]]]
set prj_name "ofdm_rx_proj"
set sol_name "sol1"
set part     "xc7a50tcsg324-1"
set clk_ns   10

# ── Create / open project ─────────────────────────────────────
open_project -reset ${prj_name}
set_top ofdm_rx

# ── Sources ───────────────────────────────────────────────────
add_files ${prj_dir}/ofdm_rx.cpp
add_files ${prj_dir}/ofdm_tx.h        ;# shared types

# ── Testbench ─────────────────────────────────────────────────
add_files -tb ${prj_dir}/ofdm_rx_tb.cpp

# ── Solution ──────────────────────────────────────────────────
open_solution -reset ${sol_name} -flow_target vivado
set_part ${part}
create_clock -period ${clk_ns} -name default

# ── C Simulation ──────────────────────────────────────────────
# Testbench reads tb_tx_output_hls.txt and tb_input_to_tx.bin from CWD.
# Run from the project directory so relative paths resolve.
puts "\n\[TCL\] Running C simulation..."
set build_dir "${prj_name}/${sol_name}/csim/build"

csim_design -setup
file copy -force ${prj_dir}/tb_tx_output_hls.txt  ${build_dir}/tb_tx_output_hls.txt
file copy -force ${prj_dir}/tb_input_to_tx.bin   ${build_dir}/tb_input_to_tx.bin

set ret [catch {exec sh -c "cd ${build_dir} && ./csim.exe 2>&1"} out]
puts ${out}

# Copy results back
foreach fname {tb_rx_decoded_hls.bin} {
    set src "${build_dir}/${fname}"
    if {[file exists ${src}]} {
        file copy -force ${src} ${prj_dir}/${fname}
        puts "\[TCL\] Copied ${fname} to project directory"
    }
}

if {${ret} != 0} {
    puts "\[TCL\] C-sim FAILED (return code ${ret})"
} else {
    puts "\[TCL\] C-sim PASSED"
}

# ── C Synthesis ───────────────────────────────────────────────
puts "\n\[TCL\] Running C synthesis..."
csynth_design

puts "\n\[TCL\] Done.  RTL at ${prj_name}/${sol_name}/syn/"
