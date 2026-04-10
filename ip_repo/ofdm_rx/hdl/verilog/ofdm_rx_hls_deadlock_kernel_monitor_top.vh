
wire kernel_monitor_reset;
wire kernel_monitor_clock;
wire kernel_monitor_report;
assign kernel_monitor_reset = ~ap_rst_n;
assign kernel_monitor_clock = ap_clk;
assign kernel_monitor_report = 1'b0;
wire [6:0] axis_block_sigs;
wire [10:0] inst_idle_sigs;
wire [0:0] inst_block_sigs;
wire kernel_block;

assign axis_block_sigs[0] = ~grp_remove_cp_and_read_fu_356.grp_remove_cp_and_read_Pipeline_SKIP_CP_fu_22.iq_in_TDATA_blk_n;
assign axis_block_sigs[1] = ~grp_remove_cp_and_read_fu_356.grp_remove_cp_and_read_Pipeline_READ_SYM_fu_28.iq_in_TDATA_blk_n;
assign axis_block_sigs[2] = ~grp_run_fft_fu_364.grp_run_fft_Pipeline_FFT_TX_fu_28.fft_in_TDATA_blk_n;
assign axis_block_sigs[3] = ~grp_run_fft_fu_364.grp_run_fft_Pipeline_FFT_RX_fu_38.fft_out_TDATA_blk_n;
assign axis_block_sigs[4] = ~grp_ofdm_rx_Pipeline_DRAIN_fu_440.iq_in_TDATA_blk_n;
assign axis_block_sigs[5] = ~grp_equalize_demap_pack_fu_446.grp_equalize_demap_pack_Pipeline_QPSK_PACK_fu_56.bits_out_TDATA_blk_n;
assign axis_block_sigs[6] = ~grp_equalize_demap_pack_fu_446.grp_equalize_demap_pack_Pipeline_QAM16_PACK_fu_76.bits_out_TDATA_blk_n;

assign inst_block_sigs[0] = 1'b0;

assign inst_idle_sigs[0] = 1'b0;
assign inst_idle_sigs[1] = grp_remove_cp_and_read_fu_356.ap_idle;
assign inst_idle_sigs[2] = grp_remove_cp_and_read_fu_356.grp_remove_cp_and_read_Pipeline_SKIP_CP_fu_22.ap_idle;
assign inst_idle_sigs[3] = grp_remove_cp_and_read_fu_356.grp_remove_cp_and_read_Pipeline_READ_SYM_fu_28.ap_idle;
assign inst_idle_sigs[4] = grp_run_fft_fu_364.ap_idle;
assign inst_idle_sigs[5] = grp_run_fft_fu_364.grp_run_fft_Pipeline_FFT_TX_fu_28.ap_idle;
assign inst_idle_sigs[6] = grp_run_fft_fu_364.grp_run_fft_Pipeline_FFT_RX_fu_38.ap_idle;
assign inst_idle_sigs[7] = grp_ofdm_rx_Pipeline_DRAIN_fu_440.ap_idle;
assign inst_idle_sigs[8] = grp_equalize_demap_pack_fu_446.ap_idle;
assign inst_idle_sigs[9] = grp_equalize_demap_pack_fu_446.grp_equalize_demap_pack_Pipeline_QPSK_PACK_fu_56.ap_idle;
assign inst_idle_sigs[10] = grp_equalize_demap_pack_fu_446.grp_equalize_demap_pack_Pipeline_QAM16_PACK_fu_76.ap_idle;

ofdm_rx_hls_deadlock_idx0_monitor ofdm_rx_hls_deadlock_idx0_monitor_U (
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
