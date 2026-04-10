// ============================================================
// interleaver.cpp  —  802.11a Two-Step Bit Interleaver / Deinterleaver
//
// Structure per symbol
// ────────────────────
// Phase 1 (FILL):    read NCBPS/8 bytes from stream into LUTRAM buffer.
// Phase 2 (PERM):    for j = 0..NCBPS-1, look up source bit index src[j],
//                    read that bit from the buffer, and pack into output bytes.
//
// Source index computation — no precomputed ROM needed
// ─────────────────────────────────────────────────────
// For QPSK (NCBPS=400, M=25, s=1):
//   Step 2 is identity (s=1 → mod 1 = 0 always).
//   TX (π⁻¹): src[j] = 16×(j mod 25) + ⌊j/25⌋
//   RX (π)  : src[j] = 25×(j mod 16) + ⌊j/16⌋  =  25×(j&15) + (j>>4)
//
// For 16-QAM (NCBPS=800, M=50, s=2):
//   Step 2 swaps adjacent pairs (k, k+1) when column c = ⌊k/50⌋ is odd
//   and r = k mod 50 is even (r↔r+1 within the column).  This is self-inverse.
//
//   TX (π⁻¹): apply π₂⁻¹ = π₂ first (pair-swap), then π₁⁻¹:
//     c = j/50,  r = j%50
//     k_r = (c odd) ? (r even ? r+1 : r-1) : r
//     src[j] = 16 × k_r + c          (k_div50 = c always)
//
//   RX (π):   apply π₁ first, then π₂:
//     c = j & 15  (j mod 16),  r = j >> 4  (j / 16)
//     k = 50×c + r
//     src[j] = (c odd) ? (r even ? k+1 : k-1) : k
//
// Verification (spot-check, 16-QAM):
//   TX src[50]: c=1(odd),r=0(even) → k_r=1,c=1 → i=16×1+1=17
//   Check: π(17): k=50×(17%16)+17//16=50×1+1=51, π₂(51):c=1(odd),r=1(odd)→50 ≠ 50? Wait...
//   Actually π(17): k=50×1+1=51, π₂(51):c=51//50=1(odd),r=51%50=1(odd)→k-1=50. ✓
//   TX src[51]: c=1(odd),r=1(odd) → k_r=0,c=1 → i=16×0+1=1
//   Check: π(1): k=50×1+0=50, π₂(50):c=1(odd),r=0(even)→k+1=51. ✓
//
// HLS notes
// ──────────
// Separate if/else branches for QPSK and 16-QAM give each inner loop
// fixed trip counts → PIPELINE II=1 achievable.
// LUTRAM buffer (max 100 bytes): single-port, fills then reads, no conflict.
// Division by 25 / 50 in src computation: HLS optimises to multiply-shift
// (reciprocal approximation) → 1-cycle combinational, no II impact.
// Out-byte shift-accumulate: 1-cycle loop-carried dependency → II=1 achievable.
// ============================================================
#include "interleaver.h"

// ── Source index helpers ──────────────────────────────────────

// QPSK TX (interleave): src[j] = π⁻¹(j) = 16*(j%25) + j/25
static int src_qpsk_tx(int j) {
    #pragma HLS INLINE
    return 16 * (j % 25) + j / 25;
}

// QPSK RX (deinterleave): src[j] = π(j) = 25*(j%16) + j/16
static int src_qpsk_rx(int j) {
    #pragma HLS INLINE
    return 25 * (j & 15) + (j >> 4);
}

// 16-QAM TX (interleave): src[j] = π⁻¹(j)
// π₂ is self-inverse (pair-swap), k_div50 = c = j/50 always.
static int src_16qam_tx(int j) {
    #pragma HLS INLINE
    int c   = j / 50;
    int r   = j % 50;
    int k_r = (c & 1) ? ((r & 1) ? r - 1 : r + 1) : r;
    return 16 * k_r + c;
}

// 16-QAM RX (deinterleave): src[j] = π(j)
// π₁: k = 50*(j%16) + j/16;  π₂: pair-swap for odd columns.
static int src_16qam_rx(int j) {
    #pragma HLS INLINE
    int c = j & 15;            // j % 16
    int r = j >> 4;            // j / 16
    int k = 50 * c + r;
    return (c & 1) ? ((r & 1) ? k - 1 : k + 1) : k;
}

// ── Top-level ─────────────────────────────────────────────────

void interleaver(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& data_out,
    mod_t                    mod,
    ap_uint<8>               n_syms,
    ap_uint<1>               is_rx
) {
    #pragma HLS INTERFACE axis      port=data_in
    #pragma HLS INTERFACE axis      port=data_out
    #pragma HLS INTERFACE s_axilite port=mod    bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=n_syms bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=is_rx  bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return bundle=ctrl

    // Bit buffer: max 100 bytes for 16-QAM.
    // LUTRAM: ~13 LUTs, single-port.  Fill and permute are separate
    // sequential phases so there are no read-write port conflicts.
    ap_uint<8> buf[100];
    #pragma HLS BIND_STORAGE variable=buf type=RAM_1P impl=LUTRAM

    SYMBOL_LOOP: for (int s = 0; s < (int)n_syms; s++) {
        if (mod == 0) {
            // ── QPSK: 50 bytes = 400 bits per symbol ─────────

            FILL_Q: for (int i = 0; i < 50; i++) {
                #pragma HLS PIPELINE II=1
                buf[i] = data_in.read();
            }

            ap_uint<8> out_byte = 0;
            PERM_Q: for (int j = 0; j < 400; j++) {
                #pragma HLS PIPELINE II=1
                #pragma HLS loop_tripcount min=400 max=400

                int src = is_rx ? src_qpsk_rx(j) : src_qpsk_tx(j);

                // Read source bit (MSB-first: bit 7 of byte = first bit in stream)
                ap_uint<1> bit = buf[src >> 3][7 - (src & 7)];

                // Shift-accumulate MSB-first into output byte
                out_byte = (ap_uint<8>)((out_byte << 1) | (ap_uint<8>)bit);
                if ((j & 7) == 7) {
                    data_out.write(out_byte);
                    out_byte = 0;
                }
            }

        } else {
            // ── 16-QAM: 100 bytes = 800 bits per symbol ──────

            FILL_16: for (int i = 0; i < 100; i++) {
                #pragma HLS PIPELINE II=1
                buf[i] = data_in.read();
            }

            ap_uint<8> out_byte = 0;
            PERM_16: for (int j = 0; j < 800; j++) {
                #pragma HLS PIPELINE II=1
                #pragma HLS loop_tripcount min=800 max=800

                int src = is_rx ? src_16qam_rx(j) : src_16qam_tx(j);

                ap_uint<1> bit = buf[src >> 3][7 - (src & 7)];

                out_byte = (ap_uint<8>)((out_byte << 1) | (ap_uint<8>)bit);
                if ((j & 7) == 7) {
                    data_out.write(out_byte);
                    out_byte = 0;
                }
            }
        }
    }
}
