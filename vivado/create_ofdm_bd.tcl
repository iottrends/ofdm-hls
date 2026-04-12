# ============================================================
# create_ofdm_bd.tcl — Build the ofdm_chain BD and export its
#                     Verilog wrapper + a file list for LiteX.
#
# This is the Path-B entry point: it creates the same BD as
# create_project.tcl (via `source`) but stops after make_wrapper
# and writes ip_repo/ofdm_chain.f listing every Verilog source
# that shell.py (LiteX) needs to register with its platform.
#
# Run:
#   vivado -mode batch -source vivado/create_ofdm_bd.tcl
# ============================================================

set ROOT     [file normalize [file dirname [file dirname [info script]]]]
set PROJ_DIR "$ROOT/vivado/ofdm_bd"
set IP_REPO  "$ROOT/ip_repo"
set PART     "xc7a50tcsg325-2"

set_param general.maxThreads 2

# ── Create a fresh project (reuses existing HLS IP repo) ──
create_project ofdm_bd $PROJ_DIR -part $PART -force
set_property ip_repo_paths $IP_REPO [current_project]
update_ip_catalog -rebuild

# ── Build the block design ────────────────────────────────
# Re-use the exact BD creation logic from create_project.tcl by
# sourcing its body up to `validate_bd_design`. The cleanest way
# here is to evaluate the file but skip its synth/impl tail, so
# we inline the BD-construction commands directly below.
#
# Keep this block in sync with create_project.tcl § "Block
# design" through § "External ports".

create_bd_design "ofdm_chain"

create_bd_port -dir I -type clk -freq_hz 100000000 clk
create_bd_port -dir I rst_n

# --- TX chain ---
create_bd_cell -type ip -vlnv hallycon.in:ofdm:scrambler:1.0    tx_scrambler
create_bd_cell -type ip -vlnv hallycon.in:ofdm:conv_enc:1.0     tx_conv_enc
create_bd_cell -type ip -vlnv hallycon.in:ofdm:interleaver:1.0  tx_interleaver
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_tx:1.0      ofdm_tx_0

# --- RX chain ---
create_bd_cell -type ip -vlnv hallycon.in:ofdm:sync_detect:1.0  sync_detect_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:cfo_correct:1.0  cfo_correct_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:ofdm_rx:1.0      ofdm_rx_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:interleaver:1.0  rx_interleaver
create_bd_cell -type ip -vlnv hallycon.in:ofdm:viterbi_dec:1.0  viterbi_dec_0
create_bd_cell -type ip -vlnv hallycon.in:ofdm:scrambler:1.0    rx_scrambler

# --- xfft IP (256-pt) ---
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
set_property -dict [list CONFIG.CONST_WIDTH {16} CONFIG.CONST_VAL {340}] \
    [get_bd_cells ifft_cfg_val]
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 fft_cfg_val
set_property -dict [list CONFIG.CONST_WIDTH {16} CONFIG.CONST_VAL {341}] \
    [get_bd_cells fft_cfg_val]
create_bd_cell -type ip -vlnv xilinx.com:ip:xlconstant:1.1 cfg_tvalid
set_property -dict [list CONFIG.CONST_WIDTH {1} CONFIG.CONST_VAL {1}] \
    [get_bd_cells cfg_tvalid]

# --- Clocks / resets ---
foreach cell {
    tx_scrambler tx_conv_enc tx_interleaver ofdm_tx_0
    sync_detect_0 cfo_correct_0 ofdm_rx_0 rx_interleaver viterbi_dec_0 rx_scrambler
} {
    connect_bd_net [get_bd_ports clk]   [get_bd_pins $cell/ap_clk]
    connect_bd_net [get_bd_ports rst_n] [get_bd_pins $cell/ap_rst_n]
}
connect_bd_net [get_bd_ports clk] [get_bd_pins ofdm_tx_ifft/aclk]
connect_bd_net [get_bd_ports clk] [get_bd_pins ofdm_rx_fft/aclk]

# --- TX chain nets ---
connect_bd_intf_net [get_bd_intf_pins tx_scrambler/data_out]   [get_bd_intf_pins tx_conv_enc/data_in]
connect_bd_intf_net [get_bd_intf_pins tx_conv_enc/coded_out]   [get_bd_intf_pins tx_interleaver/data_in]
connect_bd_intf_net [get_bd_intf_pins tx_interleaver/data_out] [get_bd_intf_pins ofdm_tx_0/bits_in]

