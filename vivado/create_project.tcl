# create_project.tcl — OFDM full TX+RX+MAC chain (post-merge, with combined MAC)
# Target: xc7a50t-2csg325
# Clocks: 100 MHz sys, 200 MHz fec_rx
#
# Topology: see vivado/create_ofdm_bd.tcl for authoritative BD description.
#
# Run: vivado -mode batch -source vivado/create_project.tcl

set ROOT     [file normalize [file dirname [file dirname [info script]]]]
set PROJ_DIR "$ROOT/vivado/ofdm_impl"
set IP_REPO  "$ROOT/ip_repo"
set PART     "xc7a50tcsg325-2"

set_param general.maxThreads 2

create_project ofdm_hdl $PROJ_DIR -part $PART -force
set_property ip_repo_paths $IP_REPO [current_project]
update_ip_catalog -rebuild

# ── Block design ──────────────────────────────────────────
create_bd_design "ofdm_chain"

create_bd_port -dir I -type clk -freq_hz 100000000 clk
create_bd_port -dir I -type clk -freq_hz 200000000 clk_fec
create_bd_port -dir I rst_n
create_bd_port -dir I rst_fec_n

# HLS IP cells
create_bd_cell -type ip -vlnv hallycon.in:ofdm:tx_chain:1.0  tx_chain_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_tx:1.0   ofdm_tx_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:sync_detect:1.0  sync_detect_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_rx:1.0   ofdm_rx_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:fec_rx:1.0    fec_rx_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_mac:1.0  ofdm_mac_0

# xfft IPs
create_bd_cell -type ip -vlnv xilinx.com:ip:xfft:9.1 ofdm_tx_ifft
set_property -dict [list \
    CONFIG.transform_length                       {256}                    \
    CONFIG.implementation_options                 {pipelined_streaming_io} \
    CONFIG.run_time_configurable_transform_length {false}                  \
    CONFIG.output_ordering                        {natural_order}          \
] [get_bd_cells ofdm_tx_ifft]

create_bd_cell -type ip -vlnv xilinx.com:ip:xfft:9.1 ofdm_rx_fft
set_property -dict [list \
    CONFIG.transform_length                       {256}                    \
    CONFIG.implementation_options                 {pipelined_streaming_io} \
    CONFIG.run_time_configurable_transform_length {false}                  \
    CONFIG.output_ordering                        {natural_order}          \
] [get_bd_cells ofdm_rx_fft]

create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 ifft_cfg_val
set_property -dict [list CONFIG.CONST_WIDTH {16} CONFIG.CONST_VAL {340}] [get_bd_cells ifft_cfg_val]
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 fft_cfg_val
set_property -dict [list CONFIG.CONST_WIDTH {16} CONFIG.CONST_VAL {341}] [get_bd_cells fft_cfg_val]
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 cfg_tvalid
set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {1}] [get_bd_cells cfg_tvalid]

# AXIS CCs
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_clock_converter:1.1 fec_cc1
set_property -dict [list CONFIG.TDATA_NUM_BYTES {1}] [get_bd_cells fec_cc1]
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_clock_converter:1.1 fec_cc2
set_property -dict [list CONFIG.TDATA_NUM_BYTES {1}] [get_bd_cells fec_cc2]

# Clocks / resets
foreach cell {tx_chain_0 ofdm_tx_0 sync_detect_0 ofdm_rx_0 ofdm_mac_0} {
    connect_bd_net [get_bd_ports clk]   [get_bd_pins $cell/ap_clk]
    connect_bd_net [get_bd_ports rst_n] [get_bd_pins $cell/ap_rst_n]
}
connect_bd_net [get_bd_ports clk] [get_bd_pins ofdm_tx_ifft/aclk]
connect_bd_net [get_bd_ports clk] [get_bd_pins ofdm_rx_fft/aclk]

connect_bd_net [get_bd_ports clk_fec]   [get_bd_pins fec_rx_0/ap_clk]
connect_bd_net [get_bd_ports rst_fec_n] [get_bd_pins fec_rx_0/ap_rst_n]

connect_bd_net [get_bd_ports clk]       [get_bd_pins fec_cc1/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n]     [get_bd_pins fec_cc1/s_axis_aresetn]
connect_bd_net [get_bd_ports clk_fec]   [get_bd_pins fec_cc1/m_axis_aclk]
connect_bd_net [get_bd_ports rst_fec_n] [get_bd_pins fec_cc1/m_axis_aresetn]
connect_bd_net [get_bd_ports clk_fec]   [get_bd_pins fec_cc2/s_axis_aclk]
connect_bd_net [get_bd_ports rst_fec_n] [get_bd_pins fec_cc2/s_axis_aresetn]
connect_bd_net [get_bd_ports clk]       [get_bd_pins fec_cc2/m_axis_aclk]
connect_bd_net [get_bd_ports rst_n]     [get_bd_pins fec_cc2/m_axis_aresetn]

