# create_project.tcl — OFDM HDL full TX+RX chain
# Target: xc7a50t-2csg325  (Hallycon M.2 SDR, speed grade -2)
# Clock:  100 MHz  Goal: post-implementation resource utilization
#
# Run: vivado -mode batch -source vivado/create_project.tcl
#      from /mnt/d/work/HW/ofdm_tx_hls/

set ROOT     [file normalize [file dirname [file dirname [info script]]]]
set PROJ_DIR "$ROOT/vivado/ofdm_impl"
set IP_REPO  "$ROOT/ip_repo"
set PART     "xc7a50tcsg325-2"

# ── Low-memory settings (target: 4 GB RAM) ─────────────────
# Cap threads across all Vivado engines; OOC runs sequentially (-jobs 1 below)
set_param general.maxThreads 2

# ── Create project ─────────────────────────────────────────
create_project ofdm_hdl $PROJ_DIR -part $PART -force
set_property ip_repo_paths $IP_REPO [current_project]
update_ip_catalog -rebuild

# ── Block design ───────────────────────────────────────────
create_bd_design "ofdm_chain"

# ── Ports ──────────────────────────────────────────────────
create_bd_port -dir I -type clk -freq_hz 100000000 clk
create_bd_port -dir I rst_n

# All blocks use ap_rst_n (active-low) after adding s_axilite pragma

# ── Instantiate all 10 IP instances ───────────────────────
# TX chain
create_bd_cell -type ip -vlnv hallycon.in:ofdm:scrambler:1.0    tx_scrambler
create_bd_cell -type ip -vlnv hallycon.in:ofdm:conv_enc:1.0     tx_conv_enc
create_bd_cell -type ip -vlnv hallycon.in:ofdm:interleaver:1.0  tx_interleaver
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_tx:1.0      ofdm_tx_0

# RX chain
create_bd_cell -type ip -vlnv hallycon.in:ofdm:sync_detect:1.0  sync_detect_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:cfo_correct:1.0  cfo_correct_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_rx:1.0      ofdm_rx_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:interleaver:1.0  rx_interleaver
create_bd_cell -type ip -vlnv hallycon.in:ofdm:viterbi_dec:1.0  viterbi_dec_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:scrambler:1.0    rx_scrambler

# ── Xilinx xfft v9.1 — replaces hls::fft to save ~17K LUT ───────────────
# Minimal config — data width defaults to 16-bit in v9.1, no xn/xk_data_width params.
# ofdm_tx IFFT: 256-pt inverse, pipelined streaming
create_bd_cell -type ip -vlnv xilinx.com:ip:xfft:9.1 ofdm_tx_ifft
set_property -dict [list \
    CONFIG.transform_length                         {256}                    \
    CONFIG.implementation_options                   {pipelined_streaming_io} \
    CONFIG.run_time_configurable_transform_length   {false}                  \
    CONFIG.output_ordering                          {natural_order}          \
] [get_bd_cells ofdm_tx_ifft]

# ofdm_rx FFT: 256-pt forward, pipelined streaming
create_bd_cell -type ip -vlnv xilinx.com:ip:xfft:9.1 ofdm_rx_fft
set_property -dict [list \
    CONFIG.transform_length                         {256}                    \
    CONFIG.implementation_options                   {pipelined_streaming_io} \
    CONFIG.run_time_configurable_transform_length   {false}                  \
    CONFIG.output_ordering                          {natural_order}          \
] [get_bd_cells ofdm_rx_fft]

# Config word format (per PG109): bit[0]=FWD_INV (0=IFFT,1=FFT), bits[8:1]=SCALE_SCH.
# SCALE_SCH=0xAA=10101010b → radix-4 ÷4/stage × 4 stages = ÷256 = ÷N (matches sw model).
# IFFT: 0x0154 (340 decimal)  FFT: 0x0155 (341 decimal)
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 ifft_cfg_val
set_property -dict [list CONFIG.CONST_WIDTH {16} CONFIG.CONST_VAL {340}] \
    [get_bd_cells ifft_cfg_val]

create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 fft_cfg_val
set_property -dict [list CONFIG.CONST_WIDTH {16} CONFIG.CONST_VAL {341}] \
    [get_bd_cells fft_cfg_val]

# Drive s_axis_config_tvalid=1 so xfft latches FWD_INV/SCALE_SCH on first cycle
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 cfg_tvalid
set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {1}] \
    [get_bd_cells cfg_tvalid]

