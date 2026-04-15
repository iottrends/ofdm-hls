# ============================================================
# create_ofdm_bd.tcl — ofdm_chain BD (PHY + combined MAC)
#
# Topology (6 HLS IPs + xfft x2 + 2× AXIS CC):
#
#   TX datapath:
#     host_tx_in (8b AXIS, 100MHz) → ofdm_mac → tx_chain → ofdm_tx → xfft → rf_tx_out
#
#   RX datapath:
#     rf_rx_in → adc_fifo → sync_cfo → ofdm_rx → xfft
#              → fec_cc1 (100→200MHz) → fec_rx
#              → fec_cc2 (200→100MHz) → ofdm_mac → rx_output_fifo → host_rx_out
#
#   Control:
#     ctrl_xbar: 2 SI × 6 MI smartconnect
#       S00  = external ctrl_axi (host)
#       S01  = ofdm_mac.m_axi_csr_master (MAC → PHY CSRs)
#       M00..M04 = tx_chain, ofdm_tx, sync_cfo, ofdm_rx, fec_rx
#       M05  = ofdm_mac.s_axi_ctrl (host reaches MAC's own CSR)
#
# Clock domains:
#   clk     (100 MHz): tx_chain, ofdm_tx, sync_cfo, ofdm_rx, ofdm_mac,
#                      xfft pair, adc_fifo, fec_cc2 master, smartconnect
#   clk_fec (200 MHz): fec_rx, fec_cc1 master, fec_cc2 slave
# ============================================================

set ROOT     [file normalize [file dirname [file dirname [info script]]]]
set PROJ_DIR "$ROOT/vivado/ofdm_bd"
set IP_REPO  "$ROOT/ip_repo"
set PART     "xc7a50tcsg325-2"

set_param general.maxThreads 2

create_project ofdm_bd $PROJ_DIR -part $PART -force
set_property ip_repo_paths $IP_REPO [current_project]
update_ip_catalog -rebuild

create_bd_design "ofdm_chain"

# ── Clock / reset ports ───────────────────────────────────
create_bd_port -dir I -type clk -freq_hz 100000000 clk
create_bd_port -dir I -type clk -freq_hz 200000000 clk_fec
create_bd_port -dir I rst_n
create_bd_port -dir I rst_fec_n

# ── HLS IP cells ──────────────────────────────────────────
create_bd_cell -type ip -vlnv hallycon.in:ofdm:tx_chain:1.0  tx_chain_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_tx:1.0   ofdm_tx_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:sync_detect:1.0  sync_detect_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_rx:1.0   ofdm_rx_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:fec_rx:1.0    fec_rx_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_mac:1.0  ofdm_mac_0

# ── xfft IPs (256-pt, pipelined streaming, natural order) ─
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

# ── AXIS clock converters ─────────────────────────────────
# fec_cc1: 100 MHz (ofdm_rx.bits_out) → 200 MHz (fec_rx.data_in)
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_clock_converter:1.1 fec_cc1
set_property -dict [list CONFIG.TDATA_NUM_BYTES {1}] [get_bd_cells fec_cc1]
# fec_cc2: 200 MHz (fec_rx.data_out) → 100 MHz (ofdm_mac.phy_rx_in)
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_clock_converter:1.1 fec_cc2
set_property -dict [list CONFIG.TDATA_NUM_BYTES {1}] [get_bd_cells fec_cc2]

# ── Clocks ────────────────────────────────────────────────
# 100 MHz domain
foreach cell {tx_chain_0 ofdm_tx_0 sync_detect_0 ofdm_rx_0 ofdm_mac_0} {
    connect_bd_net [get_bd_ports clk]   [get_bd_pins $cell/ap_clk]
    connect_bd_net [get_bd_ports rst_n] [get_bd_pins $cell/ap_rst_n]
}
connect_bd_net [get_bd_ports clk] [get_bd_pins ofdm_tx_ifft/aclk]
connect_bd_net [get_bd_ports clk] [get_bd_pins ofdm_rx_fft/aclk]