# --- CFO direct wire + RX chain nets ---
connect_bd_net      [get_bd_pins      sync_detect_0/cfo_est]   [get_bd_pins      cfo_correct_0/cfo_est]
connect_bd_intf_net [get_bd_intf_pins sync_detect_0/iq_out]    [get_bd_intf_pins cfo_correct_0/iq_in]
connect_bd_intf_net [get_bd_intf_pins cfo_correct_0/iq_out]    [get_bd_intf_pins ofdm_rx_0/iq_in]
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_0/bits_out]      [get_bd_intf_pins rx_interleaver/data_in]
connect_bd_intf_net [get_bd_intf_pins rx_interleaver/data_out] [get_bd_intf_pins viterbi_dec_0/coded_in]
connect_bd_intf_net [get_bd_intf_pins viterbi_dec_0/data_out]  [get_bd_intf_pins rx_scrambler/data_in]

# --- ofdm_tx ↔ xfft IFFT ---
connect_bd_intf_net [get_bd_intf_pins ofdm_tx_0/ifft_in]       [get_bd_intf_pins ofdm_tx_ifft/S_AXIS_DATA]
connect_bd_intf_net [get_bd_intf_pins ofdm_tx_ifft/M_AXIS_DATA] [get_bd_intf_pins ofdm_tx_0/ifft_out]
connect_bd_net [get_bd_pins ifft_cfg_val/dout] [get_bd_pins ofdm_tx_ifft/s_axis_config_tdata]
connect_bd_net [get_bd_pins cfg_tvalid/dout]   [get_bd_pins ofdm_tx_ifft/s_axis_config_tvalid]

# --- ofdm_rx ↔ xfft FFT ---
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_0/fft_in]        [get_bd_intf_pins ofdm_rx_fft/S_AXIS_DATA]
connect_bd_intf_net [get_bd_intf_pins ofdm_rx_fft/M_AXIS_DATA]  [get_bd_intf_pins ofdm_rx_0/fft_out]
connect_bd_net [get_bd_pins fft_cfg_val/dout] [get_bd_pins ofdm_rx_fft/s_axis_config_tdata]
connect_bd_net [get_bd_pins cfg_tvalid/dout]  [get_bd_pins ofdm_rx_fft/s_axis_config_tvalid]

# --- FIFOs ---
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 adc_input_fifo
# TDATA_NUM_BYTES = 6 (48-bit) matches sync_detect/iq_in (HLS iq_t = {i:16,q:16,last:1} → 48b).
# Without this the externalized S_AXIS breaks downstream width inference and defaults to 8b.
set_property -dict [list CONFIG.FIFO_DEPTH {4096} CONFIG.TDATA_NUM_BYTES {6}] [get_bd_cells adc_input_fifo]
connect_bd_net [get_bd_ports clk]   [get_bd_pins adc_input_fifo/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins adc_input_fifo/s_axis_aresetn]
connect_bd_intf_net [get_bd_intf_pins adc_input_fifo/M_AXIS] [get_bd_intf_pins sync_detect_0/iq_in]

create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 rx_output_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {32768}] [get_bd_cells rx_output_fifo]
connect_bd_net [get_bd_ports clk]   [get_bd_pins rx_output_fifo/s_axis_aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins rx_output_fifo/s_axis_aresetn]
connect_bd_intf_net [get_bd_intf_pins rx_scrambler/data_out] [get_bd_intf_pins rx_output_fifo/S_AXIS]

# --- AXI-Lite CSR crossbar (1:10 smartconnect) ---
# Aggregates all 10 HLS s_axi_ctrl slaves behind a single ctrl_axi slave
# exposed at the wrapper boundary.  LiteX drives this from the OFDMLowerMAC
# FSM via an AXILiteInterface master (see shell.py).
#
# Address map — each HLS block gets a 4 KB window:
#   0x0000_0000  tx_scrambler
#   0x0000_1000  tx_conv_enc
#   0x0000_2000  tx_interleaver
#   0x0000_3000  ofdm_tx_0
#   0x0000_4000  sync_detect_0
#   0x0000_5000  cfo_correct_0
#   0x0000_6000  ofdm_rx_0
#   0x0000_7000  rx_interleaver
#   0x0000_8000  viterbi_dec_0
#   0x0000_9000  rx_scrambler
# Total CSR region: 40 KB → ctrl_axi needs 16 addr bits.  LiteX maps this
# as a SoCRegion so driver sees a flat PCIe BAR range.
#
# Keep this ordering in sync with OFDMLowerMAC.CSR_BASE in shell.py.

set csr_cells {
    tx_scrambler
    tx_conv_enc
    tx_interleaver
    ofdm_tx_0
    sync_detect_0
    cfo_correct_0
    ofdm_rx_0
    rx_interleaver
    viterbi_dec_0
    rx_scrambler
}

