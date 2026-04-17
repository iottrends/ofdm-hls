
wire kernel_monitor_reset;
wire kernel_monitor_clock;
wire kernel_monitor_report;
assign kernel_monitor_reset = ~ap_rst_n;
assign kernel_monitor_clock = ap_clk;
assign kernel_monitor_report = 1'b0;
wire [2:0] axis_block_sigs;
wire [8:0] inst_idle_sigs;
wire [3:0] inst_block_sigs;
wire kernel_block;

assign axis_block_sigs[0] = ~scrambler_U0.data_in_TDATA_blk_n;
assign axis_block_sigs[1] = ~interleaver_U0.grp_interleaver_Pipeline_PERM_Q_fu_112.data_out_TDATA_blk_n;
assign axis_block_sigs[2] = ~interleaver_U0.grp_interleaver_Pipeline_PERM_16_fu_119.data_out_TDATA_blk_n;

assign inst_idle_sigs[0] = entry_proc_U0.ap_idle;
assign inst_block_sigs[0] = (entry_proc_U0.ap_done & ~entry_proc_U0.ap_continue) | ~entry_proc_U0.modcod_c1_blk_n | ~entry_proc_U0.n_syms_c_blk_n;
assign inst_idle_sigs[1] = scrambler_U0.ap_idle;
assign inst_block_sigs[1] = (scrambler_U0.ap_done & ~scrambler_U0.ap_continue) | ~scrambler_U0.s1_blk_n | ~scrambler_U0.n_data_bytes_c_blk_n;
assign inst_idle_sigs[2] = conv_enc_U0.ap_idle;
assign inst_block_sigs[2] = (conv_enc_U0.ap_done & ~conv_enc_U0.ap_continue) | ~conv_enc_U0.modcod_blk_n | ~conv_enc_U0.n_data_bytes_blk_n | ~conv_enc_U0.s1_blk_n | ~conv_enc_U0.s2_blk_n | ~conv_enc_U0.modcod_c_blk_n;
assign inst_idle_sigs[3] = interleaver_U0.ap_idle;
assign inst_block_sigs[3] = (interleaver_U0.ap_done & ~interleaver_U0.ap_continue) | ~interleaver_U0.modcod_blk_n | ~interleaver_U0.grp_interleaver_Pipeline_FILL_Q_fu_98.s2_blk_n | ~interleaver_U0.grp_interleaver_Pipeline_FILL_16_fu_105.s2_blk_n | ~interleaver_U0.n_syms_blk_n;

assign inst_idle_sigs[4] = 1'b0;
assign inst_idle_sigs[5] = scrambler_U0.ap_idle;
assign inst_idle_sigs[6] = interleaver_U0.ap_idle;
assign inst_idle_sigs[7] = interleaver_U0.grp_interleaver_Pipeline_PERM_Q_fu_112.ap_idle;
assign inst_idle_sigs[8] = interleaver_U0.grp_interleaver_Pipeline_PERM_16_fu_119.ap_idle;

tx_chain_hls_deadlock_idx0_monitor tx_chain_hls_deadlock_idx0_monitor_U (
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