# ── Connect clocks ─────────────────────────────────────────
# HLS blocks use ap_clk
foreach cell {
    tx_scrambler tx_conv_enc tx_interleaver ofdm_tx_0
    sync_detect_0 cfo_correct_0 ofdm_rx_0 rx_interleaver viterbi_dec_0 rx_scrambler
} {
    connect_bd_net [get_bd_ports clk] [get_bd_pins $cell/ap_clk]
}
# xfft uses aclk
connect_bd_net [get_bd_ports clk] [get_bd_pins ofdm_tx_ifft/aclk]
connect_bd_net [get_bd_ports clk] [get_bd_pins ofdm_rx_fft/aclk]

# ── Connect resets — HLS blocks use ap_rst_n ───────────────
foreach cell {
    tx_scrambler tx_conv_enc tx_interleaver ofdm_tx_0
    sync_detect_0 cfo_correct_0 ofdm_rx_0 rx_interleaver viterbi_dec_0 rx_scrambler
} {
    connect_bd_net [get_bd_ports rst_n] [get_bd_pins $cell/ap_rst_n]
}
# xfft v9.1 pipelined_streaming_io has no reset pin — nothing to connect.

# ── TX AXI-Stream chain ────────────────────────────────────
# host_tx_in → tx_scrambler → tx_conv_enc → tx_interleaver → ofdm_tx → rf_tx_out
connect_bd_intf_net \
    [get_bd_intf_pins tx_scrambler/data_out] \
    [get_bd_intf_pins tx_conv_enc/data_in]

connect_bd_intf_net \
    [get_bd_intf_pins tx_conv_enc/coded_out] \
    [get_bd_intf_pins tx_interleaver/data_in]

connect_bd_intf_net \
    [get_bd_intf_pins tx_interleaver/data_out] \
    [get_bd_intf_pins ofdm_tx_0/bits_in]

# ── RX AXI-Stream chain ────────────────────────────────────
# rf_rx_in → adc_input_fifo → sync_detect → cfo_correct → ofdm_rx → rx_interleaver → viterbi_dec → rx_scrambler → rx_output_fifo → host_rx_out

# C4: wire cfo_est directly in hardware — sync_detect ap_vld → cfo_correct ap_none.
# No software round-trip required; value propagates in 1 clock cycle after
# sync_detect asserts ap_done.
connect_bd_net \
    [get_bd_pins sync_detect_0/cfo_est] \
    [get_bd_pins cfo_correct_0/cfo_est]

connect_bd_intf_net \
    [get_bd_intf_pins sync_detect_0/iq_out] \
    [get_bd_intf_pins cfo_correct_0/iq_in]

connect_bd_intf_net \
    [get_bd_intf_pins cfo_correct_0/iq_out] \
    [get_bd_intf_pins ofdm_rx_0/iq_in]

connect_bd_intf_net \
    [get_bd_intf_pins ofdm_rx_0/bits_out] \
    [get_bd_intf_pins rx_interleaver/data_in]

connect_bd_intf_net \
    [get_bd_intf_pins rx_interleaver/data_out] \
    [get_bd_intf_pins viterbi_dec_0/coded_in]

connect_bd_intf_net \
    [get_bd_intf_pins viterbi_dec_0/data_out] \
    [get_bd_intf_pins rx_scrambler/data_in]

# ── ofdm_tx ↔ xfft IFFT ───────────────────────────────────
connect_bd_intf_net \
    [get_bd_intf_pins ofdm_tx_0/ifft_in] \
    [get_bd_intf_pins ofdm_tx_ifft/S_AXIS_DATA]

connect_bd_intf_net \
    [get_bd_intf_pins ofdm_tx_ifft/M_AXIS_DATA] \
    [get_bd_intf_pins ofdm_tx_0/ifft_out]

connect_bd_net \
    [get_bd_pins ifft_cfg_val/dout] \
    [get_bd_pins ofdm_tx_ifft/s_axis_config_tdata]
connect_bd_net \
    [get_bd_pins cfg_tvalid/dout] \
    [get_bd_pins ofdm_tx_ifft/s_axis_config_tvalid]

# ── ofdm_rx ↔ xfft FFT ────────────────────────────────────
connect_bd_intf_net \
    [get_bd_intf_pins ofdm_rx_0/fft_in] \
    [get_bd_intf_pins ofdm_rx_fft/S_AXIS_DATA]

connect_bd_intf_net \
    [get_bd_intf_pins ofdm_rx_fft/M_AXIS_DATA] \
    [get_bd_intf_pins ofdm_rx_0/fft_out]