# 200 MHz domain
connect_bd_net [get_bd_ports clk_fec]   [get_bd_pins fec_rx_0/ap_clk]
connect_bd_net [get_bd_ports rst_fec_n] [get_bd_pins fec_rx_0/ap_rst_n]

# AXIS CCs straddle the two domains
connect_bd_net [get_bd_ports clk]       [get_bd_pins fec_cc1/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n]     [get_bd_pins fec_cc1/s_axis_aresetn]
connect_bd_net [get_bd_ports clk_fec]   [get_bd_pins fec_cc1/m_axis_aclk]
connect_bd_net [get_bd_ports rst_fec_n] [get_bd_pins fec_cc1/m_axis_aresetn]

connect_bd_net [get_bd_ports clk_fec]   [get_bd_pins fec_cc2/s_axis_aclk]
connect_bd_net [get_bd_ports rst_fec_n] [get_bd_pins fec_cc2/s_axis_aresetn]
connect_bd_net [get_bd_ports clk]       [get_bd_pins fec_cc2/m_axis_aclk]
connect_bd_net [get_bd_ports rst_n]     [get_bd_pins fec_cc2/m_axis_aresetn]

# ── TX datapath ───────────────────────────────────────────
connect_bd_intf_net [get_bd_intf_pins ofdm_mac_0/phy_tx_out] \
                    [get_bd_intf_pins tx_chain_0/data_in]
connect_bd_intf_net [get_bd_intf_pins tx_chain_0/data_out] \
                    [get_bd_intf_pins ofdm_tx_0/bits_in]

# ── RX datapath ───────────────────────────────────────────
connect_bd_intf_net [get_bd_intf_pins sync_detect_0/iq_out] \
                    [get_bd_intf_pins ofdm_rx_0/iq_in]
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_0/bits_out] \
                    [get_bd_intf_pins fec_cc1/S_AXIS]
connect_bd_intf_net [get_bd_intf_pins fec_cc1/M_AXIS] \
                    [get_bd_intf_pins fec_rx_0/data_in]
connect_bd_intf_net [get_bd_intf_pins fec_rx_0/data_out] \
                    [get_bd_intf_pins fec_cc2/S_AXIS]
connect_bd_intf_net [get_bd_intf_pins fec_cc2/M_AXIS] \
                    [get_bd_intf_pins ofdm_mac_0/phy_rx_in]

# ── ofdm_rx header-decode wires → fec_rx + ofdm_mac ───────
# ofdm_rx decodes modcod/n_syms from the BPSK air header and exposes them
# as ap_vld outputs.  They fan out directly (same net) to:
#   - fec_rx (as ap_none inputs) for per-packet self-configuration, and
#   - ofdm_mac (as ap_none inputs) for stats/FSM visibility.
connect_bd_net [get_bd_pins ofdm_rx_0/modcod_out] [get_bd_pins fec_rx_0/modcod]
connect_bd_net [get_bd_pins ofdm_rx_0/modcod_out] [get_bd_pins ofdm_mac_0/rx_modcod_in]
connect_bd_net [get_bd_pins ofdm_rx_0/n_syms_out] [get_bd_pins fec_rx_0/n_syms]
connect_bd_net [get_bd_pins ofdm_rx_0/n_syms_out] [get_bd_pins ofdm_mac_0/rx_n_syms_in]
connect_bd_net [get_bd_pins ofdm_rx_0/header_err] [get_bd_pins ofdm_mac_0/rx_header_err]

# Feedback from ofdm_rx → sync_cfo: n_syms_fb tells the gate how many data
# symbols to forward after the preamble+header.  Bridges the RX chain's
# header-decode output back to the gatekeeper at the front.
connect_bd_net [get_bd_pins ofdm_rx_0/n_syms_fb] [get_bd_pins sync_detect_0/n_syms_fb]

# ── ofdm_mac ap_start tied high (self-retrigger per packet) ────
# sync_detect, ofdm_rx, fec_rx are ap_ctrl_none — no ap_start pin to wire.
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 mac_ap_start_hi
set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {1}] [get_bd_cells mac_ap_start_hi]
connect_bd_net [get_bd_pins mac_ap_start_hi/dout] [get_bd_pins ofdm_mac_0/ap_start]