# Datapath
connect_bd_intf_net [get_bd_intf_pins ofdm_mac_0/phy_tx_out] [get_bd_intf_pins tx_chain_0/data_in]
connect_bd_intf_net [get_bd_intf_pins tx_chain_0/data_out]   [get_bd_intf_pins ofdm_tx_0/bits_in]
connect_bd_intf_net [get_bd_intf_pins sync_detect_0/iq_out]     [get_bd_intf_pins ofdm_rx_0/iq_in]
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_0/bits_out]    [get_bd_intf_pins fec_cc1/S_AXIS]
connect_bd_intf_net [get_bd_intf_pins fec_cc1/M_AXIS]        [get_bd_intf_pins fec_rx_0/data_in]
connect_bd_intf_net [get_bd_intf_pins fec_rx_0/data_out]     [get_bd_intf_pins fec_cc2/S_AXIS]
connect_bd_intf_net [get_bd_intf_pins fec_cc2/M_AXIS]        [get_bd_intf_pins ofdm_mac_0/phy_rx_in]

# ofdm_rx header-decode wires fan out to fec_rx + ofdm_mac
connect_bd_net [get_bd_pins ofdm_rx_0/modcod_out] [get_bd_pins fec_rx_0/modcod]
connect_bd_net [get_bd_pins ofdm_rx_0/modcod_out] [get_bd_pins ofdm_mac_0/rx_modcod_in]
connect_bd_net [get_bd_pins ofdm_rx_0/n_syms_out] [get_bd_pins fec_rx_0/n_syms]
connect_bd_net [get_bd_pins ofdm_rx_0/n_syms_out] [get_bd_pins ofdm_mac_0/rx_n_syms_in]
connect_bd_net [get_bd_pins ofdm_rx_0/header_err] [get_bd_pins ofdm_mac_0/rx_header_err]

# n_syms feedback from ofdm_rx → sync_cfo (gate close signal)
connect_bd_net [get_bd_pins ofdm_rx_0/n_syms_fb] [get_bd_pins sync_detect_0/n_syms_fb]

# ofdm_mac ap_start tied high — sync_detect/ofdm_rx/fec_rx are ap_ctrl_none
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 mac_ap_start_hi
set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {1}] [get_bd_cells mac_ap_start_hi]
connect_bd_net [get_bd_pins mac_ap_start_hi/dout] [get_bd_pins ofdm_mac_0/ap_start]

# xfft
connect_bd_intf_net [get_bd_intf_pins ofdm_tx_0/ifft_in]        [get_bd_intf_pins ofdm_tx_ifft/S_AXIS_DATA]
connect_bd_intf_net [get_bd_intf_pins ofdm_tx_ifft/M_AXIS_DATA] [get_bd_intf_pins ofdm_tx_0/ifft_out]
connect_bd_net [get_bd_pins ifft_cfg_val/dout] [get_bd_pins ofdm_tx_ifft/s_axis_config_tdata]
connect_bd_net [get_bd_pins cfg_tvalid/dout]   [get_bd_pins ofdm_tx_ifft/s_axis_config_tvalid]
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_0/fft_in]         [get_bd_intf_pins ofdm_rx_fft/S_AXIS_DATA]
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_fft/M_AXIS_DATA]  [get_bd_intf_pins ofdm_rx_0/fft_out]
connect_bd_net [get_bd_pins fft_cfg_val/dout] [get_bd_pins ofdm_rx_fft/s_axis_config_tdata]
connect_bd_net [get_bd_pins cfg_tvalid/dout]  [get_bd_pins ofdm_rx_fft/s_axis_config_tvalid]

# FIFOs
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 adc_input_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {4096} CONFIG.TDATA_NUM_BYTES {5}] [get_bd_cells adc_input_fifo]
connect_bd_net [get_bd_ports clk]   [get_bd_pins adc_input_fifo/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins adc_input_fifo/s_axis_aresetn]
connect_bd_intf_net [get_bd_intf_pins adc_input_fifo/M_AXIS] [get_bd_intf_pins sync_detect_0/iq_in]

create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 rx_output_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {32768}] [get_bd_cells rx_output_fifo]
connect_bd_net [get_bd_ports clk]   [get_bd_pins rx_output_fifo/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins rx_output_fifo/s_axis_aresetn]
connect_bd_intf_net [get_bd_intf_pins ofdm_mac_0/host_rx_out] [get_bd_intf_pins rx_output_fifo/S_AXIS]