connect_bd_net \
    [get_bd_pins fft_cfg_val/dout] \
    [get_bd_pins ofdm_rx_fft/s_axis_config_tdata]
connect_bd_net \
    [get_bd_pins cfg_tvalid/dout] \
    [get_bd_pins ofdm_rx_fft/s_axis_config_tvalid]

# ── FIFOs ──────────────────────────────────────────────────
# adc_input_fifo — depth 4096 → 4 BRAM_36, covers 205 µs at 20 MSPS
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 adc_input_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {4096}] [get_bd_cells adc_input_fifo]
connect_bd_net [get_bd_ports clk]   [get_bd_pins adc_input_fifo/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins adc_input_fifo/s_axis_aresetn]
connect_bd_intf_net [get_bd_intf_pins adc_input_fifo/M_AXIS] [get_bd_intf_pins sync_detect_0/iq_in]

# rx_output_fifo — depth 32768 → 8 BRAM_36, holds one full decoded frame
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 rx_output_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {32768}] [get_bd_cells rx_output_fifo]
connect_bd_net [get_bd_ports clk]   [get_bd_pins rx_output_fifo/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins rx_output_fifo/s_axis_aresetn]
connect_bd_intf_net [get_bd_intf_pins rx_scrambler/data_out] [get_bd_intf_pins rx_output_fifo/S_AXIS]

# ── External ports ─────────────────────────────────────────
make_bd_intf_pins_external [get_bd_intf_pins tx_scrambler/data_in]  -name host_tx_in
make_bd_intf_pins_external [get_bd_intf_pins ofdm_tx_0/iq_out]      -name rf_tx_out
make_bd_intf_pins_external [get_bd_intf_pins adc_input_fifo/S_AXIS] -name rf_rx_in
make_bd_intf_pins_external [get_bd_intf_pins rx_output_fifo/M_AXIS] -name host_rx_out

# ── Validate and wrap ──────────────────────────────────────
validate_bd_design
save_bd_design

set bd_file [get_files ofdm_chain.bd]

# OOC per-IP: each block compiles to a DCP, synth_design stitches the wrapper.
# Prevents global constant-propagation from trimming the design to nothing.
generate_target all $bd_file
export_ip_user_files -of_objects $bd_file -no_script -sync -force -quiet

make_wrapper -files $bd_file -top
set wrapper [glob "$PROJ_DIR/ofdm_hdl.gen/sources_1/bd/ofdm_chain/hdl/ofdm_chain_wrapper.v"]
add_files -norecurse $wrapper
set_property top ofdm_chain_wrapper [current_fileset]

# ── XDC (add to synth_1 fileset) ───────────────────────────
add_files -fileset constrs_1 -norecurse "$ROOT/vivado/ofdm_top.xdc"

# ── Synthesis via launch_runs ───────────────────────────────
# launch_runs synth_1 handles the full OOC dependency chain:
#   BD sub-IPs (axis_data_fifo, xfft, xlconstant) → OOC DCPs → top synth.
# Direct synth_design would skip the OOC step → "module not found".
puts "\n========== Running Vivado Synthesis =========="
set_property STEPS.SYNTH_DESIGN.ARGS.FLATTEN_HIERARCHY none [get_runs synth_1]
launch_runs synth_1 -jobs 1
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    puts "ERROR: Synthesis failed — check vivado_run.log"
    exit 1
}
open_run synth_1 -name synth_1

# ── Post-synthesis utilization ──────────────────────────────
report_utilization -file "$ROOT/vivado/utilization_post_synth.rpt" -hierarchical
report_utilization -file "$ROOT/vivado/utilization_post_synth_summary.rpt"

# ── Implementation via launch_runs ─────────────────────────
puts "\n========== Running Implementation =========="
launch_runs impl_1 -to_step route_design -jobs 1
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    puts "ERROR: Implementation failed — check vivado_run.log"
    exit 1
}
open_run impl_1 -name impl_1

# ── Reports ────────────────────────────────────────────────
report_utilization -file "$ROOT/vivado/utilization_post_impl.rpt" -hierarchical
report_utilization -file "$ROOT/vivado/utilization_post_impl_summary.rpt"
report_timing_summary  -file "$ROOT/vivado/timing_post_impl.rpt" -max_paths 10

puts "\n============================================"
puts " DONE"
puts "   vivado/utilization_post_impl.rpt"
puts "   vivado/timing_post_impl.rpt"
puts "============================================"
