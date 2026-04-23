-- Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
-- Copyright 2022-2026 Advanced Micro Devices, Inc. All Rights Reserved.
-- -------------------------------------------------------------------------------
-- This file contains confidential and proprietary information
-- of AMD and is protected under U.S. and international copyright
-- and other intellectual property laws.
--
-- DISCLAIMER
-- This disclaimer is not a license and does not grant any
-- rights to the materials distributed herewith. Except as
-- otherwise provided in a valid license issued to you by
-- AMD, and to the maximum extent permitted by applicable
-- law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
-- WITH ALL FAULTS, AND AMD HEREBY DISCLAIMS ALL WARRANTIES
-- AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
-- BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
-- INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
-- (2) AMD shall not be liable (whether in contract or tort,
-- including negligence, or under any other theory of
-- liability) for any loss or damage of any kind or nature
-- related to, arising under or in connection with these
-- materials, including for any direct, or any indirect,
-- special, incidental, or consequential loss or damage
-- (including loss of data, profits, goodwill, or any type of
-- loss or damage suffered as a result of any action brought
-- by a third party) even if such damage or loss was
-- reasonably foreseeable or AMD had been advised of the
-- possibility of the same.
--
-- CRITICAL APPLICATIONS
-- AMD products are not designed or intended to be fail-
-- safe, or for use in any application requiring fail-safe
-- performance, such as life-support or safety devices or
-- systems, Class III medical devices, nuclear facilities,
-- applications related to the deployment of airbags, or any
-- other applications that could lead to death, personal
-- injury, or severe property or environmental damage
-- (individually and collectively, "Critical
-- Applications"). Customer assumes the sole risk and
-- liability of any use of AMD products in Critical
-- Applications, subject only to applicable laws and
-- regulations governing limitations on product liability.
--
-- THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
-- PART OF THIS FILE AT ALL TIMES.
--
-- DO NOT MODIFY THIS FILE.

-- MODULE VLNV: amd.com:blockdesign:ofdm_chain:1.0

-- The following code must appear in the VHDL architecture header.

-- COMP_TAG     ------ Begin cut for COMPONENT Declaration ------
COMPONENT ofdm_chain
  PORT (
    clk : IN STD_LOGIC;
    clk_fec : IN STD_LOGIC;
    rst_n : IN STD_LOGIC;
    rst_fec_n : IN STD_LOGIC;
    host_tx_in_tdata : IN STD_LOGIC_VECTOR(7 DOWNTO 0);
    host_tx_in_tkeep : IN STD_LOGIC_VECTOR(0 DOWNTO 0);
    host_tx_in_tlast : IN STD_LOGIC_VECTOR(0 DOWNTO 0);
    host_tx_in_tready : OUT STD_LOGIC;
    host_tx_in_tstrb : IN STD_LOGIC_VECTOR(0 DOWNTO 0);
    host_tx_in_tvalid : IN STD_LOGIC;
    rf_tx_out_tdata : OUT STD_LOGIC_VECTOR(47 DOWNTO 0);
    rf_tx_out_tready : IN STD_LOGIC;
    rf_tx_out_tvalid : OUT STD_LOGIC;
    rf_rx_in_tdata : IN STD_LOGIC_VECTOR(39 DOWNTO 0);
    rf_rx_in_tready : OUT STD_LOGIC;
    rf_rx_in_tvalid : IN STD_LOGIC;
    host_rx_out_tdata : OUT STD_LOGIC_VECTOR(7 DOWNTO 0);
    host_rx_out_tready : IN STD_LOGIC;
    host_rx_out_tvalid : OUT STD_LOGIC;
    ctrl_axi_awaddr : IN STD_LOGIC_VECTOR(15 DOWNTO 0);
    ctrl_axi_awprot : IN STD_LOGIC_VECTOR(2 DOWNTO 0);
    ctrl_axi_awvalid : IN STD_LOGIC;
    ctrl_axi_awready : OUT STD_LOGIC;
    ctrl_axi_wdata : IN STD_LOGIC_VECTOR(31 DOWNTO 0);
    ctrl_axi_wstrb : IN STD_LOGIC_VECTOR(3 DOWNTO 0);
    ctrl_axi_wvalid : IN STD_LOGIC;
    ctrl_axi_wready : OUT STD_LOGIC;
    ctrl_axi_bresp : OUT STD_LOGIC_VECTOR(1 DOWNTO 0);
    ctrl_axi_bvalid : OUT STD_LOGIC;
    ctrl_axi_bready : IN STD_LOGIC;
    ctrl_axi_araddr : IN STD_LOGIC_VECTOR(15 DOWNTO 0);
    ctrl_axi_arprot : IN STD_LOGIC_VECTOR(2 DOWNTO 0);
    ctrl_axi_arvalid : IN STD_LOGIC;
    ctrl_axi_arready : OUT STD_LOGIC;
    ctrl_axi_rdata : OUT STD_LOGIC_VECTOR(31 DOWNTO 0);
    ctrl_axi_rresp : OUT STD_LOGIC_VECTOR(1 DOWNTO 0);
    ctrl_axi_rvalid : OUT STD_LOGIC;
    ctrl_axi_rready : IN STD_LOGIC;
    mac_tx_done_pulse : OUT STD_LOGIC_VECTOR(0 DOWNTO 0);
    mac_rx_pkt_pulse : OUT STD_LOGIC_VECTOR(0 DOWNTO 0);
    host_rx_out_tkeep : OUT STD_LOGIC_VECTOR(0 DOWNTO 0);
    host_rx_out_tlast : OUT STD_LOGIC;
    host_rx_out_tstrb : OUT STD_LOGIC_VECTOR(0 DOWNTO 0)
  );
END COMPONENT;
-- COMP_TAG_END ------  End cut for COMPONENT Declaration  ------

-- The following code must appear in the VHDL architecture
-- body. Substitute your own instance name and net names.

-- INST_TAG     ------ Begin cut for INSTANTIATION Template ------
your_instance_name : ofdm_chain
  PORT MAP (
    clk => clk,
    clk_fec => clk_fec,
    rst_n => rst_n,
    rst_fec_n => rst_fec_n,
    host_tx_in_tdata => host_tx_in_tdata,
    host_tx_in_tkeep => host_tx_in_tkeep,
    host_tx_in_tlast => host_tx_in_tlast,
    host_tx_in_tready => host_tx_in_tready,
    host_tx_in_tstrb => host_tx_in_tstrb,
    host_tx_in_tvalid => host_tx_in_tvalid,
    rf_tx_out_tdata => rf_tx_out_tdata,
    rf_tx_out_tready => rf_tx_out_tready,
    rf_tx_out_tvalid => rf_tx_out_tvalid,
    rf_rx_in_tdata => rf_rx_in_tdata,
    rf_rx_in_tready => rf_rx_in_tready,
    rf_rx_in_tvalid => rf_rx_in_tvalid,
    host_rx_out_tdata => host_rx_out_tdata,
    host_rx_out_tready => host_rx_out_tready,
    host_rx_out_tvalid => host_rx_out_tvalid,
    ctrl_axi_awaddr => ctrl_axi_awaddr,
    ctrl_axi_awprot => ctrl_axi_awprot,
    ctrl_axi_awvalid => ctrl_axi_awvalid,
    ctrl_axi_awready => ctrl_axi_awready,
    ctrl_axi_wdata => ctrl_axi_wdata,
    ctrl_axi_wstrb => ctrl_axi_wstrb,
    ctrl_axi_wvalid => ctrl_axi_wvalid,
    ctrl_axi_wready => ctrl_axi_wready,
    ctrl_axi_bresp => ctrl_axi_bresp,
    ctrl_axi_bvalid => ctrl_axi_bvalid,
    ctrl_axi_bready => ctrl_axi_bready,
    ctrl_axi_araddr => ctrl_axi_araddr,
    ctrl_axi_arprot => ctrl_axi_arprot,
    ctrl_axi_arvalid => ctrl_axi_arvalid,
    ctrl_axi_arready => ctrl_axi_arready,
    ctrl_axi_rdata => ctrl_axi_rdata,
    ctrl_axi_rresp => ctrl_axi_rresp,
    ctrl_axi_rvalid => ctrl_axi_rvalid,
    ctrl_axi_rready => ctrl_axi_rready,
    mac_tx_done_pulse => mac_tx_done_pulse,
    mac_rx_pkt_pulse => mac_rx_pkt_pulse,
    host_rx_out_tkeep => host_rx_out_tkeep,
    host_rx_out_tlast => host_rx_out_tlast,
    host_rx_out_tstrb => host_rx_out_tstrb
  );
-- INST_TAG_END ------  End cut for INSTANTIATION Template  ------

-- You must compile the wrapper file ofdm_chain.vhd when simulating
-- the module, ofdm_chain. When compiling the wrapper file, be sure to
-- reference the VHDL simulation library.
