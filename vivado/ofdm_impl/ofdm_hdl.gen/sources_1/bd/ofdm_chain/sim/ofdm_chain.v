//Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
//Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2025.2 (lin64) Build 6299465 Fri Nov 14 12:34:56 MST 2025
//Date        : Fri Apr 10 09:39:49 2026
//Host        : LPT1146 running 64-bit Ubuntu 24.04.3 LTS
//Command     : generate_target ofdm_chain.bd
//Design      : ofdm_chain
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* CORE_GENERATION_INFO = "ofdm_chain,IP_Integrator,{x_ipVendor=xilinx.com,x_ipLibrary=BlockDiagram,x_ipName=ofdm_chain,x_ipVersion=1.00.a,x_ipLanguage=VERILOG,numBlks=15,numReposBlks=15,numNonXlnxBlks=10,numHierBlks=0,maxHierDepth=0,numSysgenBlks=0,numHlsBlks=10,numHdlrefBlks=0,numPkgbdBlks=0,bdsource=USER,synth_mode=Hierarchical}" *) (* HW_HANDOFF = "ofdm_chain.hwdef" *) 
module ofdm_chain
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
  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 CLK.CLK CLK" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME CLK.CLK, ASSOCIATED_BUSIF host_tx_in:rf_tx_out:rf_rx_in:host_rx_out, ASSOCIATED_RESET rst_n, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, FREQ_TOLERANCE_HZ 0, INSERT_VIP 0, PHASE 0.0" *) input clk;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TDATA" *) (* X_INTERFACE_MODE = "Master" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME host_rx_out, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, HAS_TKEEP 0, HAS_TLAST 0, HAS_TREADY 1, HAS_TSTRB 0, INSERT_VIP 0, LAYERED_METADATA undef, PHASE 0.0, TDATA_NUM_BYTES 1, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0" *) output [7:0]host_rx_out_tdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TREADY" *) input host_rx_out_tready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TVALID" *) output host_rx_out_tvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TDATA" *) (* X_INTERFACE_MODE = "Slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME host_tx_in, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, HAS_TKEEP 0, HAS_TLAST 0, HAS_TREADY 1, HAS_TSTRB 0, INSERT_VIP 0, LAYERED_METADATA undef, PHASE 0.0, TDATA_NUM_BYTES 1, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0" *) input [7:0]host_tx_in_tdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TREADY" *) output host_tx_in_tready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TVALID" *) input host_tx_in_tvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_rx_in TDATA" *) (* X_INTERFACE_MODE = "Slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rf_rx_in, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, HAS_TKEEP 0, HAS_TLAST 0, HAS_TREADY 1, HAS_TSTRB 0, INSERT_VIP 0, LAYERED_METADATA undef, PHASE 0.0, TDATA_NUM_BYTES 6, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0" *) input [47:0]rf_rx_in_tdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_rx_in TREADY" *) output rf_rx_in_tready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_rx_in TVALID" *) input rf_rx_in_tvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_tx_out TDATA" *) (* X_INTERFACE_MODE = "Master" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rf_tx_out, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, HAS_TKEEP 0, HAS_TLAST 0, HAS_TREADY 1, HAS_TSTRB 0, INSERT_VIP 0, LAYERED_METADATA undef, PHASE 0.0, TDATA_NUM_BYTES 6, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0" *) output [47:0]rf_tx_out_tdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_tx_out TREADY" *) input rf_tx_out_tready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_tx_out TVALID" *) output rf_tx_out_tvalid;
  input rst_n;

  wire [0:0]cfg_tvalid_dout;
  wire [47:0]cfo_correct_0_iq_out_TDATA;
  wire cfo_correct_0_iq_out_TREADY;
  wire cfo_correct_0_iq_out_TVALID;
  wire clk;
  wire [15:0]fft_cfg_val_dout;
  wire [7:0]host_rx_out_tdata;
  wire host_rx_out_tready;
  wire host_rx_out_tvalid;
  wire [7:0]host_tx_in_tdata;
  wire host_tx_in_tready;
  wire host_tx_in_tvalid;
  wire [15:0]ifft_cfg_val_dout;
  wire [7:0]ofdm_rx_0_bits_out_TDATA;
  wire ofdm_rx_0_bits_out_TREADY;
  wire ofdm_rx_0_bits_out_TVALID;
  wire [47:0]ofdm_rx_0_fft_in_TDATA;
  wire ofdm_rx_0_fft_in_TREADY;
  wire ofdm_rx_0_fft_in_TVALID;
  wire [31:0]ofdm_rx_fft_M_AXIS_DATA_TDATA;
  wire ofdm_rx_fft_M_AXIS_DATA_TREADY;
  wire ofdm_rx_fft_M_AXIS_DATA_TVALID;
  wire [47:0]ofdm_tx_0_ifft_in_TDATA;
  wire ofdm_tx_0_ifft_in_TREADY;
  wire ofdm_tx_0_ifft_in_TVALID;
  wire [31:0]ofdm_tx_ifft_M_AXIS_DATA_TDATA;
  wire ofdm_tx_ifft_M_AXIS_DATA_TREADY;
  wire ofdm_tx_ifft_M_AXIS_DATA_TVALID;
  wire [47:0]rf_rx_in_tdata;
  wire rf_rx_in_tready;
  wire rf_rx_in_tvalid;
  wire [47:0]rf_tx_out_tdata;
  wire rf_tx_out_tready;
  wire rf_tx_out_tvalid;
  wire rst_n;
  wire [7:0]rx_interleaver_data_out_TDATA;
  wire rx_interleaver_data_out_TREADY;
  wire rx_interleaver_data_out_TVALID;
  wire [47:0]sync_detect_0_iq_out_TDATA;
  wire sync_detect_0_iq_out_TREADY;
  wire sync_detect_0_iq_out_TVALID;
  wire [7:0]tx_conv_enc_coded_out_TDATA;
  wire tx_conv_enc_coded_out_TREADY;
  wire tx_conv_enc_coded_out_TVALID;
  wire [7:0]tx_interleaver_data_out_TDATA;
  wire tx_interleaver_data_out_TREADY;
  wire tx_interleaver_data_out_TVALID;
  wire [7:0]tx_scrambler_data_out_TDATA;
  wire tx_scrambler_data_out_TREADY;
  wire tx_scrambler_data_out_TVALID;
  wire [7:0]viterbi_dec_0_data_out_TDATA;
  wire viterbi_dec_0_data_out_TREADY;
  wire viterbi_dec_0_data_out_TVALID;

  ofdm_chain_cfg_tvalid_0 cfg_tvalid
       (.dout(cfg_tvalid_dout));
  ofdm_chain_cfo_correct_0_0 cfo_correct_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .iq_in_TDATA(sync_detect_0_iq_out_TDATA),
        .iq_in_TREADY(sync_detect_0_iq_out_TREADY),
        .iq_in_TVALID(sync_detect_0_iq_out_TVALID),
        .iq_out_TDATA(cfo_correct_0_iq_out_TDATA),
        .iq_out_TREADY(cfo_correct_0_iq_out_TREADY),
        .iq_out_TVALID(cfo_correct_0_iq_out_TVALID),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_fft_cfg_val_0 fft_cfg_val
       (.dout(fft_cfg_val_dout));
  ofdm_chain_ifft_cfg_val_0 ifft_cfg_val
       (.dout(ifft_cfg_val_dout));
  ofdm_chain_ofdm_rx_0_0 ofdm_rx_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .bits_out_TDATA(ofdm_rx_0_bits_out_TDATA),
        .bits_out_TREADY(ofdm_rx_0_bits_out_TREADY),
        .bits_out_TVALID(ofdm_rx_0_bits_out_TVALID),
        .fft_in_TDATA(ofdm_rx_0_fft_in_TDATA),
        .fft_in_TREADY(ofdm_rx_0_fft_in_TREADY),
        .fft_in_TVALID(ofdm_rx_0_fft_in_TVALID),
        .fft_out_TDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,ofdm_rx_fft_M_AXIS_DATA_TDATA}),
        .fft_out_TREADY(ofdm_rx_fft_M_AXIS_DATA_TREADY),
        .fft_out_TVALID(ofdm_rx_fft_M_AXIS_DATA_TVALID),
        .iq_in_TDATA(cfo_correct_0_iq_out_TDATA),
        .iq_in_TREADY(cfo_correct_0_iq_out_TREADY),
        .iq_in_TVALID(cfo_correct_0_iq_out_TVALID),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_ofdm_rx_fft_0 ofdm_rx_fft
       (.aclk(clk),
        .m_axis_data_tdata(ofdm_rx_fft_M_AXIS_DATA_TDATA),
        .m_axis_data_tready(ofdm_rx_fft_M_AXIS_DATA_TREADY),
        .m_axis_data_tvalid(ofdm_rx_fft_M_AXIS_DATA_TVALID),
        .s_axis_config_tdata(fft_cfg_val_dout),
        .s_axis_config_tvalid(cfg_tvalid_dout),
        .s_axis_data_tdata(ofdm_rx_0_fft_in_TDATA[31:0]),
        .s_axis_data_tlast(1'b0),
        .s_axis_data_tready(ofdm_rx_0_fft_in_TREADY),
        .s_axis_data_tvalid(ofdm_rx_0_fft_in_TVALID));
  ofdm_chain_ofdm_tx_0_0 ofdm_tx_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .bits_in_TDATA(tx_interleaver_data_out_TDATA),
        .bits_in_TREADY(tx_interleaver_data_out_TREADY),
        .bits_in_TVALID(tx_interleaver_data_out_TVALID),
        .ifft_in_TDATA(ofdm_tx_0_ifft_in_TDATA),
        .ifft_in_TREADY(ofdm_tx_0_ifft_in_TREADY),
        .ifft_in_TVALID(ofdm_tx_0_ifft_in_TVALID),
        .ifft_out_TDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,ofdm_tx_ifft_M_AXIS_DATA_TDATA}),
        .ifft_out_TREADY(ofdm_tx_ifft_M_AXIS_DATA_TREADY),
        .ifft_out_TVALID(ofdm_tx_ifft_M_AXIS_DATA_TVALID),
        .iq_out_TDATA(rf_tx_out_tdata),
        .iq_out_TREADY(rf_tx_out_tready),
        .iq_out_TVALID(rf_tx_out_tvalid),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_ofdm_tx_ifft_0 ofdm_tx_ifft
       (.aclk(clk),
        .m_axis_data_tdata(ofdm_tx_ifft_M_AXIS_DATA_TDATA),
        .m_axis_data_tready(ofdm_tx_ifft_M_AXIS_DATA_TREADY),
        .m_axis_data_tvalid(ofdm_tx_ifft_M_AXIS_DATA_TVALID),
        .s_axis_config_tdata(ifft_cfg_val_dout),
        .s_axis_config_tvalid(cfg_tvalid_dout),
        .s_axis_data_tdata(ofdm_tx_0_ifft_in_TDATA[31:0]),
        .s_axis_data_tlast(1'b0),
        .s_axis_data_tready(ofdm_tx_0_ifft_in_TREADY),
        .s_axis_data_tvalid(ofdm_tx_0_ifft_in_TVALID));
  ofdm_chain_rx_interleaver_0 rx_interleaver
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .data_in_TDATA(ofdm_rx_0_bits_out_TDATA),
        .data_in_TREADY(ofdm_rx_0_bits_out_TREADY),
        .data_in_TVALID(ofdm_rx_0_bits_out_TVALID),
        .data_out_TDATA(rx_interleaver_data_out_TDATA),
        .data_out_TREADY(rx_interleaver_data_out_TREADY),
        .data_out_TVALID(rx_interleaver_data_out_TVALID),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_rx_scrambler_0 rx_scrambler
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .data_in_TDATA(viterbi_dec_0_data_out_TDATA),
        .data_in_TREADY(viterbi_dec_0_data_out_TREADY),
        .data_in_TVALID(viterbi_dec_0_data_out_TVALID),
        .data_out_TDATA(host_rx_out_tdata),
        .data_out_TREADY(host_rx_out_tready),
        .data_out_TVALID(host_rx_out_tvalid),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_sync_detect_0_0 sync_detect_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .iq_in_TDATA(rf_rx_in_tdata),
        .iq_in_TREADY(rf_rx_in_tready),
        .iq_in_TVALID(rf_rx_in_tvalid),
        .iq_out_TDATA(sync_detect_0_iq_out_TDATA),
        .iq_out_TREADY(sync_detect_0_iq_out_TREADY),
        .iq_out_TVALID(sync_detect_0_iq_out_TVALID),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_tx_conv_enc_0 tx_conv_enc
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .coded_out_TDATA(tx_conv_enc_coded_out_TDATA),
        .coded_out_TREADY(tx_conv_enc_coded_out_TREADY),
        .coded_out_TVALID(tx_conv_enc_coded_out_TVALID),
        .data_in_TDATA(tx_scrambler_data_out_TDATA),
        .data_in_TREADY(tx_scrambler_data_out_TREADY),
        .data_in_TVALID(tx_scrambler_data_out_TVALID),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_tx_interleaver_0 tx_interleaver
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .data_in_TDATA(tx_conv_enc_coded_out_TDATA),
        .data_in_TREADY(tx_conv_enc_coded_out_TREADY),
        .data_in_TVALID(tx_conv_enc_coded_out_TVALID),
        .data_out_TDATA(tx_interleaver_data_out_TDATA),
        .data_out_TREADY(tx_interleaver_data_out_TREADY),
        .data_out_TVALID(tx_interleaver_data_out_TVALID),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_tx_scrambler_0 tx_scrambler
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .data_in_TDATA(host_tx_in_tdata),
        .data_in_TREADY(host_tx_in_tready),
        .data_in_TVALID(host_tx_in_tvalid),
        .data_out_TDATA(tx_scrambler_data_out_TDATA),
        .data_out_TREADY(tx_scrambler_data_out_TREADY),
        .data_out_TVALID(tx_scrambler_data_out_TVALID),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
  ofdm_chain_viterbi_dec_0_0 viterbi_dec_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .coded_in_TDATA(rx_interleaver_data_out_TDATA),
        .coded_in_TREADY(rx_interleaver_data_out_TREADY),
        .coded_in_TVALID(rx_interleaver_data_out_TVALID),
        .data_out_TDATA(viterbi_dec_0_data_out_TDATA),
        .data_out_TREADY(viterbi_dec_0_data_out_TREADY),
        .data_out_TVALID(viterbi_dec_0_data_out_TVALID),
        .s_axi_ctrl_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_ARVALID(1'b0),
        .s_axi_ctrl_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_AWVALID(1'b0),
        .s_axi_ctrl_BREADY(1'b0),
        .s_axi_ctrl_RREADY(1'b0),
        .s_axi_ctrl_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_ctrl_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_ctrl_WVALID(1'b0));
endmodule