# ── ofdm_tx ↔ xfft IFFT ───────────────────────────────────
connect_bd_intf_net [get_bd_intf_pins ofdm_tx_0/ifft_in]        [get_bd_intf_pins ofdm_tx_ifft/S_AXIS_DATA]
connect_bd_intf_net [get_bd_intf_pins ofdm_tx_ifft/M_AXIS_DATA] [get_bd_intf_pins ofdm_tx_0/ifft_out]
connect_bd_net [get_bd_pins ifft_cfg_val/dout] [get_bd_pins ofdm_tx_ifft/s_axis_config_tdata]
connect_bd_net [get_bd_pins cfg_tvalid/dout]   [get_bd_pins ofdm_tx_ifft/s_axis_config_tvalid]

# ── ofdm_rx ↔ xfft FFT ────────────────────────────────────
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_0/fft_in]         [get_bd_intf_pins ofdm_rx_fft/S_AXIS_DATA]
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_fft/M_AXIS_DATA]  [get_bd_intf_pins ofdm_rx_0/fft_out]
connect_bd_net [get_bd_pins fft_cfg_val/dout] [get_bd_pins ofdm_rx_fft/s_axis_config_tdata]
connect_bd_net [get_bd_pins cfg_tvalid/dout]  [get_bd_pins ofdm_rx_fft/s_axis_config_tvalid]

# ── FIFOs ─────────────────────────────────────────────────
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 adc_input_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {4096} CONFIG.TDATA_NUM_BYTES {5}] [get_bd_cells adc_input_fifo]
connect_bd_net [get_bd_ports clk]   [get_bd_pins adc_input_fifo/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins adc_input_fifo/s_axis_aresetn]
connect_bd_intf_net [get_bd_intf_pins adc_input_fifo/M_AXIS] [get_bd_intf_pins sync_detect_0/iq_in]

# rx_output_fifo now on 100 MHz (MAC side)
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 rx_output_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {32768}] [get_bd_cells rx_output_fifo]
connect_bd_net [get_bd_ports clk]   [get_bd_pins rx_output_fifo/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins rx_output_fifo/s_axis_aresetn]
connect_bd_intf_net [get_bd_intf_pins ofdm_mac_0/host_rx_out] [get_bd_intf_pins rx_output_fifo/S_AXIS]

# ── AXI smartconnect: 2 SI × 6 MI ─────────────────────────
# S00 = host (external), S01 = ofdm_mac m_axi CSR master
# M00..M04 = PHY blocks; M05 = ofdm_mac own s_axi_ctrl
set csr_cells {
    tx_chain_0
    ofdm_tx_0
    sync_detect_0
    ofdm_rx_0
    fec_rx_0
    ofdm_mac_0
}

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 ctrl_xbar
set_property -dict [list \
    CONFIG.NUM_SI   {2}  \
    CONFIG.NUM_MI   {6}  \
    CONFIG.NUM_CLKS {2}  \
] [get_bd_cells ctrl_xbar]

connect_bd_net [get_bd_ports clk]     [get_bd_pins ctrl_xbar/aclk]
connect_bd_net [get_bd_ports clk_fec] [get_bd_pins ctrl_xbar/aclk1]
connect_bd_net [get_bd_ports rst_n]   [get_bd_pins ctrl_xbar/aresetn]

# Attach MAC's m_axi master to S01
connect_bd_intf_net [get_bd_intf_pins ctrl_xbar/S01_AXI] \
                    [get_bd_intf_pins ofdm_mac_0/m_axi_csr_master]

# M00..M05 to PHY + MAC s_axilite slaves
set i 0
foreach cell $csr_cells {
    set mi [format "M%02d_AXI" $i]
    connect_bd_intf_net [get_bd_intf_pins ctrl_xbar/$mi] \
                        [get_bd_intf_pins $cell/s_axi_ctrl]
    incr i
}

