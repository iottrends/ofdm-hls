
wire kernel_monitor_reset;
wire kernel_monitor_clock;
wire kernel_monitor_report;
assign kernel_monitor_reset = ~ap_rst_n;
assign kernel_monitor_clock = ap_clk;
assign kernel_monitor_report = 1'b0;
wire [9:0] axis_block_sigs;
wire [17:0] inst_idle_sigs;
wire [0:0] inst_block_sigs;
wire kernel_block;

assign axis_block_sigs[0] = ~grp_send_freq_symbol_fu_424.grp_run_ifft_fu_46.grp_run_ifft_Pipeline_IFFT_TX_fu_38.ifft_in_TDATA_blk_n;
assign axis_block_sigs[1] = ~grp_send_freq_symbol_fu_424.grp_run_ifft_fu_46.grp_run_ifft_Pipeline_IFFT_RX_fu_48.ifft_out_TDATA_blk_n;
assign axis_block_sigs[2] = ~grp_send_freq_symbol_fu_424.grp_insert_cp_and_send_fu_62.grp_insert_cp_and_send_Pipeline_CP_LOOP_fu_42.iq_out_TDATA_blk_n;
assign axis_block_sigs[3] = ~grp_send_freq_symbol_fu_424.grp_insert_cp_and_send_fu_62.grp_insert_cp_and_send_Pipeline_SYM_LOOP_fu_52.iq_out_TDATA_blk_n;
assign axis_block_sigs[4] = ~grp_process_symbol_fu_445.grp_unpack_bits_fu_92.grp_unpack_bits_Pipeline_UNPACK_QPSK_fu_32.bits_in_TDATA_blk_n;
assign axis_block_sigs[5] = ~grp_process_symbol_fu_445.grp_unpack_bits_fu_92.grp_unpack_bits_Pipeline_UNPACK_16QAM_fu_40.bits_in_TDATA_blk_n;
assign axis_block_sigs[6] = ~grp_process_symbol_fu_445.grp_run_ifft_fu_111.grp_run_ifft_Pipeline_IFFT_TX_fu_38.ifft_in_TDATA_blk_n;
assign axis_block_sigs[7] = ~grp_process_symbol_fu_445.grp_run_ifft_fu_111.grp_run_ifft_Pipeline_IFFT_RX_fu_48.ifft_out_TDATA_blk_n;
assign axis_block_sigs[8] = ~grp_process_symbol_fu_445.grp_insert_cp_and_send_fu_123.grp_insert_cp_and_send_Pipeline_CP_LOOP_fu_42.iq_out_TDATA_blk_n;
assign axis_block_sigs[9] = ~grp_process_symbol_fu_445.grp_insert_cp_and_send_fu_123.grp_insert_cp_and_send_Pipeline_SYM_LOOP_fu_52.iq_out_TDATA_blk_n;

assign inst_block_sigs[0] = 1'b0;

assign inst_idle_sigs[0] = 1'b0;
assign inst_idle_sigs[1] = grp_send_freq_symbol_fu_424.ap_idle;
assign inst_idle_sigs[2] = grp_send_freq_symbol_fu_424.grp_run_ifft_fu_46.ap_idle;
assign inst_idle_sigs[3] = grp_send_freq_symbol_fu_424.grp_run_ifft_fu_46.grp_run_ifft_Pipeline_IFFT_TX_fu_38.ap_idle;
assign inst_idle_sigs[4] = grp_send_freq_symbol_fu_424.grp_run_ifft_fu_46.grp_run_ifft_Pipeline_IFFT_RX_fu_48.ap_idle;
assign inst_idle_sigs[5] = grp_send_freq_symbol_fu_424.grp_insert_cp_and_send_fu_62.ap_idle;
assign inst_idle_sigs[6] = grp_send_freq_symbol_fu_424.grp_insert_cp_and_send_fu_62.grp_insert_cp_and_send_Pipeline_CP_LOOP_fu_42.ap_idle;
assign inst_idle_sigs[7] = grp_send_freq_symbol_fu_424.grp_insert_cp_and_send_fu_62.grp_insert_cp_and_send_Pipeline_SYM_LOOP_fu_52.ap_idle;
assign inst_idle_sigs[8] = grp_process_symbol_fu_445.ap_idle;
assign inst_idle_sigs[9] = grp_process_symbol_fu_445.grp_unpack_bits_fu_92.ap_idle;
assign inst_idle_sigs[10] = grp_process_symbol_fu_445.grp_unpack_bits_fu_92.grp_unpack_bits_Pipeline_UNPACK_QPSK_fu_32.ap_idle;
assign inst_idle_sigs[11] = grp_process_symbol_fu_445.grp_unpack_bits_fu_92.grp_unpack_bits_Pipeline_UNPACK_16QAM_fu_40.ap_idle;
assign inst_idle_sigs[12] = grp_process_symbol_fu_445.grp_run_ifft_fu_111.ap_idle;
assign inst_idle_sigs[13] = grp_process_symbol_fu_445.grp_run_ifft_fu_111.grp_run_ifft_Pipeline_IFFT_TX_fu_38.ap_idle;
assign inst_idle_sigs[14] = grp_process_symbol_fu_445.grp_run_ifft_fu_111.grp_run_ifft_Pipeline_IFFT_RX_fu_48.ap_idle;
assign inst_idle_sigs[15] = grp_process_symbol_fu_445.grp_insert_cp_and_send_fu_123.ap_idle;
assign inst_idle_sigs[16] = grp_process_symbol_fu_445.grp_insert_cp_and_send_fu_123.grp_insert_cp_and_send_Pipeline_CP_LOOP_fu_42.ap_idle;
assign inst_idle_sigs[17] = grp_process_symbol_fu_445.grp_insert_cp_and_send_fu_123.grp_insert_cp_and_send_Pipeline_SYM_LOOP_fu_52.ap_idle;

ofdm_tx_hls_deadlock_idx0_monitor ofdm_tx_hls_deadlock_idx0_monitor_U (
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
