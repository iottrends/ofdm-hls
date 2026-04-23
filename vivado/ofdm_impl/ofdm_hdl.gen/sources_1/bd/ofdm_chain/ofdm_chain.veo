// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2026 Advanced Micro Devices, Inc. All Rights Reserved.
// -------------------------------------------------------------------------------
// This file contains confidential and proprietary information
// of AMD and is protected under U.S. and international copyright
// and other intellectual property laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// AMD, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND AMD HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) AMD shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or AMD had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// AMD products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of AMD products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.
//
// DO NOT MODIFY THIS FILE.

// MODULE VLNV: amd.com:blockdesign:ofdm_chain:1.0

// The following must be inserted into your Verilog file for this
// module to be instantiated. Change the instance name and port connections
// (in parentheses) to your own signal names.

// INST_TAG     ------ Begin cut for INSTANTIATION Template ------
ofdm_chain your_instance_name (
  .clk(clk), // input wire clk
  .clk_fec(clk_fec), // input wire clk_fec
  .rst_n(rst_n), // input wire rst_n
  .rst_fec_n(rst_fec_n), // input wire rst_fec_n
  .host_tx_in_tdata(host_tx_in_tdata), // input wire [7:0] host_tx_in_tdata
  .host_tx_in_tkeep(host_tx_in_tkeep), // input wire [0:0] host_tx_in_tkeep
  .host_tx_in_tlast(host_tx_in_tlast), // input wire [0:0] host_tx_in_tlast
  .host_tx_in_tready(host_tx_in_tready), // output wire host_tx_in_tready
  .host_tx_in_tstrb(host_tx_in_tstrb), // input wire [0:0] host_tx_in_tstrb
  .host_tx_in_tvalid(host_tx_in_tvalid), // input wire host_tx_in_tvalid
  .rf_tx_out_tdata(rf_tx_out_tdata), // output wire [47:0] rf_tx_out_tdata
  .rf_tx_out_tready(rf_tx_out_tready), // input wire rf_tx_out_tready
  .rf_tx_out_tvalid(rf_tx_out_tvalid), // output wire rf_tx_out_tvalid
  .rf_rx_in_tdata(rf_rx_in_tdata), // input wire [39:0] rf_rx_in_tdata
  .rf_rx_in_tready(rf_rx_in_tready), // output wire rf_rx_in_tready
  .rf_rx_in_tvalid(rf_rx_in_tvalid), // input wire rf_rx_in_tvalid
  .host_rx_out_tdata(host_rx_out_tdata), // output wire [7:0] host_rx_out_tdata
  .host_rx_out_tready(host_rx_out_tready), // input wire host_rx_out_tready
  .host_rx_out_tvalid(host_rx_out_tvalid), // output wire host_rx_out_tvalid
  .ctrl_axi_awaddr(ctrl_axi_awaddr), // input wire [15:0] ctrl_axi_awaddr
  .ctrl_axi_awprot(ctrl_axi_awprot), // input wire [2:0] ctrl_axi_awprot
  .ctrl_axi_awvalid(ctrl_axi_awvalid), // input wire ctrl_axi_awvalid
  .ctrl_axi_awready(ctrl_axi_awready), // output wire ctrl_axi_awready
  .ctrl_axi_wdata(ctrl_axi_wdata), // input wire [31:0] ctrl_axi_wdata
  .ctrl_axi_wstrb(ctrl_axi_wstrb), // input wire [3:0] ctrl_axi_wstrb
  .ctrl_axi_wvalid(ctrl_axi_wvalid), // input wire ctrl_axi_wvalid
  .ctrl_axi_wready(ctrl_axi_wready), // output wire ctrl_axi_wready
  .ctrl_axi_bresp(ctrl_axi_bresp), // output wire [1:0] ctrl_axi_bresp
  .ctrl_axi_bvalid(ctrl_axi_bvalid), // output wire ctrl_axi_bvalid
  .ctrl_axi_bready(ctrl_axi_bready), // input wire ctrl_axi_bready
  .ctrl_axi_araddr(ctrl_axi_araddr), // input wire [15:0] ctrl_axi_araddr
  .ctrl_axi_arprot(ctrl_axi_arprot), // input wire [2:0] ctrl_axi_arprot
  .ctrl_axi_arvalid(ctrl_axi_arvalid), // input wire ctrl_axi_arvalid
  .ctrl_axi_arready(ctrl_axi_arready), // output wire ctrl_axi_arready
  .ctrl_axi_rdata(ctrl_axi_rdata), // output wire [31:0] ctrl_axi_rdata
  .ctrl_axi_rresp(ctrl_axi_rresp), // output wire [1:0] ctrl_axi_rresp
  .ctrl_axi_rvalid(ctrl_axi_rvalid), // output wire ctrl_axi_rvalid
  .ctrl_axi_rready(ctrl_axi_rready), // input wire ctrl_axi_rready
  .mac_tx_done_pulse(mac_tx_done_pulse), // output wire [0:0] mac_tx_done_pulse
  .mac_rx_pkt_pulse(mac_rx_pkt_pulse), // output wire [0:0] mac_rx_pkt_pulse
  .host_rx_out_tkeep(host_rx_out_tkeep), // output wire [0:0] host_rx_out_tkeep
  .host_rx_out_tlast(host_rx_out_tlast), // output wire host_rx_out_tlast
  .host_rx_out_tstrb(host_rx_out_tstrb) // output wire [0:0] host_rx_out_tstrb
);
// INST_TAG_END ------  End cut for INSTANTIATION Template  ------

// You must compile the wrapper file ofdm_chain.v when simulating
// the module, ofdm_chain. When compiling the wrapper file, be sure to
// reference the Verilog simulation library.