# Smartconnect 2:6
set csr_cells {tx_chain_0 ofdm_tx_0 sync_detect_0 ofdm_rx_0 fec_rx_0 ofdm_mac_0}
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 ctrl_xbar
set_property -dict [list CONFIG.NUM_SI {2} CONFIG.NUM_MI {6} CONFIG.NUM_CLKS {2}] [get_bd_cells ctrl_xbar]
connect_bd_net [get_bd_ports clk]     [get_bd_pins ctrl_xbar/aclk]
connect_bd_net [get_bd_ports clk_fec] [get_bd_pins ctrl_xbar/aclk1]
connect_bd_net [get_bd_ports rst_n]   [get_bd_pins ctrl_xbar/aresetn]

connect_bd_intf_net [get_bd_intf_pins ctrl_xbar/S01_AXI] \
                    [get_bd_intf_pins ofdm_mac_0/m_axi_csr_master]

set i 0
foreach cell $csr_cells {
    set mi [format "M%02d_AXI" $i]
    connect_bd_intf_net [get_bd_intf_pins ctrl_xbar/$mi] [get_bd_intf_pins $cell/s_axi_ctrl]
    incr i
}

# External ports
make_bd_intf_pins_external [get_bd_intf_pins ofdm_mac_0/host_tx_in] -name host_tx_in
make_bd_intf_pins_external [get_bd_intf_pins ofdm_tx_0/iq_out]      -name rf_tx_out
make_bd_intf_pins_external [get_bd_intf_pins adc_input_fifo/S_AXIS] -name rf_rx_in
make_bd_intf_pins_external [get_bd_intf_pins rx_output_fifo/M_AXIS] -name host_rx_out
make_bd_intf_pins_external [get_bd_intf_pins ctrl_xbar/S00_AXI]     -name ctrl_axi
set_property CONFIG.PROTOCOL   AXI4LITE [get_bd_intf_ports ctrl_axi]
set_property CONFIG.DATA_WIDTH 32       [get_bd_intf_ports ctrl_axi]
set_property CONFIG.ADDR_WIDTH 16       [get_bd_intf_ports ctrl_axi]

make_bd_pins_external [get_bd_pins ofdm_mac_0/tx_done_pulse] -name mac_tx_done_pulse
make_bd_pins_external [get_bd_pins ofdm_mac_0/rx_pkt_pulse]  -name mac_rx_pkt_pulse

# Address maps
set off 0
foreach cell $csr_cells {
    assign_bd_address \
        -target_address_space [get_bd_addr_spaces /ctrl_axi] \
        -offset [expr {$off}] -range 4K \
        [get_bd_addr_segs "$cell/s_axi_ctrl/Reg"]
    set off [expr {$off + 0x1000}]
}
# MAC m_axi master reaches only TX blocks (tx_chain, ofdm_tx).
# RX blocks are free-running — no MAC access needed.
assign_bd_address \
    -target_address_space [get_bd_addr_spaces ofdm_mac_0/Data_m_axi_csr_master] \
    -offset 0x0000 -range 4K \
    [get_bd_addr_segs "tx_chain_0/s_axi_ctrl/Reg"]
assign_bd_address \
    -target_address_space [get_bd_addr_spaces ofdm_mac_0/Data_m_axi_csr_master] \
    -offset 0x1000 -range 4K \
    [get_bd_addr_segs "ofdm_tx_0/s_axi_ctrl/Reg"]

validate_bd_design
save_bd_design

set bd_file [get_files ofdm_chain.bd]
generate_target all $bd_file
export_ip_user_files -of_objects $bd_file -no_script -sync -force -quiet

make_wrapper -files $bd_file -top
set wrapper [glob "$PROJ_DIR/ofdm_hdl.gen/sources_1/bd/ofdm_chain/hdl/ofdm_chain_wrapper.v"]
add_files -norecurse $wrapper
set_property top ofdm_chain_wrapper [current_fileset]

add_files -fileset constrs_1 -norecurse "$ROOT/vivado/ofdm_top.xdc"

puts "\n========== Running Vivado Synthesis =========="
set_property STEPS.SYNTH_DESIGN.ARGS.FLATTEN_HIERARCHY none [get_runs synth_1]
launch_runs synth_1 -jobs 1
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    puts "ERROR: Synthesis failed"; exit 1
}
open_run synth_1 -name synth_1
report_utilization -file "$ROOT/vivado/utilization_post_synth.rpt" -hierarchical
report_utilization -file "$ROOT/vivado/utilization_post_synth_summary.rpt"

puts "\n========== Running Implementation =========="
launch_runs impl_1 -to_step route_design -jobs 1
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    puts "ERROR: Implementation failed"; exit 1
}
open_run impl_1 -name impl_1
report_utilization -file "$ROOT/vivado/utilization_post_impl.rpt" -hierarchical
report_utilization -file "$ROOT/vivado/utilization_post_impl_summary.rpt"
report_timing_summary  -file "$ROOT/vivado/timing_post_impl.rpt" -max_paths 10

puts "\n============================================"
puts " DONE"
puts "============================================"
