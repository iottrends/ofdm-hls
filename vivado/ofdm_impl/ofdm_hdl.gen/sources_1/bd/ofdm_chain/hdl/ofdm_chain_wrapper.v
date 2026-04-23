//Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
//Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2025.2 (lin64) Build 6299465 Fri Nov 14 12:34:56 MST 2025
//Date        : Thu Apr 16 09:50:19 2026
//Host        : DESKTOP-KO4U942 running 64-bit Ubuntu 24.04.4 LTS
//Command     : generate_target ofdm_chain_wrapper.bd
//Design      : ofdm_chain_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module ofdm_chain_wrapper
   (clk,
    clk_fec,
    ctrl_axi_araddr,
    ctrl_axi_arprot,
    ctrl_axi_arready,
    ctrl_axi_arvalid,
    ctrl_axi_awaddr,
    ctrl_axi_awprot,
    ctrl_axi_awready,
    ctrl_axi_awvalid,
    ctrl_axi_bready,
    ctrl_axi_bresp,
    ctrl_axi_bvalid,
    ctrl_axi_rdata,
    ctrl_axi_rready,
    ctrl_axi_rresp,
    ctrl_axi_rvalid,
    ctrl_axi_wdata,
    ctrl_axi_wready,
    ctrl_axi_wstrb,
    ctrl_axi_wvalid,
    host_rx_out_tdata,
    host_rx_out_tkeep,
    host_rx_out_tlast,
    host_rx_out_tready,
    host_rx_out_tstrb,
    host_rx_out_tvalid,
    host_tx_in_tdata,
    host_tx_in_tkeep,
    host_tx_in_tlast,
    host_tx_in_tready,
    host_tx_in_tstrb,
    host_tx_in_tvalid,
    mac_rx_pkt_pulse,
    mac_tx_done_pulse,
    rf_rx_in_tdata,
    rf_rx_in_tready,
    rf_rx_in_tvalid,
    rf_tx_out_tdata,
    rf_tx_out_tready,
    rf_tx_out_tvalid,
    rst_fec_n,
    rst_n);
  input clk;
  input clk_fec;
  input [15:0]ctrl_axi_araddr;
  input [2:0]ctrl_axi_arprot;
  output ctrl_axi_arready;
  input ctrl_axi_arvalid;
  input [15:0]ctrl_axi_awaddr;
  input [2:0]ctrl_axi_awprot;
  output ctrl_axi_awready;
  input ctrl_axi_awvalid;
  input ctrl_axi_bready;
  output [1:0]ctrl_axi_bresp;
  output ctrl_axi_bvalid;
  output [31:0]ctrl_axi_rdata;
  input ctrl_axi_rready;
  output [1:0]ctrl_axi_rresp;
  output ctrl_axi_rvalid;
  input [31:0]ctrl_axi_wdata;
  output ctrl_axi_wready;
  input [3:0]ctrl_axi_wstrb;
  input ctrl_axi_wvalid;
  output [7:0]host_rx_out_tdata;
  output [0:0]host_rx_out_tkeep;
  output host_rx_out_tlast;
  input host_rx_out_tready;
  output [0:0]host_rx_out_tstrb;
  output host_rx_out_tvalid;
  input [7:0]host_tx_in_tdata;
  input [0:0]host_tx_in_tkeep;
  input [0:0]host_tx_in_tlast;
  output host_tx_in_tready;
  input [0:0]host_tx_in_tstrb;
  input host_tx_in_tvalid;
  output [0:0]mac_rx_pkt_pulse;
  output [0:0]mac_tx_done_pulse;
  input [39:0]rf_rx_in_tdata;
  output rf_rx_in_tready;
  input rf_rx_in_tvalid;
  output [47:0]rf_tx_out_tdata;
  input rf_tx_out_tready;
  output rf_tx_out_tvalid;
  input rst_fec_n;
  input rst_n;

  wire clk;
  wire clk_fec;
  wire [15:0]ctrl_axi_araddr;
  wire [2:0]ctrl_axi_arprot;
  wire ctrl_axi_arready;
  wire ctrl_axi_arvalid;
  wire [15:0]ctrl_axi_awaddr;
  wire [2:0]ctrl_axi_awprot;
  wire ctrl_axi_awready;
  wire ctrl_axi_awvalid;
  wire ctrl_axi_bready;
  wire [1:0]ctrl_axi_bresp;
  wire ctrl_axi_bvalid;
  wire [31:0]ctrl_axi_rdata;
  wire ctrl_axi_rready;
  wire [1:0]ctrl_axi_rresp;
  wire ctrl_axi_rvalid;
  wire [31:0]ctrl_axi_wdata;
  wire ctrl_axi_wready;
  wire [3:0]ctrl_axi_wstrb;
  wire ctrl_axi_wvalid;
  wire [7:0]host_rx_out_tdata;
  wire [0:0]host_rx_out_tkeep;
  wire host_rx_out_tlast;
  wire host_rx_out_tready;
  wire [0:0]host_rx_out_tstrb;
  wire host_rx_out_tvalid;
  wire [7:0]host_tx_in_tdata;
  wire [0:0]host_tx_in_tkeep;
  wire [0:0]host_tx_in_tlast;
  wire host_tx_in_tready;
  wire [0:0]host_tx_in_tstrb;
  wire host_tx_in_tvalid;
  wire [0:0]mac_rx_pkt_pulse;
  wire [0:0]mac_tx_done_pulse;
  wire [39:0]rf_rx_in_tdata;
  wire rf_rx_in_tready;
  wire rf_rx_in_tvalid;
  wire [47:0]rf_tx_out_tdata;
  wire rf_tx_out_tready;
  wire rf_tx_out_tvalid;
  wire rst_fec_n;
  wire rst_n;

  ofdm_chain ofdm_chain_i
       (.clk(clk),
        .clk_fec(clk_fec),
        .ctrl_axi_araddr(ctrl_axi_araddr),
        .ctrl_axi_arprot(ctrl_axi_arprot),
        .ctrl_axi_arready(ctrl_axi_arready),
        .ctrl_axi_arvalid(ctrl_axi_arvalid),
        .ctrl_axi_awaddr(ctrl_axi_awaddr),
        .ctrl_axi_awprot(ctrl_axi_awprot),
        .ctrl_axi_awready(ctrl_axi_awready),
        .ctrl_axi_awvalid(ctrl_axi_awvalid),
        .ctrl_axi_bready(ctrl_axi_bready),
        .ctrl_axi_bresp(ctrl_axi_bresp),
        .ctrl_axi_bvalid(ctrl_axi_bvalid),
        .ctrl_axi_rdata(ctrl_axi_rdata),
        .ctrl_axi_rready(ctrl_axi_rready),
        .ctrl_axi_rresp(ctrl_axi_rresp),
        .ctrl_axi_rvalid(ctrl_axi_rvalid),
        .ctrl_axi_wdata(ctrl_axi_wdata),
        .ctrl_axi_wready(ctrl_axi_wready),
        .ctrl_axi_wstrb(ctrl_axi_wstrb),
        .ctrl_axi_wvalid(ctrl_axi_wvalid),
        .host_rx_out_tdata(host_rx_out_tdata),
        .host_rx_out_tkeep(host_rx_out_tkeep),
        .host_rx_out_tlast(host_rx_out_tlast),
        .host_rx_out_tready(host_rx_out_tready),
        .host_rx_out_tstrb(host_rx_out_tstrb),
        .host_rx_out_tvalid(host_rx_out_tvalid),
        .host_tx_in_tdata(host_tx_in_tdata),
        .host_tx_in_tkeep(host_tx_in_tkeep),
        .host_tx_in_tlast(host_tx_in_tlast),
        .host_tx_in_tready(host_tx_in_tready),
        .host_tx_in_tstrb(host_tx_in_tstrb),
        .host_tx_in_tvalid(host_tx_in_tvalid),
        .mac_rx_pkt_pulse(mac_rx_pkt_pulse),
        .mac_tx_done_pulse(mac_tx_done_pulse),
        .rf_rx_in_tdata(rf_rx_in_tdata),
        .rf_rx_in_tready(rf_rx_in_tready),
        .rf_rx_in_tvalid(rf_rx_in_tvalid),
        .rf_tx_out_tdata(rf_tx_out_tdata),
        .rf_tx_out_tready(rf_tx_out_tready),
        .rf_tx_out_tvalid(rf_tx_out_tvalid),
        .rst_fec_n(rst_fec_n),
        .rst_n(rst_n));
endmodule
