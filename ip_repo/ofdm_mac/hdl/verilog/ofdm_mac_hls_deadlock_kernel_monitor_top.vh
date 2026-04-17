
wire kernel_monitor_reset;
wire kernel_monitor_clock;
wire kernel_monitor_report;
assign kernel_monitor_reset = ~ap_rst_n;
assign kernel_monitor_clock = ap_clk;
assign kernel_monitor_report = 1'b0;
wire [9:0] axis_block_sigs;
wire [11:0] inst_idle_sigs;
wire [0:0] inst_block_sigs;
wire kernel_block;

assign axis_block_sigs[0] = ~grp_do_rx_fu_312.grp_do_rx_Pipeline_HDR_RD_fu_322.phy_rx_in_TDATA_blk_n;
assign axis_block_sigs[1] = ~grp_do_rx_fu_312.grp_do_rx_Pipeline_PAY_fu_335.phy_rx_in_TDATA_blk_n;
assign axis_block_sigs[2] = ~grp_do_rx_fu_312.grp_do_rx_Pipeline_PAY_fu_335.host_rx_out_TDATA_blk_n;
assign axis_block_sigs[3] = ~grp_do_rx_fu_312.grp_do_rx_Pipeline_FCS_RD_fu_354.phy_rx_in_TDATA_blk_n;
assign axis_block_sigs[4] = ~grp_do_tx_fu_343.grp_do_tx_Pipeline_ABSORB_fu_228.host_tx_in_TDATA_blk_n;
assign axis_block_sigs[5] = ~grp_do_tx_fu_343.grp_do_tx_Pipeline_HDR_DST_fu_245.phy_tx_out_TDATA_blk_n;
assign axis_block_sigs[6] = ~grp_do_tx_fu_343.grp_do_tx_Pipeline_HDR_SRC_fu_251.phy_tx_out_TDATA_blk_n;
assign axis_block_sigs[7] = ~grp_do_tx_fu_343.grp_do_tx_Pipeline_HDR_LEN_fu_260.phy_tx_out_TDATA_blk_n;
assign axis_block_sigs[8] = ~grp_do_tx_fu_343.grp_do_tx_Pipeline_PAY_fu_269.phy_tx_out_TDATA_blk_n;
assign axis_block_sigs[9] = ~grp_do_tx_fu_343.grp_do_tx_Pipeline_FCS_TX_fu_280.phy_tx_out_TDATA_blk_n;

assign inst_block_sigs[0] = 1'b0;

assign inst_idle_sigs[0] = 1'b0;
assign inst_idle_sigs[1] = grp_do_rx_fu_312.ap_idle;
assign inst_idle_sigs[2] = grp_do_rx_fu_312.grp_do_rx_Pipeline_HDR_RD_fu_322.ap_idle;
assign inst_idle_sigs[3] = grp_do_rx_fu_312.grp_do_rx_Pipeline_PAY_fu_335.ap_idle;
assign inst_idle_sigs[4] = grp_do_rx_fu_312.grp_do_rx_Pipeline_FCS_RD_fu_354.ap_idle;
assign inst_idle_sigs[5] = grp_do_tx_fu_343.ap_idle;
assign inst_idle_sigs[6] = grp_do_tx_fu_343.grp_do_tx_Pipeline_ABSORB_fu_228.ap_idle;
assign inst_idle_sigs[7] = grp_do_tx_fu_343.grp_do_tx_Pipeline_HDR_DST_fu_245.ap_idle;
assign inst_idle_sigs[8] = grp_do_tx_fu_343.grp_do_tx_Pipeline_HDR_SRC_fu_251.ap_idle;
assign inst_idle_sigs[9] = grp_do_tx_fu_343.grp_do_tx_Pipeline_HDR_LEN_fu_260.ap_idle;
assign inst_idle_sigs[10] = grp_do_tx_fu_343.grp_do_tx_Pipeline_PAY_fu_269.ap_idle;
assign inst_idle_sigs[11] = grp_do_tx_fu_343.grp_do_tx_Pipeline_FCS_TX_fu_280.ap_idle;

ofdm_mac_hls_deadlock_idx0_monitor ofdm_mac_hls_deadlock_idx0_monitor_U (
    .clock(kernel_monitor_clock),
    .reset(kernel_monitor_reset),
    .axis_block_sigs(axis_block_sigs),
    .inst_idle_sigs(inst_idle_sigs),
    .inst_block_sigs(inst_block_sigs),
    .block(kernel_block)
);


always @ (kernel_block or kernel_monitor_reset) begin
    if (kernel_block == 1'b1 && kernel_monitor_reset == 1'b0) begin
        find_kernel_block = 1'b1;
    end
    else begin
        find_kernel_block = 1'b0;
    end
end
