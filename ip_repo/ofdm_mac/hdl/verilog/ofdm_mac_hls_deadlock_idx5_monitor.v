`timescale 1 ns / 1 ps

module ofdm_mac_hls_deadlock_idx5_monitor ( // for module ofdm_mac_ofdm_mac_inst.grp_do_tx_fu_343
    input wire clock,
    input wire reset,
    input wire [9:0] axis_block_sigs,
    input wire [11:0] inst_idle_sigs,
    input wire [0:0] inst_block_sigs,
    output wire block
);

// signal declare
reg monitor_find_block;
wire idx6_block;
wire idx7_block;
wire idx8_block;
wire idx9_block;
wire idx10_block;
wire idx11_block;
wire sub_parallel_block;
wire all_sub_parallel_has_block;
wire all_sub_single_has_block;
wire cur_axis_has_block;
wire seq_is_axis_block;

assign block = monitor_find_block;
assign idx6_block = axis_block_sigs[4];
assign idx7_block = axis_block_sigs[5];
assign idx8_block = axis_block_sigs[6];
assign idx9_block = axis_block_sigs[7];
assign idx10_block = axis_block_sigs[8];
assign idx11_block = axis_block_sigs[9];
assign sub_parallel_block = 1'b0 | ((idx6_block & (axis_block_sigs[4])) & ((idx7_block | inst_idle_sigs[7]))) | ((idx7_block & (axis_block_sigs[5])) & ((idx6_block | inst_idle_sigs[6])));
assign all_sub_parallel_has_block = sub_parallel_block;
assign all_sub_single_has_block = 1'b0 | (idx8_block & (axis_block_sigs[6])) | (idx9_block & (axis_block_sigs[7])) | (idx10_block & (axis_block_sigs[8])) | (idx11_block & (axis_block_sigs[9]));
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