# ── External ports ────────────────────────────────────────
make_bd_intf_pins_external [get_bd_intf_pins ofdm_mac_0/host_tx_in] -name host_tx_in
make_bd_intf_pins_external [get_bd_intf_pins ofdm_tx_0/iq_out]      -name rf_tx_out
make_bd_intf_pins_external [get_bd_intf_pins adc_input_fifo/S_AXIS] -name rf_rx_in
make_bd_intf_pins_external [get_bd_intf_pins rx_output_fifo/M_AXIS] -name host_rx_out
make_bd_intf_pins_external [get_bd_intf_pins ctrl_xbar/S00_AXI]     -name ctrl_axi
set_property CONFIG.PROTOCOL   AXI4LITE [get_bd_intf_ports ctrl_axi]
set_property CONFIG.DATA_WIDTH 32       [get_bd_intf_ports ctrl_axi]
set_property CONFIG.ADDR_WIDTH 16       [get_bd_intf_ports ctrl_axi]

# Expose MAC interrupt pulses at the wrapper boundary
make_bd_pins_external [get_bd_pins ofdm_mac_0/tx_done_pulse] -name mac_tx_done_pulse
make_bd_pins_external [get_bd_pins ofdm_mac_0/rx_pkt_pulse]  -name mac_rx_pkt_pulse

# ── AXI-Lite address map (host S00 sees all 6 slaves) ─────
# Keep this ordering in sync with shell.py MAC CSR constants.
set off 0
foreach cell $csr_cells {
    assign_bd_address \
        -target_address_space [get_bd_addr_spaces /ctrl_axi] \
        -offset [expr {$off}] \
        -range  4K \
        [get_bd_addr_segs "$cell/s_axi_ctrl/Reg"]
    set off [expr {$off + 0x1000}]
}

# MAC m_axi master reaches ONLY the TX chain blocks (tx_chain, ofdm_tx).
# RX blocks are free-running — MAC does not touch their CSRs.  Host still
# has full 6-slot access via S00 for stats/diagnostics.
assign_bd_address \
    -target_address_space [get_bd_addr_spaces ofdm_mac_0/Data_m_axi_csr_master] \
    -offset 0x0000 -range 4K \
    [get_bd_addr_segs "tx_chain_0/s_axi_ctrl/Reg"]
assign_bd_address \
    -target_address_space [get_bd_addr_spaces ofdm_mac_0/Data_m_axi_csr_master] \
    -offset 0x1000 -range 4K \
    [get_bd_addr_segs "ofdm_tx_0/s_axi_ctrl/Reg"]

# ── Validate, save, wrap ──────────────────────────────────
validate_bd_design
save_bd_design

set bd_file [get_files ofdm_chain.bd]
generate_target all $bd_file
export_ip_user_files -of_objects $bd_file -no_script -sync -force -quiet

make_wrapper -files $bd_file -top
set wrapper_v [glob "$PROJ_DIR/ofdm_bd.gen/sources_1/bd/ofdm_chain/hdl/ofdm_chain_wrapper.v"]
add_files -norecurse $wrapper_v

# ── File list for LiteX ───────────────────────────────────
set fl_path "$IP_REPO/ofdm_chain.f"
set fl [open $fl_path w]
puts $fl "# Auto-generated by create_ofdm_bd.tcl — do not edit."
puts $fl $wrapper_v
set gen_dir "$PROJ_DIR/ofdm_bd.gen/sources_1/bd/ofdm_chain"
foreach ext {.v .sv .vh .svh} {
    foreach f [glob -nocomplain -directory $gen_dir -types f -- "*/*$ext"]     { puts $fl $f }
    foreach f [glob -nocomplain -directory $gen_dir -types f -- "*/*/*$ext"]   { puts $fl $f }
    foreach f [glob -nocomplain -directory $gen_dir -types f -- "*/*/*/*$ext"] { puts $fl $f }
}
close $fl

puts "\n============================================"
puts " ofdm_chain BD exported (6 HLS IPs + xfft x2 + 2× AXIS CC)"
puts "   wrapper : $wrapper_v"
puts "   filelist: $fl_path"
puts "============================================"
