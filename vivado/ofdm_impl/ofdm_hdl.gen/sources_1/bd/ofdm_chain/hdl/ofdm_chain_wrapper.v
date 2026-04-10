//Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
//Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2025.2 (lin64) Build 6299465 Fri Nov 14 12:34:56 MST 2025
//Date        : Fri Apr 10 09:39:49 2026
//Host        : LPT1146 running 64-bit Ubuntu 24.04.3 LTS
//Command     : generate_target ofdm_chain_wrapper.bd
//Design      : ofdm_chain_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module ofdm_chain_wrapper
   (clk,
    host_rx_out_tdata,
    host_rx_out_tready,
    host_rx_out_tvalid,
    host_tx_in_tdata,
    host_tx_in_tready,
    host_tx_in_tvalid,
    rf_rx_in_tdata,
    rf_rx_in_tready,
    rf_rx_in_tvalid,
    rf_tx_out_tdata,
    rf_tx_out_tready,
    rf_tx_out_tvalid,
    rst_n);
  input clk;
  output [7:0]host_rx_out_tdata;
  input host_rx_out_tready;
  output host_rx_out_tvalid;
  input [7:0]host_tx_in_tdata;
  output host_tx_in_tready;
  input host_tx_in_tvalid;
  input [47:0]rf_rx_in_tdata;
  output rf_rx_in_tready;
  input rf_rx_in_tvalid;
  output [47:0]rf_tx_out_tdata;
  input rf_tx_out_tready;
  output rf_tx_out_tvalid;
  input rst_n;

  wire clk;
  wire [7:0]host_rx_out_tdata;
  wire host_rx_out_tready;
  wire host_rx_out_tvalid;
  wire [7:0]host_tx_in_tdata;
  wire host_tx_in_tready;
  wire host_tx_in_tvalid;
  wire [47:0]rf_rx_in_tdata;
  wire rf_rx_in_tready;
  wire rf_rx_in_tvalid;
  wire [47:0]rf_tx_out_tdata;
  wire rf_tx_out_tready;
  wire rf_tx_out_tvalid;
  wire rst_n;

  ofdm_chain ofdm_chain_i
       (.clk(clk),
        .host_rx_out_tdata(host_rx_out_tdata),
        .host_rx_out_tready(host_rx_out_tready),
        .host_rx_out_tvalid(host_rx_out_tvalid),
        .host_tx_in_tdata(host_tx_in_tdata),
        .host_tx_in_tready(host_tx_in_tready),
        .host_tx_in_tvalid(host_tx_in_tvalid),
        .rf_rx_in_tdata(rf_rx_in_tdata),
        .rf_rx_in_tready(rf_rx_in_tready),
        .rf_rx_in_tvalid(rf_rx_in_tvalid),
        .rf_tx_out_tdata(rf_tx_out_tdata),
        .rf_tx_out_tready(rf_tx_out_tready),
        .rf_tx_out_tvalid(rf_tx_out_tvalid),
        .rst_n(rst_n));
endmodule
