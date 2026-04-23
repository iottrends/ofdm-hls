//Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
//Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2025.2 (lin64) Build 6299465 Fri Nov 14 12:34:56 MST 2025
//Date        : Thu Apr 16 09:50:18 2026
//Host        : DESKTOP-KO4U942 running 64-bit Ubuntu 24.04.4 LTS
//Command     : generate_target ofdm_chain.bd
//Design      : ofdm_chain
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

(* CORE_GENERATION_INFO = "ofdm_chain,IP_Integrator,{x_ipVendor=xilinx.com,x_ipLibrary=BlockDiagram,x_ipName=ofdm_chain,x_ipVersion=1.00.a,x_ipLanguage=VERILOG,numBlks=17,numReposBlks=17,numNonXlnxBlks=6,numHierBlks=0,maxHierDepth=0,numSysgenBlks=0,numHlsBlks=6,numHdlrefBlks=0,numPkgbdBlks=0,bdsource=USER,synth_mode=Hierarchical}" *) (* HW_HANDOFF = "ofdm_chain.hwdef" *) 
module ofdm_chain
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
  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 CLK.CLK CLK" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME CLK.CLK, ASSOCIATED_BUSIF host_tx_in:rf_tx_out:rf_rx_in:host_rx_out:ctrl_axi, ASSOCIATED_RESET rst_n, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, FREQ_TOLERANCE_HZ 0, INSERT_VIP 0, PHASE 0.0" *) input clk;
  (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 CLK.CLK_FEC CLK" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME CLK.CLK_FEC, ASSOCIATED_RESET rst_fec_n, CLK_DOMAIN ofdm_chain_clk_fec, FREQ_HZ 200000000, FREQ_TOLERANCE_HZ 0, INSERT_VIP 0, PHASE 0.0" *) input clk_fec;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi ARADDR" *) (* X_INTERFACE_MODE = "Slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME ctrl_axi, ADDR_WIDTH 16, ARUSER_WIDTH 0, AWUSER_WIDTH 0, BUSER_WIDTH 0, CLK_DOMAIN ofdm_chain_clk, DATA_WIDTH 32, FREQ_HZ 100000000, HAS_BRESP 1, HAS_BURST 1, HAS_CACHE 1, HAS_LOCK 1, HAS_PROT 1, HAS_QOS 1, HAS_REGION 1, HAS_RRESP 1, HAS_WSTRB 1, ID_WIDTH 0, INSERT_VIP 0, MAX_BURST_LENGTH 1, NUM_READ_OUTSTANDING 1, NUM_READ_THREADS 1, NUM_WRITE_OUTSTANDING 1, NUM_WRITE_THREADS 1, PHASE 0.0, PROTOCOL AXI4LITE, READ_WRITE_MODE READ_WRITE, RUSER_BITS_PER_BYTE 0, RUSER_WIDTH 0, SUPPORTS_NARROW_BURST 0, WUSER_BITS_PER_BYTE 0, WUSER_WIDTH 0" *) input [15:0]ctrl_axi_araddr;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi ARPROT" *) input [2:0]ctrl_axi_arprot;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi ARREADY" *) output ctrl_axi_arready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi ARVALID" *) input ctrl_axi_arvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi AWADDR" *) input [15:0]ctrl_axi_awaddr;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi AWPROT" *) input [2:0]ctrl_axi_awprot;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi AWREADY" *) output ctrl_axi_awready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi AWVALID" *) input ctrl_axi_awvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi BREADY" *) input ctrl_axi_bready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi BRESP" *) output [1:0]ctrl_axi_bresp;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi BVALID" *) output ctrl_axi_bvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi RDATA" *) output [31:0]ctrl_axi_rdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi RREADY" *) input ctrl_axi_rready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi RRESP" *) output [1:0]ctrl_axi_rresp;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi RVALID" *) output ctrl_axi_rvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi WDATA" *) input [31:0]ctrl_axi_wdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi WREADY" *) output ctrl_axi_wready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi WSTRB" *) input [3:0]ctrl_axi_wstrb;
  (* X_INTERFACE_INFO = "xilinx.com:interface:aximm:1.0 ctrl_axi WVALID" *) input ctrl_axi_wvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TDATA" *) (* X_INTERFACE_MODE = "Master" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME host_rx_out, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, HAS_TKEEP 1, HAS_TLAST 1, HAS_TREADY 1, HAS_TSTRB 1, INSERT_VIP 0, LAYERED_METADATA undef, PHASE 0.0, TDATA_NUM_BYTES 1, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0" *) output [7:0]host_rx_out_tdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TKEEP" *) output [0:0]host_rx_out_tkeep;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TLAST" *) output host_rx_out_tlast;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TREADY" *) input host_rx_out_tready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TSTRB" *) output [0:0]host_rx_out_tstrb;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_rx_out TVALID" *) output host_rx_out_tvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TDATA" *) (* X_INTERFACE_MODE = "Slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME host_tx_in, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, HAS_TKEEP 1, HAS_TLAST 1, HAS_TREADY 1, HAS_TSTRB 1, INSERT_VIP 0, LAYERED_METADATA undef, PHASE 0.0, TDATA_NUM_BYTES 1, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0" *) input [7:0]host_tx_in_tdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TKEEP" *) input [0:0]host_tx_in_tkeep;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TLAST" *) input [0:0]host_tx_in_tlast;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TREADY" *) output host_tx_in_tready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TSTRB" *) input [0:0]host_tx_in_tstrb;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 host_tx_in TVALID" *) input host_tx_in_tvalid;
  (* X_INTERFACE_INFO = "xilinx.com:signal:data:1.0 DATA.MAC_RX_PKT_PULSE DATA" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME DATA.MAC_RX_PKT_PULSE, LAYERED_METADATA undef" *) output [0:0]mac_rx_pkt_pulse;
  (* X_INTERFACE_INFO = "xilinx.com:signal:data:1.0 DATA.MAC_TX_DONE_PULSE DATA" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME DATA.MAC_TX_DONE_PULSE, LAYERED_METADATA undef" *) output [0:0]mac_tx_done_pulse;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_rx_in TDATA" *) (* X_INTERFACE_MODE = "Slave" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rf_rx_in, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, HAS_TKEEP 0, HAS_TLAST 0, HAS_TREADY 1, HAS_TSTRB 0, INSERT_VIP 0, LAYERED_METADATA undef, PHASE 0.0, TDATA_NUM_BYTES 5, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0" *) input [39:0]rf_rx_in_tdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_rx_in TREADY" *) output rf_rx_in_tready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_rx_in TVALID" *) input rf_rx_in_tvalid;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_tx_out TDATA" *) (* X_INTERFACE_MODE = "Master" *) (* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME rf_tx_out, CLK_DOMAIN ofdm_chain_clk, FREQ_HZ 100000000, HAS_TKEEP 0, HAS_TLAST 0, HAS_TREADY 1, HAS_TSTRB 0, INSERT_VIP 0, LAYERED_METADATA undef, PHASE 0.0, TDATA_NUM_BYTES 6, TDEST_WIDTH 0, TID_WIDTH 0, TUSER_WIDTH 0" *) output [47:0]rf_tx_out_tdata;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_tx_out TREADY" *) input rf_tx_out_tready;
  (* X_INTERFACE_INFO = "xilinx.com:interface:axis:1.0 rf_tx_out TVALID" *) output rf_tx_out_tvalid;
  input rst_fec_n;
  input rst_n;

  wire [39:0]adc_input_fifo_M_AXIS_TDATA;
  wire adc_input_fifo_M_AXIS_TREADY;
  wire adc_input_fifo_M_AXIS_TVALID;
  wire [0:0]cfg_tvalid_dout;
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
  wire [5:0]ctrl_xbar_M00_AXI_ARADDR;
  wire ctrl_xbar_M00_AXI_ARREADY;
  wire ctrl_xbar_M00_AXI_ARVALID;
  wire [5:0]ctrl_xbar_M00_AXI_AWADDR;
  wire ctrl_xbar_M00_AXI_AWREADY;
  wire ctrl_xbar_M00_AXI_AWVALID;
  wire ctrl_xbar_M00_AXI_BREADY;
  wire [1:0]ctrl_xbar_M00_AXI_BRESP;
  wire ctrl_xbar_M00_AXI_BVALID;
  wire [31:0]ctrl_xbar_M00_AXI_RDATA;
  wire ctrl_xbar_M00_AXI_RREADY;
  wire [1:0]ctrl_xbar_M00_AXI_RRESP;
  wire ctrl_xbar_M00_AXI_RVALID;
  wire [31:0]ctrl_xbar_M00_AXI_WDATA;
  wire ctrl_xbar_M00_AXI_WREADY;
  wire [3:0]ctrl_xbar_M00_AXI_WSTRB;
  wire ctrl_xbar_M00_AXI_WVALID;
  wire [4:0]ctrl_xbar_M01_AXI_ARADDR;
  wire ctrl_xbar_M01_AXI_ARREADY;
  wire ctrl_xbar_M01_AXI_ARVALID;
  wire [4:0]ctrl_xbar_M01_AXI_AWADDR;
  wire ctrl_xbar_M01_AXI_AWREADY;
  wire ctrl_xbar_M01_AXI_AWVALID;
  wire ctrl_xbar_M01_AXI_BREADY;
  wire [1:0]ctrl_xbar_M01_AXI_BRESP;
  wire ctrl_xbar_M01_AXI_BVALID;
  wire [31:0]ctrl_xbar_M01_AXI_RDATA;
  wire ctrl_xbar_M01_AXI_RREADY;
  wire [1:0]ctrl_xbar_M01_AXI_RRESP;
  wire ctrl_xbar_M01_AXI_RVALID;
  wire [31:0]ctrl_xbar_M01_AXI_WDATA;
  wire ctrl_xbar_M01_AXI_WREADY;
  wire [3:0]ctrl_xbar_M01_AXI_WSTRB;
  wire ctrl_xbar_M01_AXI_WVALID;
  wire [5:0]ctrl_xbar_M02_AXI_ARADDR;
  wire ctrl_xbar_M02_AXI_ARREADY;
  wire ctrl_xbar_M02_AXI_ARVALID;
  wire [5:0]ctrl_xbar_M02_AXI_AWADDR;
  wire ctrl_xbar_M02_AXI_AWREADY;
  wire ctrl_xbar_M02_AXI_AWVALID;
  wire ctrl_xbar_M02_AXI_BREADY;
  wire [1:0]ctrl_xbar_M02_AXI_BRESP;
  wire ctrl_xbar_M02_AXI_BVALID;
  wire [31:0]ctrl_xbar_M02_AXI_RDATA;
  wire ctrl_xbar_M02_AXI_RREADY;
  wire [1:0]ctrl_xbar_M02_AXI_RRESP;
  wire ctrl_xbar_M02_AXI_RVALID;
  wire [31:0]ctrl_xbar_M02_AXI_WDATA;
  wire ctrl_xbar_M02_AXI_WREADY;
  wire [3:0]ctrl_xbar_M02_AXI_WSTRB;
  wire ctrl_xbar_M02_AXI_WVALID;
  wire [4:0]ctrl_xbar_M03_AXI_ARADDR;
  wire ctrl_xbar_M03_AXI_ARREADY;
  wire ctrl_xbar_M03_AXI_ARVALID;
  wire [4:0]ctrl_xbar_M03_AXI_AWADDR;
  wire ctrl_xbar_M03_AXI_AWREADY;
  wire ctrl_xbar_M03_AXI_AWVALID;
  wire ctrl_xbar_M03_AXI_BREADY;
  wire [1:0]ctrl_xbar_M03_AXI_BRESP;
  wire ctrl_xbar_M03_AXI_BVALID;
  wire [31:0]ctrl_xbar_M03_AXI_RDATA;
  wire ctrl_xbar_M03_AXI_RREADY;
  wire [1:0]ctrl_xbar_M03_AXI_RRESP;
  wire ctrl_xbar_M03_AXI_RVALID;
  wire [31:0]ctrl_xbar_M03_AXI_WDATA;
  wire ctrl_xbar_M03_AXI_WREADY;
  wire [3:0]ctrl_xbar_M03_AXI_WSTRB;
  wire ctrl_xbar_M03_AXI_WVALID;
  wire [7:0]ctrl_xbar_M04_AXI_ARADDR;
  wire ctrl_xbar_M04_AXI_ARREADY;
  wire ctrl_xbar_M04_AXI_ARVALID;
  wire [7:0]ctrl_xbar_M04_AXI_AWADDR;
  wire ctrl_xbar_M04_AXI_AWREADY;
  wire ctrl_xbar_M04_AXI_AWVALID;
  wire ctrl_xbar_M04_AXI_BREADY;
  wire [1:0]ctrl_xbar_M04_AXI_BRESP;
  wire ctrl_xbar_M04_AXI_BVALID;
  wire [31:0]ctrl_xbar_M04_AXI_RDATA;
  wire ctrl_xbar_M04_AXI_RREADY;
  wire [1:0]ctrl_xbar_M04_AXI_RRESP;
  wire ctrl_xbar_M04_AXI_RVALID;
  wire [31:0]ctrl_xbar_M04_AXI_WDATA;
  wire ctrl_xbar_M04_AXI_WREADY;
  wire [3:0]ctrl_xbar_M04_AXI_WSTRB;
  wire ctrl_xbar_M04_AXI_WVALID;
  wire [7:0]fec_cc1_M_AXIS_TDATA;
  wire fec_cc1_M_AXIS_TREADY;
  wire fec_cc1_M_AXIS_TVALID;
  wire [7:0]fec_cc2_M_AXIS_TDATA;
  wire fec_cc2_M_AXIS_TREADY;
  wire fec_cc2_M_AXIS_TVALID;
  wire [7:0]fec_rx_0_data_out_TDATA;
  wire fec_rx_0_data_out_TREADY;
  wire fec_rx_0_data_out_TVALID;
  wire [15:0]fft_cfg_val_dout;
  wire [0:0]hdr_err_const_dout;
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
  wire [15:0]ifft_cfg_val_dout;
  wire [0:0]mac_rx_pkt_pulse;
  wire [0:0]mac_tx_done_pulse;
  wire [7:0]ofdm_mac_0_host_rx_out_TDATA;
  wire [0:0]ofdm_mac_0_host_rx_out_TKEEP;
  wire [0:0]ofdm_mac_0_host_rx_out_TLAST;
  wire ofdm_mac_0_host_rx_out_TREADY;
  wire [0:0]ofdm_mac_0_host_rx_out_TSTRB;
  wire ofdm_mac_0_host_rx_out_TVALID;
  wire [63:0]ofdm_mac_0_m_axi_csr_master_AWADDR;
  wire [1:0]ofdm_mac_0_m_axi_csr_master_AWBURST;
  wire [3:0]ofdm_mac_0_m_axi_csr_master_AWCACHE;
  wire [0:0]ofdm_mac_0_m_axi_csr_master_AWID;
  wire [7:0]ofdm_mac_0_m_axi_csr_master_AWLEN;
  wire [1:0]ofdm_mac_0_m_axi_csr_master_AWLOCK;
  wire [2:0]ofdm_mac_0_m_axi_csr_master_AWPROT;
  wire [3:0]ofdm_mac_0_m_axi_csr_master_AWQOS;
  wire ofdm_mac_0_m_axi_csr_master_AWREADY;
  wire [2:0]ofdm_mac_0_m_axi_csr_master_AWSIZE;
  wire ofdm_mac_0_m_axi_csr_master_AWVALID;
  wire [0:0]ofdm_mac_0_m_axi_csr_master_BID;
  wire ofdm_mac_0_m_axi_csr_master_BREADY;
  wire [1:0]ofdm_mac_0_m_axi_csr_master_BRESP;
  wire ofdm_mac_0_m_axi_csr_master_BVALID;
  wire [31:0]ofdm_mac_0_m_axi_csr_master_WDATA;
  wire ofdm_mac_0_m_axi_csr_master_WLAST;
  wire ofdm_mac_0_m_axi_csr_master_WREADY;
  wire [3:0]ofdm_mac_0_m_axi_csr_master_WSTRB;
  wire ofdm_mac_0_m_axi_csr_master_WVALID;
  wire [7:0]ofdm_mac_0_phy_tx_out_TDATA;
  wire ofdm_mac_0_phy_tx_out_TREADY;
  wire ofdm_mac_0_phy_tx_out_TVALID;
  wire [7:0]ofdm_rx_0_bits_out_TDATA;
  wire ofdm_rx_0_bits_out_TREADY;
  wire ofdm_rx_0_bits_out_TVALID;
  wire [31:0]ofdm_rx_0_fft_in_TDATA;
  wire ofdm_rx_0_fft_in_TREADY;
  wire ofdm_rx_0_fft_in_TVALID;
  wire [1:0]ofdm_rx_0_modcod_out;
  wire [7:0]ofdm_rx_0_n_syms_fb;
  wire [7:0]ofdm_rx_0_n_syms_out;
  wire [31:0]ofdm_rx_fft_M_AXIS_DATA_TDATA;
  wire ofdm_rx_fft_M_AXIS_DATA_TREADY;
  wire ofdm_rx_fft_M_AXIS_DATA_TVALID;
  wire [31:0]ofdm_tx_0_ifft_in_TDATA;
  wire ofdm_tx_0_ifft_in_TREADY;
  wire ofdm_tx_0_ifft_in_TVALID;
  wire [31:0]ofdm_tx_ifft_M_AXIS_DATA_TDATA;
  wire ofdm_tx_ifft_M_AXIS_DATA_TREADY;
  wire ofdm_tx_ifft_M_AXIS_DATA_TVALID;
  wire [39:0]rf_rx_in_tdata;
  wire rf_rx_in_tready;
  wire rf_rx_in_tvalid;
  wire [47:0]rf_tx_out_tdata;
  wire rf_tx_out_tready;
  wire rf_tx_out_tvalid;
  wire rst_fec_n;
  wire rst_n;
  wire [47:0]sync_detect_0_iq_out_TDATA;
  wire sync_detect_0_iq_out_TREADY;
  wire sync_detect_0_iq_out_TVALID;
  wire [7:0]tx_chain_0_data_out_TDATA;
  wire tx_chain_0_data_out_TREADY;
  wire tx_chain_0_data_out_TVALID;

  ofdm_chain_adc_input_fifo_0 adc_input_fifo
       (.m_axis_tdata(adc_input_fifo_M_AXIS_TDATA),
        .m_axis_tready(adc_input_fifo_M_AXIS_TREADY),
        .m_axis_tvalid(adc_input_fifo_M_AXIS_TVALID),
        .s_axis_aclk(clk),
        .s_axis_aresetn(rst_n),
        .s_axis_tdata(rf_rx_in_tdata),
        .s_axis_tready(rf_rx_in_tready),
        .s_axis_tvalid(rf_rx_in_tvalid));
  ofdm_chain_cfg_tvalid_0 cfg_tvalid
       (.dout(cfg_tvalid_dout));
  ofdm_chain_ctrl_xbar_0 ctrl_xbar
       (.M00_AXI_araddr(ctrl_xbar_M00_AXI_ARADDR),
        .M00_AXI_arready(ctrl_xbar_M00_AXI_ARREADY),
        .M00_AXI_arvalid(ctrl_xbar_M00_AXI_ARVALID),
        .M00_AXI_awaddr(ctrl_xbar_M00_AXI_AWADDR),
        .M00_AXI_awready(ctrl_xbar_M00_AXI_AWREADY),
        .M00_AXI_awvalid(ctrl_xbar_M00_AXI_AWVALID),
        .M00_AXI_bready(ctrl_xbar_M00_AXI_BREADY),
        .M00_AXI_bresp(ctrl_xbar_M00_AXI_BRESP),
        .M00_AXI_bvalid(ctrl_xbar_M00_AXI_BVALID),
        .M00_AXI_rdata(ctrl_xbar_M00_AXI_RDATA),
        .M00_AXI_rready(ctrl_xbar_M00_AXI_RREADY),
        .M00_AXI_rresp(ctrl_xbar_M00_AXI_RRESP),
        .M00_AXI_rvalid(ctrl_xbar_M00_AXI_RVALID),
        .M00_AXI_wdata(ctrl_xbar_M00_AXI_WDATA),
        .M00_AXI_wready(ctrl_xbar_M00_AXI_WREADY),
        .M00_AXI_wstrb(ctrl_xbar_M00_AXI_WSTRB),
        .M00_AXI_wvalid(ctrl_xbar_M00_AXI_WVALID),
        .M01_AXI_araddr(ctrl_xbar_M01_AXI_ARADDR),
        .M01_AXI_arready(ctrl_xbar_M01_AXI_ARREADY),
        .M01_AXI_arvalid(ctrl_xbar_M01_AXI_ARVALID),
        .M01_AXI_awaddr(ctrl_xbar_M01_AXI_AWADDR),
        .M01_AXI_awready(ctrl_xbar_M01_AXI_AWREADY),
        .M01_AXI_awvalid(ctrl_xbar_M01_AXI_AWVALID),
        .M01_AXI_bready(ctrl_xbar_M01_AXI_BREADY),
        .M01_AXI_bresp(ctrl_xbar_M01_AXI_BRESP),
        .M01_AXI_bvalid(ctrl_xbar_M01_AXI_BVALID),
        .M01_AXI_rdata(ctrl_xbar_M01_AXI_RDATA),
        .M01_AXI_rready(ctrl_xbar_M01_AXI_RREADY),
        .M01_AXI_rresp(ctrl_xbar_M01_AXI_RRESP),
        .M01_AXI_rvalid(ctrl_xbar_M01_AXI_RVALID),
        .M01_AXI_wdata(ctrl_xbar_M01_AXI_WDATA),
        .M01_AXI_wready(ctrl_xbar_M01_AXI_WREADY),
        .M01_AXI_wstrb(ctrl_xbar_M01_AXI_WSTRB),
        .M01_AXI_wvalid(ctrl_xbar_M01_AXI_WVALID),
        .M02_AXI_araddr(ctrl_xbar_M02_AXI_ARADDR),
        .M02_AXI_arready(ctrl_xbar_M02_AXI_ARREADY),
        .M02_AXI_arvalid(ctrl_xbar_M02_AXI_ARVALID),
        .M02_AXI_awaddr(ctrl_xbar_M02_AXI_AWADDR),
        .M02_AXI_awready(ctrl_xbar_M02_AXI_AWREADY),
        .M02_AXI_awvalid(ctrl_xbar_M02_AXI_AWVALID),
        .M02_AXI_bready(ctrl_xbar_M02_AXI_BREADY),
        .M02_AXI_bresp(ctrl_xbar_M02_AXI_BRESP),
        .M02_AXI_bvalid(ctrl_xbar_M02_AXI_BVALID),
        .M02_AXI_rdata(ctrl_xbar_M02_AXI_RDATA),
        .M02_AXI_rready(ctrl_xbar_M02_AXI_RREADY),
        .M02_AXI_rresp(ctrl_xbar_M02_AXI_RRESP),
        .M02_AXI_rvalid(ctrl_xbar_M02_AXI_RVALID),
        .M02_AXI_wdata(ctrl_xbar_M02_AXI_WDATA),
        .M02_AXI_wready(ctrl_xbar_M02_AXI_WREADY),
        .M02_AXI_wstrb(ctrl_xbar_M02_AXI_WSTRB),
        .M02_AXI_wvalid(ctrl_xbar_M02_AXI_WVALID),
        .M03_AXI_araddr(ctrl_xbar_M03_AXI_ARADDR),
        .M03_AXI_arready(ctrl_xbar_M03_AXI_ARREADY),
        .M03_AXI_arvalid(ctrl_xbar_M03_AXI_ARVALID),
        .M03_AXI_awaddr(ctrl_xbar_M03_AXI_AWADDR),
        .M03_AXI_awready(ctrl_xbar_M03_AXI_AWREADY),
        .M03_AXI_awvalid(ctrl_xbar_M03_AXI_AWVALID),
        .M03_AXI_bready(ctrl_xbar_M03_AXI_BREADY),
        .M03_AXI_bresp(ctrl_xbar_M03_AXI_BRESP),
        .M03_AXI_bvalid(ctrl_xbar_M03_AXI_BVALID),
        .M03_AXI_rdata(ctrl_xbar_M03_AXI_RDATA),
        .M03_AXI_rready(ctrl_xbar_M03_AXI_RREADY),
        .M03_AXI_rresp(ctrl_xbar_M03_AXI_RRESP),
        .M03_AXI_rvalid(ctrl_xbar_M03_AXI_RVALID),
        .M03_AXI_wdata(ctrl_xbar_M03_AXI_WDATA),
        .M03_AXI_wready(ctrl_xbar_M03_AXI_WREADY),
        .M03_AXI_wstrb(ctrl_xbar_M03_AXI_WSTRB),
        .M03_AXI_wvalid(ctrl_xbar_M03_AXI_WVALID),
        .M04_AXI_araddr(ctrl_xbar_M04_AXI_ARADDR),
        .M04_AXI_arready(ctrl_xbar_M04_AXI_ARREADY),
        .M04_AXI_arvalid(ctrl_xbar_M04_AXI_ARVALID),
        .M04_AXI_awaddr(ctrl_xbar_M04_AXI_AWADDR),
        .M04_AXI_awready(ctrl_xbar_M04_AXI_AWREADY),
        .M04_AXI_awvalid(ctrl_xbar_M04_AXI_AWVALID),
        .M04_AXI_bready(ctrl_xbar_M04_AXI_BREADY),
        .M04_AXI_bresp(ctrl_xbar_M04_AXI_BRESP),
        .M04_AXI_bvalid(ctrl_xbar_M04_AXI_BVALID),
        .M04_AXI_rdata(ctrl_xbar_M04_AXI_RDATA),
        .M04_AXI_rready(ctrl_xbar_M04_AXI_RREADY),
        .M04_AXI_rresp(ctrl_xbar_M04_AXI_RRESP),
        .M04_AXI_rvalid(ctrl_xbar_M04_AXI_RVALID),
        .M04_AXI_wdata(ctrl_xbar_M04_AXI_WDATA),
        .M04_AXI_wready(ctrl_xbar_M04_AXI_WREADY),
        .M04_AXI_wstrb(ctrl_xbar_M04_AXI_WSTRB),
        .M04_AXI_wvalid(ctrl_xbar_M04_AXI_WVALID),
        .S00_AXI_araddr(ctrl_axi_araddr),
        .S00_AXI_arprot(ctrl_axi_arprot),
        .S00_AXI_arready(ctrl_axi_arready),
        .S00_AXI_arvalid(ctrl_axi_arvalid),
        .S00_AXI_awaddr(ctrl_axi_awaddr),
        .S00_AXI_awprot(ctrl_axi_awprot),
        .S00_AXI_awready(ctrl_axi_awready),
        .S00_AXI_awvalid(ctrl_axi_awvalid),
        .S00_AXI_bready(ctrl_axi_bready),
        .S00_AXI_bresp(ctrl_axi_bresp),
        .S00_AXI_bvalid(ctrl_axi_bvalid),
        .S00_AXI_rdata(ctrl_axi_rdata),
        .S00_AXI_rready(ctrl_axi_rready),
        .S00_AXI_rresp(ctrl_axi_rresp),
        .S00_AXI_rvalid(ctrl_axi_rvalid),
        .S00_AXI_wdata(ctrl_axi_wdata),
        .S00_AXI_wready(ctrl_axi_wready),
        .S00_AXI_wstrb(ctrl_axi_wstrb),
        .S00_AXI_wvalid(ctrl_axi_wvalid),
        .S01_AXI_awaddr(ofdm_mac_0_m_axi_csr_master_AWADDR),
        .S01_AXI_awburst(ofdm_mac_0_m_axi_csr_master_AWBURST),
        .S01_AXI_awcache(ofdm_mac_0_m_axi_csr_master_AWCACHE),
        .S01_AXI_awid(ofdm_mac_0_m_axi_csr_master_AWID),
        .S01_AXI_awlen(ofdm_mac_0_m_axi_csr_master_AWLEN),
        .S01_AXI_awlock(ofdm_mac_0_m_axi_csr_master_AWLOCK[0]),
        .S01_AXI_awprot(ofdm_mac_0_m_axi_csr_master_AWPROT),
        .S01_AXI_awqos(ofdm_mac_0_m_axi_csr_master_AWQOS),
        .S01_AXI_awready(ofdm_mac_0_m_axi_csr_master_AWREADY),
        .S01_AXI_awsize(ofdm_mac_0_m_axi_csr_master_AWSIZE),
        .S01_AXI_awvalid(ofdm_mac_0_m_axi_csr_master_AWVALID),
        .S01_AXI_bid(ofdm_mac_0_m_axi_csr_master_BID),
        .S01_AXI_bready(ofdm_mac_0_m_axi_csr_master_BREADY),
        .S01_AXI_bresp(ofdm_mac_0_m_axi_csr_master_BRESP),
        .S01_AXI_bvalid(ofdm_mac_0_m_axi_csr_master_BVALID),
        .S01_AXI_wdata(ofdm_mac_0_m_axi_csr_master_WDATA),
        .S01_AXI_wlast(ofdm_mac_0_m_axi_csr_master_WLAST),
        .S01_AXI_wready(ofdm_mac_0_m_axi_csr_master_WREADY),
        .S01_AXI_wstrb(ofdm_mac_0_m_axi_csr_master_WSTRB),
        .S01_AXI_wvalid(ofdm_mac_0_m_axi_csr_master_WVALID),
        .aclk(clk),
        .aresetn(rst_n));
  ofdm_chain_fec_cc1_0 fec_cc1
       (.m_axis_aclk(clk_fec),
        .m_axis_aresetn(rst_fec_n),
        .m_axis_tdata(fec_cc1_M_AXIS_TDATA),
        .m_axis_tready(fec_cc1_M_AXIS_TREADY),
        .m_axis_tvalid(fec_cc1_M_AXIS_TVALID),
        .s_axis_aclk(clk),
        .s_axis_aresetn(rst_n),
        .s_axis_tdata(ofdm_rx_0_bits_out_TDATA),
        .s_axis_tready(ofdm_rx_0_bits_out_TREADY),
        .s_axis_tvalid(ofdm_rx_0_bits_out_TVALID));
  ofdm_chain_fec_cc2_0 fec_cc2
       (.m_axis_aclk(clk),
        .m_axis_aresetn(rst_n),
        .m_axis_tdata(fec_cc2_M_AXIS_TDATA),
        .m_axis_tready(fec_cc2_M_AXIS_TREADY),
        .m_axis_tvalid(fec_cc2_M_AXIS_TVALID),
        .s_axis_aclk(clk_fec),
        .s_axis_aresetn(rst_fec_n),
        .s_axis_tdata(fec_rx_0_data_out_TDATA),
        .s_axis_tready(fec_rx_0_data_out_TREADY),
        .s_axis_tvalid(fec_rx_0_data_out_TVALID));
  ofdm_chain_fec_rx_0_0 fec_rx_0
       (.ap_clk(clk_fec),
        .ap_rst_n(rst_fec_n),
        .data_in_TDATA(fec_cc1_M_AXIS_TDATA),
        .data_in_TREADY(fec_cc1_M_AXIS_TREADY),
        .data_in_TVALID(fec_cc1_M_AXIS_TVALID),
        .data_out_TDATA(fec_rx_0_data_out_TDATA),
        .data_out_TREADY(fec_rx_0_data_out_TREADY),
        .data_out_TVALID(fec_rx_0_data_out_TVALID),
        .modcod(ofdm_rx_0_modcod_out),
        .n_syms(ofdm_rx_0_n_syms_out));
  ofdm_chain_fft_cfg_val_0 fft_cfg_val
       (.dout(fft_cfg_val_dout));
  ofdm_chain_hdr_err_const_0 hdr_err_const
       (.dout(hdr_err_const_dout));
  ofdm_chain_ifft_cfg_val_0 ifft_cfg_val
       (.dout(ifft_cfg_val_dout));
  ofdm_chain_ofdm_mac_0_0 ofdm_mac_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .host_rx_out_TDATA(ofdm_mac_0_host_rx_out_TDATA),
        .host_rx_out_TKEEP(ofdm_mac_0_host_rx_out_TKEEP),
        .host_rx_out_TLAST(ofdm_mac_0_host_rx_out_TLAST),
        .host_rx_out_TREADY(ofdm_mac_0_host_rx_out_TREADY),
        .host_rx_out_TSTRB(ofdm_mac_0_host_rx_out_TSTRB),
        .host_rx_out_TVALID(ofdm_mac_0_host_rx_out_TVALID),
        .host_tx_in_TDATA(host_tx_in_tdata),
        .host_tx_in_TKEEP(host_tx_in_tkeep),
        .host_tx_in_TLAST(host_tx_in_tlast),
        .host_tx_in_TREADY(host_tx_in_tready),
        .host_tx_in_TSTRB(host_tx_in_tstrb),
        .host_tx_in_TVALID(host_tx_in_tvalid),
        .m_axi_csr_master_ARREADY(1'b0),
        .m_axi_csr_master_AWADDR(ofdm_mac_0_m_axi_csr_master_AWADDR),
        .m_axi_csr_master_AWBURST(ofdm_mac_0_m_axi_csr_master_AWBURST),
        .m_axi_csr_master_AWCACHE(ofdm_mac_0_m_axi_csr_master_AWCACHE),
        .m_axi_csr_master_AWID(ofdm_mac_0_m_axi_csr_master_AWID),
        .m_axi_csr_master_AWLEN(ofdm_mac_0_m_axi_csr_master_AWLEN),
        .m_axi_csr_master_AWLOCK(ofdm_mac_0_m_axi_csr_master_AWLOCK),
        .m_axi_csr_master_AWPROT(ofdm_mac_0_m_axi_csr_master_AWPROT),
        .m_axi_csr_master_AWQOS(ofdm_mac_0_m_axi_csr_master_AWQOS),
        .m_axi_csr_master_AWREADY(ofdm_mac_0_m_axi_csr_master_AWREADY),
        .m_axi_csr_master_AWSIZE(ofdm_mac_0_m_axi_csr_master_AWSIZE),
        .m_axi_csr_master_AWVALID(ofdm_mac_0_m_axi_csr_master_AWVALID),
        .m_axi_csr_master_BID(ofdm_mac_0_m_axi_csr_master_BID),
        .m_axi_csr_master_BREADY(ofdm_mac_0_m_axi_csr_master_BREADY),
        .m_axi_csr_master_BRESP(ofdm_mac_0_m_axi_csr_master_BRESP),
        .m_axi_csr_master_BVALID(ofdm_mac_0_m_axi_csr_master_BVALID),
        .m_axi_csr_master_RDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .m_axi_csr_master_RID(1'b0),
        .m_axi_csr_master_RLAST(1'b0),
        .m_axi_csr_master_RRESP({1'b0,1'b0}),
        .m_axi_csr_master_RVALID(1'b0),
        .m_axi_csr_master_WDATA(ofdm_mac_0_m_axi_csr_master_WDATA),
        .m_axi_csr_master_WLAST(ofdm_mac_0_m_axi_csr_master_WLAST),
        .m_axi_csr_master_WREADY(ofdm_mac_0_m_axi_csr_master_WREADY),
        .m_axi_csr_master_WSTRB(ofdm_mac_0_m_axi_csr_master_WSTRB),
        .m_axi_csr_master_WVALID(ofdm_mac_0_m_axi_csr_master_WVALID),
        .phy_rx_in_TDATA(fec_cc2_M_AXIS_TDATA),
        .phy_rx_in_TREADY(fec_cc2_M_AXIS_TREADY),
        .phy_rx_in_TVALID(fec_cc2_M_AXIS_TVALID),
        .phy_tx_out_TDATA(ofdm_mac_0_phy_tx_out_TDATA),
        .phy_tx_out_TREADY(ofdm_mac_0_phy_tx_out_TREADY),
        .phy_tx_out_TVALID(ofdm_mac_0_phy_tx_out_TVALID),
        .rx_header_err(hdr_err_const_dout),
        .rx_modcod_in(ofdm_rx_0_modcod_out),
        .rx_n_syms_in(ofdm_rx_0_n_syms_out),
        .rx_pkt_pulse(mac_rx_pkt_pulse),
        .s_axi_control_ARADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_control_ARVALID(1'b0),
        .s_axi_control_AWADDR({1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_control_AWVALID(1'b0),
        .s_axi_control_BREADY(1'b0),
        .s_axi_control_RREADY(1'b0),
        .s_axi_control_WDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0}),
        .s_axi_control_WSTRB({1'b1,1'b1,1'b1,1'b1}),
        .s_axi_control_WVALID(1'b0),
        .s_axi_ctrl_ARADDR(ctrl_xbar_M04_AXI_ARADDR),
        .s_axi_ctrl_ARREADY(ctrl_xbar_M04_AXI_ARREADY),
        .s_axi_ctrl_ARVALID(ctrl_xbar_M04_AXI_ARVALID),
        .s_axi_ctrl_AWADDR(ctrl_xbar_M04_AXI_AWADDR),
        .s_axi_ctrl_AWREADY(ctrl_xbar_M04_AXI_AWREADY),
        .s_axi_ctrl_AWVALID(ctrl_xbar_M04_AXI_AWVALID),
        .s_axi_ctrl_BREADY(ctrl_xbar_M04_AXI_BREADY),
        .s_axi_ctrl_BRESP(ctrl_xbar_M04_AXI_BRESP),
        .s_axi_ctrl_BVALID(ctrl_xbar_M04_AXI_BVALID),
        .s_axi_ctrl_RDATA(ctrl_xbar_M04_AXI_RDATA),
        .s_axi_ctrl_RREADY(ctrl_xbar_M04_AXI_RREADY),
        .s_axi_ctrl_RRESP(ctrl_xbar_M04_AXI_RRESP),
        .s_axi_ctrl_RVALID(ctrl_xbar_M04_AXI_RVALID),
        .s_axi_ctrl_WDATA(ctrl_xbar_M04_AXI_WDATA),
        .s_axi_ctrl_WREADY(ctrl_xbar_M04_AXI_WREADY),
        .s_axi_ctrl_WSTRB(ctrl_xbar_M04_AXI_WSTRB),
        .s_axi_ctrl_WVALID(ctrl_xbar_M04_AXI_WVALID),
        .tx_done_pulse(mac_tx_done_pulse));
  ofdm_chain_ofdm_rx_0_0 ofdm_rx_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .bits_out_TDATA(ofdm_rx_0_bits_out_TDATA),
        .bits_out_TREADY(ofdm_rx_0_bits_out_TREADY),
        .bits_out_TVALID(ofdm_rx_0_bits_out_TVALID),
        .fft_in_TDATA(ofdm_rx_0_fft_in_TDATA),
        .fft_in_TREADY(ofdm_rx_0_fft_in_TREADY),
        .fft_in_TVALID(ofdm_rx_0_fft_in_TVALID),
        .fft_out_TDATA(ofdm_rx_fft_M_AXIS_DATA_TDATA),
        .fft_out_TREADY(ofdm_rx_fft_M_AXIS_DATA_TREADY),
        .fft_out_TVALID(ofdm_rx_fft_M_AXIS_DATA_TVALID),
        .iq_in_TDATA(sync_detect_0_iq_out_TDATA),
        .iq_in_TREADY(sync_detect_0_iq_out_TREADY),
        .iq_in_TVALID(sync_detect_0_iq_out_TVALID),
        .modcod_out(ofdm_rx_0_modcod_out),
        .n_syms_fb(ofdm_rx_0_n_syms_fb),
        .n_syms_out(ofdm_rx_0_n_syms_out),
        .s_axi_stat_ARADDR(ctrl_xbar_M03_AXI_ARADDR),
        .s_axi_stat_ARREADY(ctrl_xbar_M03_AXI_ARREADY),
        .s_axi_stat_ARVALID(ctrl_xbar_M03_AXI_ARVALID),
        .s_axi_stat_AWADDR(ctrl_xbar_M03_AXI_AWADDR),
        .s_axi_stat_AWREADY(ctrl_xbar_M03_AXI_AWREADY),
        .s_axi_stat_AWVALID(ctrl_xbar_M03_AXI_AWVALID),
        .s_axi_stat_BREADY(ctrl_xbar_M03_AXI_BREADY),
        .s_axi_stat_BRESP(ctrl_xbar_M03_AXI_BRESP),
        .s_axi_stat_BVALID(ctrl_xbar_M03_AXI_BVALID),
        .s_axi_stat_RDATA(ctrl_xbar_M03_AXI_RDATA),
        .s_axi_stat_RREADY(ctrl_xbar_M03_AXI_RREADY),
        .s_axi_stat_RRESP(ctrl_xbar_M03_AXI_RRESP),
        .s_axi_stat_RVALID(ctrl_xbar_M03_AXI_RVALID),
        .s_axi_stat_WDATA(ctrl_xbar_M03_AXI_WDATA),
        .s_axi_stat_WREADY(ctrl_xbar_M03_AXI_WREADY),
        .s_axi_stat_WSTRB(ctrl_xbar_M03_AXI_WSTRB),
        .s_axi_stat_WVALID(ctrl_xbar_M03_AXI_WVALID));
  ofdm_chain_ofdm_rx_fft_0 ofdm_rx_fft
       (.aclk(clk),
        .m_axis_data_tdata(ofdm_rx_fft_M_AXIS_DATA_TDATA),
        .m_axis_data_tready(ofdm_rx_fft_M_AXIS_DATA_TREADY),
        .m_axis_data_tvalid(ofdm_rx_fft_M_AXIS_DATA_TVALID),
        .s_axis_config_tdata(fft_cfg_val_dout),
        .s_axis_config_tvalid(cfg_tvalid_dout),
        .s_axis_data_tdata(ofdm_rx_0_fft_in_TDATA),
        .s_axis_data_tlast(1'b0),
        .s_axis_data_tready(ofdm_rx_0_fft_in_TREADY),
        .s_axis_data_tvalid(ofdm_rx_0_fft_in_TVALID));
  ofdm_chain_ofdm_tx_0_0 ofdm_tx_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .bits_in_TDATA(tx_chain_0_data_out_TDATA),
        .bits_in_TREADY(tx_chain_0_data_out_TREADY),
        .bits_in_TVALID(tx_chain_0_data_out_TVALID),
        .ifft_in_TDATA(ofdm_tx_0_ifft_in_TDATA),
        .ifft_in_TREADY(ofdm_tx_0_ifft_in_TREADY),
        .ifft_in_TVALID(ofdm_tx_0_ifft_in_TVALID),
        .ifft_out_TDATA(ofdm_tx_ifft_M_AXIS_DATA_TDATA),
        .ifft_out_TREADY(ofdm_tx_ifft_M_AXIS_DATA_TREADY),
        .ifft_out_TVALID(ofdm_tx_ifft_M_AXIS_DATA_TVALID),
        .iq_out_TDATA(rf_tx_out_tdata),
        .iq_out_TREADY(rf_tx_out_tready),
        .iq_out_TVALID(rf_tx_out_tvalid),
        .s_axi_ctrl_ARADDR(ctrl_xbar_M01_AXI_ARADDR),
        .s_axi_ctrl_ARREADY(ctrl_xbar_M01_AXI_ARREADY),
        .s_axi_ctrl_ARVALID(ctrl_xbar_M01_AXI_ARVALID),
        .s_axi_ctrl_AWADDR(ctrl_xbar_M01_AXI_AWADDR),
        .s_axi_ctrl_AWREADY(ctrl_xbar_M01_AXI_AWREADY),
        .s_axi_ctrl_AWVALID(ctrl_xbar_M01_AXI_AWVALID),
        .s_axi_ctrl_BREADY(ctrl_xbar_M01_AXI_BREADY),
        .s_axi_ctrl_BRESP(ctrl_xbar_M01_AXI_BRESP),
        .s_axi_ctrl_BVALID(ctrl_xbar_M01_AXI_BVALID),
        .s_axi_ctrl_RDATA(ctrl_xbar_M01_AXI_RDATA),
        .s_axi_ctrl_RREADY(ctrl_xbar_M01_AXI_RREADY),
        .s_axi_ctrl_RRESP(ctrl_xbar_M01_AXI_RRESP),
        .s_axi_ctrl_RVALID(ctrl_xbar_M01_AXI_RVALID),
        .s_axi_ctrl_WDATA(ctrl_xbar_M01_AXI_WDATA),
        .s_axi_ctrl_WREADY(ctrl_xbar_M01_AXI_WREADY),
        .s_axi_ctrl_WSTRB(ctrl_xbar_M01_AXI_WSTRB),
        .s_axi_ctrl_WVALID(ctrl_xbar_M01_AXI_WVALID));
  ofdm_chain_ofdm_tx_ifft_0 ofdm_tx_ifft
       (.aclk(clk),
        .m_axis_data_tdata(ofdm_tx_ifft_M_AXIS_DATA_TDATA),
        .m_axis_data_tready(ofdm_tx_ifft_M_AXIS_DATA_TREADY),
        .m_axis_data_tvalid(ofdm_tx_ifft_M_AXIS_DATA_TVALID),
        .s_axis_config_tdata(ifft_cfg_val_dout),
        .s_axis_config_tvalid(cfg_tvalid_dout),
        .s_axis_data_tdata(ofdm_tx_0_ifft_in_TDATA),
        .s_axis_data_tlast(1'b0),
        .s_axis_data_tready(ofdm_tx_0_ifft_in_TREADY),
        .s_axis_data_tvalid(ofdm_tx_0_ifft_in_TVALID));
  ofdm_chain_rx_output_fifo_0 rx_output_fifo
       (.m_axis_tdata(host_rx_out_tdata),
        .m_axis_tkeep(host_rx_out_tkeep),
        .m_axis_tlast(host_rx_out_tlast),
        .m_axis_tready(host_rx_out_tready),
        .m_axis_tstrb(host_rx_out_tstrb),
        .m_axis_tvalid(host_rx_out_tvalid),
        .s_axis_aclk(clk),
        .s_axis_aresetn(rst_n),
        .s_axis_tdata(ofdm_mac_0_host_rx_out_TDATA),
        .s_axis_tkeep(ofdm_mac_0_host_rx_out_TKEEP),
        .s_axis_tlast(ofdm_mac_0_host_rx_out_TLAST),
        .s_axis_tready(ofdm_mac_0_host_rx_out_TREADY),
        .s_axis_tstrb(ofdm_mac_0_host_rx_out_TSTRB),
        .s_axis_tvalid(ofdm_mac_0_host_rx_out_TVALID));
  ofdm_chain_sync_detect_0_0 sync_detect_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .iq_in_TDATA({1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,1'b0,adc_input_fifo_M_AXIS_TDATA}),
        .iq_in_TREADY(adc_input_fifo_M_AXIS_TREADY),
        .iq_in_TVALID(adc_input_fifo_M_AXIS_TVALID),
        .iq_out_TDATA(sync_detect_0_iq_out_TDATA),
        .iq_out_TREADY(sync_detect_0_iq_out_TREADY),
        .iq_out_TVALID(sync_detect_0_iq_out_TVALID),
        .n_syms_fb(ofdm_rx_0_n_syms_fb),
        .n_syms_fb_vld(1'b0),
        .s_axi_stat_ARADDR(ctrl_xbar_M02_AXI_ARADDR),
        .s_axi_stat_ARREADY(ctrl_xbar_M02_AXI_ARREADY),
        .s_axi_stat_ARVALID(ctrl_xbar_M02_AXI_ARVALID),
        .s_axi_stat_AWADDR(ctrl_xbar_M02_AXI_AWADDR),
        .s_axi_stat_AWREADY(ctrl_xbar_M02_AXI_AWREADY),
        .s_axi_stat_AWVALID(ctrl_xbar_M02_AXI_AWVALID),
        .s_axi_stat_BREADY(ctrl_xbar_M02_AXI_BREADY),
        .s_axi_stat_BRESP(ctrl_xbar_M02_AXI_BRESP),
        .s_axi_stat_BVALID(ctrl_xbar_M02_AXI_BVALID),
        .s_axi_stat_RDATA(ctrl_xbar_M02_AXI_RDATA),
        .s_axi_stat_RREADY(ctrl_xbar_M02_AXI_RREADY),
        .s_axi_stat_RRESP(ctrl_xbar_M02_AXI_RRESP),
        .s_axi_stat_RVALID(ctrl_xbar_M02_AXI_RVALID),
        .s_axi_stat_WDATA(ctrl_xbar_M02_AXI_WDATA),
        .s_axi_stat_WREADY(ctrl_xbar_M02_AXI_WREADY),
        .s_axi_stat_WSTRB(ctrl_xbar_M02_AXI_WSTRB),
        .s_axi_stat_WVALID(ctrl_xbar_M02_AXI_WVALID));
  ofdm_chain_tx_chain_0_0 tx_chain_0
       (.ap_clk(clk),
        .ap_rst_n(rst_n),
        .data_in_TDATA(ofdm_mac_0_phy_tx_out_TDATA),
        .data_in_TREADY(ofdm_mac_0_phy_tx_out_TREADY),
        .data_in_TVALID(ofdm_mac_0_phy_tx_out_TVALID),
        .data_out_TDATA(tx_chain_0_data_out_TDATA),
        .data_out_TREADY(tx_chain_0_data_out_TREADY),
        .data_out_TVALID(tx_chain_0_data_out_TVALID),
        .s_axi_ctrl_ARADDR(ctrl_xbar_M00_AXI_ARADDR),
        .s_axi_ctrl_ARREADY(ctrl_xbar_M00_AXI_ARREADY),
        .s_axi_ctrl_ARVALID(ctrl_xbar_M00_AXI_ARVALID),
        .s_axi_ctrl_AWADDR(ctrl_xbar_M00_AXI_AWADDR),
        .s_axi_ctrl_AWREADY(ctrl_xbar_M00_AXI_AWREADY),
        .s_axi_ctrl_AWVALID(ctrl_xbar_M00_AXI_AWVALID),
        .s_axi_ctrl_BREADY(ctrl_xbar_M00_AXI_BREADY),
        .s_axi_ctrl_BRESP(ctrl_xbar_M00_AXI_BRESP),
        .s_axi_ctrl_BVALID(ctrl_xbar_M00_AXI_BVALID),
        .s_axi_ctrl_RDATA(ctrl_xbar_M00_AXI_RDATA),
        .s_axi_ctrl_RREADY(ctrl_xbar_M00_AXI_RREADY),
        .s_axi_ctrl_RRESP(ctrl_xbar_M00_AXI_RRESP),
        .s_axi_ctrl_RVALID(ctrl_xbar_M00_AXI_RVALID),
        .s_axi_ctrl_WDATA(ctrl_xbar_M00_AXI_WDATA),
        .s_axi_ctrl_WREADY(ctrl_xbar_M00_AXI_WREADY),
        .s_axi_ctrl_WSTRB(ctrl_xbar_M00_AXI_WSTRB),
        .s_axi_ctrl_WVALID(ctrl_xbar_M00_AXI_WVALID));
endmodule
