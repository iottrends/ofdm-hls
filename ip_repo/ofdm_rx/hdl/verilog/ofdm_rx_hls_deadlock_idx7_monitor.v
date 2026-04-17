`timescale 1 ns / 1 ps

module ofdm_rx_hls_deadlock_idx7_monitor ( // for module ofdm_rx_ofdm_rx_inst.grp_equalize_demap_pack_fu_547
    input wire clock,
    input wire reset,
    input wire [5:0] axis_block_sigs,
    input wire [9:0] inst_idle_sigs,
    input wire [0:0] inst_block_sigs,
    output wire block
);

// signal declare
reg monitor_find_block;
wire idx8_block;
wire idx9_block;
wire sub_parallel_block;
wire all_sub_parallel_has_block;
wire all_sub_single_has_block;
wire cur_axis_has_block;
wire seq_is_axis_block;

assign block = monitor_find_block;
assign idx8_block = axis_block_sigs[4];
assign idx9_block = axis_block_sigs[5];
assign sub_parallel_block = 1'b0 | ((idx8_block & (axis_block_sigs[4])) & ((idx9_block | inst_idle_sigs[9]))) | ((idx9_block & (axis_block_sigs[5])) & ((idx8_block | inst_idle_sigs[8])));
assign all_sub_parallel_has_block = sub_parallel_block;
assign all_sub_single_has_block = 1'b0;
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
