// ============================================================
// tx_chain.cpp  —  Merged TX bit-processing chain (DATAFLOW)
//
//   data_in ──► scrambler ──► conv_enc ──► interleaver ──► data_out
//                           s1           s2
//
// All three sub-stages are invoked inside a single #pragma HLS DATAFLOW
// region.  The inter-stage hls::stream FIFOs (s1, s2) let the stages
// run in parallel and pipelined, preserving the throughput of the
// original 3-IP pipeline.
//
// Only the outer block exposes AXIS (data) and s_axilite (control)
// interfaces — the sub-blocks keep their functional bodies but the
// caller sees one IP in Vivado BD.
// ============================================================
#include "tx_chain.h"

void tx_chain(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& data_out,
    ap_uint<16>              n_data_bytes,
    modcod_t                 modcod,
    ap_uint<8>               n_syms
) {
    #pragma HLS INTERFACE axis      port=data_in
    #pragma HLS INTERFACE axis      port=data_out
    #pragma HLS INTERFACE s_axilite port=n_data_bytes bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=modcod       bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=n_syms       bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return       bundle=ctrl

    // Decode modcod → {mod, rate}
    mod_t  mod  = (mod_t)modcod[1];
    rate_t rate = (rate_t)modcod[0];

    #pragma HLS DATAFLOW

    // Internal FIFOs between stages.  Depth 32 is ample: each stage
    // consumes/produces at II=1 so back-pressure is rare; the depth
    // only needs to absorb the interleaver's fill-then-permute
    // phase boundary (≤100 bytes for 16-QAM).
    hls::stream<ap_uint<8>> s1;
    hls::stream<ap_uint<8>> s2;
    #pragma HLS STREAM variable=s1 depth=128
    #pragma HLS STREAM variable=s2 depth=128

    // Stage 1: scramble raw payload bytes
    scrambler(data_in, s1, n_data_bytes);

    // Stage 2: K=7 convolutional encode (+ optional puncture to rate 2/3)
    conv_enc(s1, s2, rate, (int)n_data_bytes);

    // Stage 3: 802.11a two-step interleave (TX direction → is_rx = 0)
    interleaver(s2, data_out, mod, n_syms, /*is_rx=*/0);
}
