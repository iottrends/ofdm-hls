// ============================================================
// conv_enc.cpp  —  K=7 Convolutional Encoder with Puncturer
//
// State convention:
//   sr[5] = most recently input bit (newest)
//   sr[0] = bit input 6 steps ago   (oldest)
//   After input bit b: new_sr = (b << 5) | (old_sr >> 1)
//
// Encoder outputs for input b entering state sr:
//   7-bit sequence: [b, sr[5], sr[4], sr[3], sr[2], sr[1], sr[0]]
//   G0 = b ^ sr[4] ^ sr[3] ^ sr[1] ^ sr[0]  (polynomial positions 6,4,3,1,0)
//   G1 = b ^ sr[5] ^ sr[4] ^ sr[3] ^ sr[0]  (polynomial positions 6,5,4,3,0)
//
// Rate 2/3 puncture (802.11a [[1,1],[1,0]]):
//   Input bit 0 (even): emit G0, G1
//   Input bit 1 (odd) : emit G0 only  (G1 dropped)
//   → 3 coded bits per 2 data bits = rate 2/3
// ============================================================
#include "conv_fec.h"

// Compute G0, G1 outputs and advance shift register
static void encode_bit(ap_uint<1> b, ap_uint<6>& sr,
                        ap_uint<1>& g0, ap_uint<1>& g1)
{
    g0 = b ^ sr[4] ^ sr[3] ^ sr[1] ^ sr[0];
    g1 = b ^ sr[5] ^ sr[4] ^ sr[3] ^ sr[0];
    // Cast b to 6 bits BEFORE shifting — ap_uint<1> << 5 gives 0 (1-bit truncation)
    sr = (ap_uint<6>)(((ap_uint<6>)b << 5) | (sr >> 1));
}

void conv_enc(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& coded_out,
    rate_t rate,
    int    n_data_bytes)
{
#pragma HLS INTERFACE axis      port=data_in
#pragma HLS INTERFACE axis      port=coded_out
#pragma HLS INTERFACE s_axilite port=rate         bundle=ctrl
#pragma HLS INTERFACE s_axilite port=n_data_bytes bundle=ctrl
#pragma HLS INTERFACE s_axilite port=return       bundle=ctrl
    ap_uint<6> sr = 0;        // shift register, initialised to all-zeros

    // Output bit accumulator
    ap_uint<8> out_byte  = 0;
    int        out_pos   = 7; // next bit position (7=MSB, counts down)

    // Emit one coded bit into the output byte stream
    // (local inline — lambdas not portable in all HLS versions)
    #define EMIT(bit) \
        do { \
            out_byte[out_pos] = (bit); \
            if (out_pos == 0) { coded_out.write(out_byte); out_byte = 0; out_pos = 7; } \
            else { out_pos--; } \
        } while(0)

    int global_bit = 0; // counts input bits (used for puncture pattern index)

    ENC_BYTES: for (int byte_idx = 0; byte_idx < n_data_bytes; byte_idx++) {
        ap_uint<8> in_byte = data_in.read();

        ENC_BITS: for (int bit_pos = 7; bit_pos >= 0; bit_pos--) {
            ap_uint<1> b = (in_byte >> bit_pos) & 1;  // MSB first
            ap_uint<1> g0, g1;
            encode_bit(b, sr, g0, g1);

            EMIT(g0);
            // Rate 1/2: always emit G1
            // Rate 2/3: emit G1 only on even input bits (global_bit % 2 == 0)
            if (rate == 0 || (global_bit & 1) == 0) {
                EMIT(g1);
            }
            global_bit++;
        }
    }

    #undef EMIT
}
