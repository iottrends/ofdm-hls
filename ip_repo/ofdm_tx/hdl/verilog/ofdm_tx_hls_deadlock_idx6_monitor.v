`timescale 1 ns / 1 ps

module ofdm_tx_hls_deadlock_idx6_monitor ( // for module ofdm_tx_ofdm_tx_inst.grp_send_freq_symbol_fu_434.grp_insert_cp_and_send_fu_62
    input wire clock,
    input wire reset,
    input wire [10:0] axis_block_sigs,
    input wire [18:0] inst_idle_sigs,
    input wire [0:0] inst_block_sigs,
    output wire block
);

// signal declare
reg monitor_find_block;
wire idx7_block;
wire idx8_block;
wire sub_parallel_block;
wire all_sub_parallel_has_block;
wire all_sub_single_has_block;
wire cur_axis_has_block;
wire seq_is_axis_block;

assign block = monitor_find_block;
assign idx7_block = axis_block_sigs[3];
assign idx8_block = axis_block_sigs[4];
assign all_sub_parallel_has_block = 1'b0;
assign all_sub_single_has_block = 1'b0 | (idx7_block & (axis_block_sigs[3])) | (idx8_block & (axis_block_sigs[4]));
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
endmodule
