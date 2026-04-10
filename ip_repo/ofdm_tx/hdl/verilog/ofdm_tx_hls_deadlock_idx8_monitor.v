`timescale 1 ns / 1 ps

module ofdm_tx_hls_deadlock_idx8_monitor ( // for module ofdm_tx_ofdm_tx_inst.grp_process_symbol_fu_445
    input wire clock,
    input wire reset,
    input wire [9:0] axis_block_sigs,
    input wire [17:0] inst_idle_sigs,
    input wire [0:0] inst_block_sigs,
    output wire block
);

// signal declare
reg monitor_find_block;
wire idx9_block;
wire idx15_block;
wire idx12_block;
wire sub_parallel_block;
wire all_sub_parallel_has_block;
wire all_sub_single_has_block;
wire cur_axis_has_block;
wire seq_is_axis_block;

assign block = monitor_find_block;
assign all_sub_parallel_has_block = 1'b0;
assign all_sub_single_has_block = 1'b0 | (idx9_block & (axis_block_sigs[4] | axis_block_sigs[5])) | (idx15_block & (axis_block_sigs[8] | axis_block_sigs[9])) | (idx12_block & (axis_block_sigs[6] | axis_block_sigs[7]));
assign cur_axis_has_block = 1'b0;
assign seq_is_axis_block = all_sub_parallel_has_block | all_sub_single_has_block | cur_axis_has_block;

always @(posedge clock) begin
    if (reset == 1'b1)
        monitor_find_block <= 1'b0;
    else if (seq_is_axis_block == 1'b1)
        monitor_find_block <= 1'b1;
    else
        monitor_find_block <= 1'b0;
end


// instant sub module
 ofdm_tx_hls_deadlock_idx9_monitor ofdm_tx_hls_deadlock_idx9_monitor_U (
    .clock(clock),
    .reset(reset),
    .axis_block_sigs(axis_block_sigs),
    .inst_idle_sigs(inst_idle_sigs),
    .inst_block_sigs(inst_block_sigs),
    .block(idx9_block)
);

 ofdm_tx_hls_deadlock_idx15_monitor ofdm_tx_hls_deadlock_idx15_monitor_U (
    .clock(clock),
    .reset(reset),
    .axis_block_sigs(axis_block_sigs),
    .inst_idle_sigs(inst_idle_sigs),
    .inst_block_sigs(inst_block_sigs),
    .block(idx15_block)
);

 ofdm_tx_hls_deadlock_idx12_monitor ofdm_tx_hls_deadlock_idx12_monitor_U (
    .clock(clock),
    .reset(reset),
    .axis_block_sigs(axis_block_sigs),
    .inst_idle_sigs(inst_idle_sigs),
    .inst_block_sigs(inst_block_sigs),
    .block(idx12_block)
);

endmodule