create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 ctrl_xbar
set_property -dict [list \
    CONFIG.NUM_SI   {1}  \
    CONFIG.NUM_MI   {10} \
    CONFIG.NUM_CLKS {1}  \
] [get_bd_cells ctrl_xbar]

connect_bd_net [get_bd_ports clk]   [get_bd_pins ctrl_xbar/aclk]
connect_bd_net [get_bd_ports rst_n] [get_bd_pins ctrl_xbar/aresetn]

# Wire each smartconnect master port to the matching HLS block's s_axi_ctrl.
set i 0
foreach cell $csr_cells {
    set mi [format "M%02d_AXI" $i]
    connect_bd_intf_net [get_bd_intf_pins ctrl_xbar/$mi] \
                        [get_bd_intf_pins $cell/s_axi_ctrl]
    incr i
}

# --- External interface pins ---
make_bd_intf_pins_external [get_bd_intf_pins tx_scrambler/data_in]  -name host_tx_in
make_bd_intf_pins_external [get_bd_intf_pins ofdm_tx_0/iq_out]      -name rf_tx_out
make_bd_intf_pins_external [get_bd_intf_pins adc_input_fifo/S_AXIS] -name rf_rx_in
make_bd_intf_pins_external [get_bd_intf_pins rx_output_fifo/M_AXIS] -name host_rx_out
make_bd_intf_pins_external [get_bd_intf_pins ctrl_xbar/S00_AXI]     -name ctrl_axi
# Force the external port to AXI4-Lite so the LiteX Instance() port map
# only needs AWADDR/AWVALID/AWREADY/WDATA/WSTRB/WVALID/WREADY/BRESP/BVALID/
# BREADY/ARADDR/ARVALID/ARREADY/RDATA/RRESP/RVALID/RREADY (~17 signals)
# instead of the 40+ signals of full AXI4.
set_property CONFIG.PROTOCOL  AXI4LITE [get_bd_intf_ports ctrl_axi]
set_property CONFIG.DATA_WIDTH 32      [get_bd_intf_ports ctrl_axi]
set_property CONFIG.ADDR_WIDTH 16      [get_bd_intf_ports ctrl_axi]

# --- AXI-Lite address map (deterministic, must match shell.py constants) ---
# Externalized slaves expose their address space at /<port>.  Vivado names
# the pseudo-master address space after the port, so we target /ctrl_axi.
set off 0
foreach cell $csr_cells {
    assign_bd_address \
        -target_address_space [get_bd_addr_spaces /ctrl_axi] \
        -offset [expr {$off}] \
        -range  4K \
        [get_bd_addr_segs "$cell/s_axi_ctrl/Reg"]
    set off [expr {$off + 0x1000}]
}

# ── Validate, save, generate sources, make wrapper ────────
validate_bd_design
save_bd_design

set bd_file [get_files ofdm_chain.bd]

generate_target all $bd_file
export_ip_user_files -of_objects $bd_file -no_script -sync -force -quiet

make_wrapper -files $bd_file -top
set wrapper_v [glob "$PROJ_DIR/ofdm_bd.gen/sources_1/bd/ofdm_chain/hdl/ofdm_chain_wrapper.v"]
add_files -norecurse $wrapper_v

# ── Emit a Verilog file list for LiteX (ip_repo/ofdm_chain.f) ──
# The list includes the BD wrapper and every sub-IP's synthesised
# Verilog so that LiteX's Vivado flow finds them via platform.add_source.
set fl_path "$IP_REPO/ofdm_chain.f"
set fl [open $fl_path w]
puts $fl "# Auto-generated by create_ofdm_bd.tcl — do not edit."
puts $fl "# Source list for LiteX shell.py to register with platform.add_source."
puts $fl $wrapper_v

# Collect every .v/.sv the BD generated for its sub-IPs.
set gen_dir "$PROJ_DIR/ofdm_bd.gen/sources_1/bd/ofdm_chain"
foreach ext {.v .sv .vh .svh} {
    foreach f [glob -nocomplain -directory $gen_dir -types f -- "*/*$ext"] {
        puts $fl $f
    }
    foreach f [glob -nocomplain -directory $gen_dir -types f -- "*/*/*$ext"] {
        puts $fl $f
    }
    foreach f [glob -nocomplain -directory $gen_dir -types f -- "*/*/*/*$ext"] {
        puts $fl $f
    }
}
close $fl

puts "\n============================================"
puts " ofdm_chain BD exported"
puts "   wrapper : $wrapper_v"
puts "   filelist: $fl_path"
puts "   Next    : cd litex && python3 shell.py --build"
puts "============================================"
