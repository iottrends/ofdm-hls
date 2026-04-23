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

`timescale 1ps / 1ps

`include "vivado_interfaces.svh"

module ofdm_chain_sv (
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in" *)
  (* X_INTERFACE_MODE = "slave host_tx_in" *)
  (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME host_tx_in, TDATA_NUM_BYTES 1, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0, HAS_TREADY 1, HAS_TSTRB 1, HAS_TKEEP 1, HAS_TLAST 1, FREQ_HZ 100000000, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, LAYERED_METADATA undef, INSERT_VIP 0" *)
  vivado_axis_v1_0.slave host_tx_in,
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_tx_out" *)
  (* X_INTERFACE_MODE = "master rf_tx_out" *)
  (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rf_tx_out, TDATA_NUM_BYTES 6, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0, HAS_TREADY 1, HAS_TSTRB 0, HAS_TKEEP 0, HAS_TLAST 0, FREQ_HZ 100000000, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, LAYERED_METADATA undef, INSERT_VIP 0" *)
  vivado_axis_v1_0.master rf_tx_out,
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_rx_in" *)
  (* X_INTERFACE_MODE = "slave rf_rx_in" *)
  (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rf_rx_in, TDATA_NUM_BYTES 5, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0, HAS_TREADY 1, HAS_TSTRB 0, HAS_TKEEP 0, HAS_TLAST 0, FREQ_HZ 100000000, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, LAYERED_METADATA undef, INSERT_VIP 0" *)
  vivado_axis_v1_0.slave rf_rx_in,
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out" *)
  (* X_INTERFACE_MODE = "master host_rx_out" *)
  (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME host_rx_out, TDATA_NUM_BYTES 1, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0, HAS_TREADY 1, HAS_TSTRB 1, HAS_TKEEP 1, HAS_TLAST 1, FREQ_HZ 100000000, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, LAYERED_METADATA undef, INSERT_VIP 0" *)
  vivado_axis_v1_0.master host_rx_out,
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi" *)
  (* X_INTERFACE_MODE = "slave ctrl_axi" *)
  (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME ctrl_axi, DATA_WIDTH 32, PROTOCOL AXI4LITE, FREQ_HZ 100000000, ID_WIDTH 0, ADDR_WIDTH 16, AWUSER_WIDTH 0, ARUSER_WIDTH 0, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, READ_WRITE_MODE READ_WRITE, HAS_BURST 1, HAS_LOCK 1, HAS_PROT 1, HAS_CACHE 1, HAS_QOS 1, HAS_REGION 1, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 0, NUM_READ_OUTSTANDING 1, NUM_WRITE_OUTSTANDING 1, MAX_BURST_LENGTH 1, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, NUM_READ_THREADS 1, NUM_WRITE_THREADS 1, RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *)
  vivado_axi4_lite_v1_0.slave ctrl_axi,
  (* X_INTERFACE_IGNORE = "true" *)
  input wire clk,
  (* X_INTERFACE_IGNORE = "true" *)
  input wire clk_fec,
  (* X_INTERFACE_IGNORE = "true" *)
  input wire rst_n,
  (* X_INTERFACE_IGNORE = "true" *)
  input wire rst_fec_n,
  (* X_INTERFACE_IGNORE = "true" *)
  output wire [0:0] mac_tx_done_pulse,
  (* X_INTERFACE_IGNORE = "true" *)
  output wire [0:0] mac_rx_pkt_pulse
);

  // interface wire assignments
  assign rf_tx_out.TDEST = 0;
  assign rf_tx_out.TID = 0;
  assign rf_tx_out.TKEEP = 0;
  assign rf_tx_out.TLAST = 0;
  assign rf_tx_out.TSTRB = 0;
  assign rf_tx_out.TUSER = 0;
  assign host_rx_out.TDEST = 0;
  assign host_rx_out.TID = 0;
  assign host_rx_out.TUSER = 0;

  ofdm_chain inst (
    .clk(clk),
    .clk_fec(clk_fec),
    .rst_n(rst_n),
    .rst_fec_n(rst_fec_n),
    .host_tx_in_tdata(host_tx_in.TDATA),
    .host_tx_in_tkeep(host_tx_in.TKEEP),
    .host_tx_in_tlast(host_tx_in.TLAST),
    .host_tx_in_tready(host_tx_in.TREADY),
    .host_tx_in_tstrb(host_tx_in.TSTRB),
    .host_tx_in_tvalid(host_tx_in.TVALID),
    .rf_tx_out_tdata(rf_tx_out.TDATA),
    .rf_tx_out_tready(rf_tx_out.TREADY),
    .rf_tx_out_tvalid(rf_tx_out.TVALID),
    .rf_rx_in_tdata(rf_rx_in.TDATA),
    .rf_rx_in_tready(rf_rx_in.TREADY),
    .rf_rx_in_tvalid(rf_rx_in.TVALID),
    .host_rx_out_tdata(host_rx_out.TDATA),
    .host_rx_out_tready(host_rx_out.TREADY),
    .host_rx_out_tvalid(host_rx_out.TVALID),
    .ctrl_axi_awaddr(ctrl_axi.AWADDR),
    .ctrl_axi_awprot(ctrl_axi.AWPROT),
    .ctrl_axi_awvalid(ctrl_axi.AWVALID),
    .ctrl_axi_awready(ctrl_axi.AWREADY),
    .ctrl_axi_wdata(ctrl_axi.WDATA),
    .ctrl_axi_wstrb(ctrl_axi.WSTRB),
    .ctrl_axi_wvalid(ctrl_axi.WVALID),
    .ctrl_axi_wready(ctrl_axi.WREADY),
    .ctrl_axi_bresp(ctrl_axi.BRESP),
    .ctrl_axi_bvalid(ctrl_axi.BVALID),
    .ctrl_axi_bready(ctrl_axi.BREADY),
    .ctrl_axi_araddr(ctrl_axi.ARADDR),
    .ctrl_axi_arprot(ctrl_axi.ARPROT),
    .ctrl_axi_arvalid(ctrl_axi.ARVALID),
    .ctrl_axi_arready(ctrl_axi.ARREADY),
    .ctrl_axi_rdata(ctrl_axi.RDATA),
    .ctrl_axi_rresp(ctrl_axi.RRESP),
    .ctrl_axi_rvalid(ctrl_axi.RVALID),
    .ctrl_axi_rready(ctrl_axi.RREADY),
    .mac_tx_done_pulse(mac_tx_done_pulse),
    .mac_rx_pkt_pulse(mac_rx_pkt_pulse),
    .host_rx_out_tkeep(host_rx_out.TKEEP),
    .host_rx_out_tlast(host_rx_out.TLAST),
    .host_rx_out_tstrb(host_rx_out.TSTRB)
  );

endmodule
