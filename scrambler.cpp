// ============================================================
// scrambler.cpp  —  802.11a LFSR Scrambler / Descrambler
//
// Algorithm
// ─────────
// 7-stage LFSR with polynomial x⁷ + x⁴ + 1:
//   feedback = lfsr[6] XOR lfsr[3]    (taps at positions 7 and 4)
//   output   = input_bit XOR feedback  (XOR scramble)
//   shift    : {lfsr[5:0], feedback}   (shift left, feedback into bit 0)
//
// HLS implementation
// ──────────────────
// Outer loop (SCRAM_LOOP) processes one byte per iteration → II=1.
// Inner loop (BIT_LOOP, 8 iterations) is fully UNROLLED:
//   HLS generates a combinational 8-step LFSR chain (8 × XOR + shift).
//   Critical path: 8 XOR gates in series ≈ 2 ns — well within 5 ns constraint.
//   The resulting outer-loop-carried dependency on `lfsr` (7-bit register)
//   has exactly 1-cycle latency → II=1 is achievable.
//
// Resource estimate: ~20 LUT, 7 FF (LFSR register), 0 DSP, 0 BRAM.
// ============================================================
#include "scrambler.h"

void scrambler(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& data_out,
    ap_uint<16>               n_bytes
) {
    #pragma HLS INTERFACE axis      port=data_in
    #pragma HLS INTERFACE axis      port=data_out
    #pragma HLS INTERFACE s_axilite port=n_bytes bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return  bundle=ctrl

    // Fixed seed: all 7 stages = 1.  Reset at the start of every invocation
    // (every packet).  TX and RX start with the same state → descramble = scramble.
    ap_uint<7> lfsr = 0x7F;

    SCRAM_LOOP: for (int i = 0; i < (int)n_bytes; i++) {
        #pragma HLS PIPELINE II=1
        #pragma HLS loop_tripcount min=1 max=12750

        ap_uint<8> in_byte  = data_in.read();
        ap_uint<8> out_byte = 0;

        // 8 LFSR steps unrolled → combinational chain, no loop-carried stall
        BIT_LOOP: for (int b = 7; b >= 0; b--) {
            #pragma HLS UNROLL
            // Taps: x⁷ = lfsr[6], x⁴ = lfsr[3]
            ap_uint<1> feedback = lfsr[6] ^ lfsr[3];
            out_byte[b] = in_byte[b] ^ feedback;
            // Shift left: drop lfsr[6], insert feedback at bit 0.
            // lfsr<<1 widens to ap_uint<8>; cast back to ap_uint<7> drops bit 7.
            lfsr = (ap_uint<7>)((lfsr << 1) | (ap_uint<7>)feedback);
        }

        data_out.write(out_byte);
    }
}
