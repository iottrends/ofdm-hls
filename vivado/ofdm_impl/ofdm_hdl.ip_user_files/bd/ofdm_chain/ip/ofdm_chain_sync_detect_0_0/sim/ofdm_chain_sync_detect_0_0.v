// (c) Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// (c) Copyright 2022-2026 Advanced Micro Devices, Inc. All rights reserved.
// 
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


// IP VLNV: hallycon.in:ofdm:sync_detect:1.0
// IP Revision: 2114567948

`timescale 1ns/1ps

(* IP_DEFINITION_SOURCE = "HLS" *)
(* DowngradeIPIdentifiedWarnings = "yes" *)
module ofdm_chain_sync_detect_0_0 (
  s_axi_stat_ARADDR,
  s_axi_stat_ARREADY,
  s_axi_stat_ARVALID,
  s_axi_stat_AWADDR,
  s_axi_stat_AWREADY,
  s_axi_stat_AWVALID,
  s_axi_stat_BREADY,
  s_axi_stat_BRESP,
  s_axi_stat_BVALID,
  s_axi_stat_RDATA,
  s_axi_stat_RREADY,
  s_axi_stat_RRESP,
  s_axi_stat_RVALID,
  s_axi_stat_WDATA,
  s_axi_stat_WREADY,
  s_axi_stat_WSTRB,
  s_axi_stat_WVALID,
  ap_clk,
  ap_rst_n,
  iq_in_TDATA,
  iq_in_TREADY,
  iq_in_TVALID,
  iq_out_TDATA,
  iq_out_TREADY,
  iq_out_TVALID,
  n_syms_fb,
  n_syms_fb_vld
);

(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat ARADDR" *)
(* X_INTERFACE_MODE = "slave" *)
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME s_axi_stat, ADDR_WIDTH 6, DATA_WIDTH 32, PROTOCOL AXI4LITE, READ_WRITE_MODE READ_WRITE, FREQ_HZ 100000000, ID_WIDTH 0, AWUSER_WIDTH 0, ARUSER_WIDTH 0, WUSER_WIDTH 0, RUSER_WIDTH 0, BUSER_WIDTH 0, HAS_BURST 0, HAS_LOCK 0, HAS_PROT 0, HAS_CACHE 0, HAS_QOS 0, HAS_REGION 0, HAS_WSTRB 1, HAS_BRESP 1, HAS_RRESP 1, SUPPORTS_NARROW_BURST 0, NUM_READ_OUTSTANDING 1, NUM_WRITE_OUTSTANDING 1, MAX_BURST_LENGTH 1, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, NUM_READ_THREADS 1, NUM_WRITE_THREADS 1,\
 RUSER_BITS_PER_BYTE 0, WUSER_BITS_PER_BYTE 0, INSERT_VIP 0" *)
input wire [5 : 0] s_axi_stat_ARADDR;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat ARREADY" *)
output wire s_axi_stat_ARREADY;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat ARVALID" *)
input wire s_axi_stat_ARVALID;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat AWADDR" *)
input wire [5 : 0] s_axi_stat_AWADDR;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat AWREADY" *)
output wire s_axi_stat_AWREADY;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat AWVALID" *)
input wire s_axi_stat_AWVALID;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat BREADY" *)
input wire s_axi_stat_BREADY;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat BRESP" *)
output wire [1 : 0] s_axi_stat_BRESP;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat BVALID" *)
output wire s_axi_stat_BVALID;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat RDATA" *)
output wire [31 : 0] s_axi_stat_RDATA;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat RREADY" *)
input wire s_axi_stat_RREADY;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat RRESP" *)
output wire [1 : 0] s_axi_stat_RRESP;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat RVALID" *)
output wire s_axi_stat_RVALID;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat WDATA" *)
input wire [31 : 0] s_axi_stat_WDATA;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat WREADY" *)
output wire s_axi_stat_WREADY;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat WSTRB" *)
input wire [3 : 0] s_axi_stat_WSTRB;
(* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 s_axi_stat WVALID" *)
input wire s_axi_stat_WVALID;
(* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 ap_clk CLK" *)
(* X_INTERFACE_MODE = "slave" *)
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME ap_clk, ASSOCIATED_BUSIF s_axi_stat:iq_in:iq_out, ASSOCIATED_RESET ap_rst_n, FREQ_HZ 100000000, FREQ_TOLERANCE_HZ 0, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, INSERT_VIP 0" *)
input wire ap_clk;
(* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 ap_rst_n RST" *)
(* X_INTERFACE_MODE = "slave" *)
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME ap_rst_n, POLARITY ACTIVE_LOW, INSERT_VIP 0" *)
input wire ap_rst_n;
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 iq_in TDATA" *)
(* X_INTERFACE_MODE = "slave" *)
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME iq_in, TUSER_WIDTH 0, TDATA_NUM_BYTES 6, TDEST_WIDTH 0, TID_WIDTH 0, HAS_TREADY 1, HAS_TSTRB 0, HAS_TKEEP 0, HAS_TLAST 0, FREQ_HZ 100000000, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, LAYERED_METADATA undef, INSERT_VIP 0" *)
input wire [47 : 0] iq_in_TDATA;
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 iq_in TREADY" *)
output wire iq_in_TREADY;
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 iq_in TVALID" *)
input wire iq_in_TVALID;
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 iq_out TDATA" *)
(* X_INTERFACE_MODE = "master" *)
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME iq_out, TUSER_WIDTH 0, TDATA_NUM_BYTES 6, TDEST_WIDTH 0, TID_WIDTH 0, HAS_TREADY 1, HAS_TSTRB 0, HAS_TKEEP 0, HAS_TLAST 0, FREQ_HZ 100000000, PHASE 0.0, CLK_DOMAIN ofdm_chain_clk, LAYERED_METADATA undef, INSERT_VIP 0" *)
output wire [47 : 0] iq_out_TDATA;
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 iq_out TREADY" *)
input wire iq_out_TREADY;
(* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 iq_out TVALID" *)
output wire iq_out_TVALID;
(* X_INTERFACE_INFO = "xilinx.com:signal:data:1.0 n_syms_fb DATA" *)
(* X_INTERFACE_MODE = "slave" *)
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME n_syms_fb, LAYERED_METADATA undef" *)
input wire [7 : 0] n_syms_fb;
(* X_INTERFACE_INFO = "xilinx.com:signal:data:1.0 n_syms_fb_vld DATA" *)
(* X_INTERFACE_MODE = "slave" *)
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME n_syms_fb_vld, LAYERED_METADATA undef" *)
input wire [0 : 0] n_syms_fb_vld;

(* SDX_KERNEL = "true" *)
(* SDX_KERNEL_TYPE = "hls" *)
(* SDX_KERNEL_SIM_INST = "" *)
  sync_detect #(
    .C_S_AXI_STAT_ADDR_WIDTH(6),
    .C_S_AXI_STAT_DATA_WIDTH(32)
  ) inst (
    .s_axi_stat_ARADDR(s_axi_stat_ARADDR),
    .s_axi_stat_ARREADY(s_axi_stat_ARREADY),
    .s_axi_stat_ARVALID(s_axi_stat_ARVALID),
    .s_axi_stat_AWADDR(s_axi_stat_AWADDR),
    .s_axi_stat_AWREADY(s_axi_stat_AWREADY),
    .s_axi_stat_AWVALID(s_axi_stat_AWVALID),
    .s_axi_stat_BREADY(s_axi_stat_BREADY),
    .s_axi_stat_BRESP(s_axi_stat_BRESP),
    .s_axi_stat_BVALID(s_axi_stat_BVALID),
    .s_axi_stat_RDATA(s_axi_stat_RDATA),
    .s_axi_stat_RREADY(s_axi_stat_RREADY),
    .s_axi_stat_RRESP(s_axi_stat_RRESP),
    .s_axi_stat_RVALID(s_axi_stat_RVALID),
    .s_axi_stat_WDATA(s_axi_stat_WDATA),
    .s_axi_stat_WREADY(s_axi_stat_WREADY),
    .s_axi_stat_WSTRB(s_axi_stat_WSTRB),
    .s_axi_stat_WVALID(s_axi_stat_WVALID),
    .ap_clk(ap_clk),
    .ap_rst_n(ap_rst_n),
    .iq_in_TDATA(iq_in_TDATA),
    .iq_in_TREADY(iq_in_TREADY),
    .iq_in_TVALID(iq_in_TVALID),
    .iq_out_TDATA(iq_out_TDATA),
    .iq_out_TREADY(iq_out_TREADY),
    .iq_out_TVALID(iq_out_TVALID),
    .n_syms_fb(n_syms_fb),
    .n_syms_fb_vld(n_syms_fb_vld)
  );
endmodule
