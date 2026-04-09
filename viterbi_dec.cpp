// ============================================================
// viterbi_dec.cpp  —  Sliding-Window Hard-Decision Viterbi Decoder
//
// Memory: 1121 BRAM_18K (full-trellis) → ~1 BRAM_18K (sliding window)
//
// Key idea:
//   Full trellis stores dec[145000][64] = 9.28 Mbit — doesn't fit.
//   Sliding window keeps only CIRC_SIZE=192 trellis stages at a time
//   in a circular buffer (192 × 64 = 12,288 bits).
//
// Window parameters:
//   WIN_LEN   = 96  decoded bits output per traceback
//   FLUSH_LEN = 96  look-ahead stages to guarantee path convergence
//                   (≥ 5×K = 35; 96 gives ample margin)
//   CIRC_SIZE = 192 circular buffer depth
//
// Algorithm per window:
//   1. Forward ACS for WIN_LEN stages (store pred bits in circ[])
//   2. When win_cnt reaches WIN_LEN-1 and warmup complete:
//      a. Find best terminal state (min path metric)
//      b. Traceback FLUSH_LEN stages → discard (flushes initial uncertainty)
//      c. Traceback WIN_LEN stages  → win_bits[] (chronological order)
//      d. Pack win_bits → output bytes
//
// Trellis convention (matches conv_enc.cpp):
//   new_sr = (b << 5) | (old_sr >> 1)
//   Given new state sp:
//     decoded bit b  = sp[5]
//     predecessor p0 = (sp & 0x1F) << 1
//     predecessor p1 = ((sp & 0x1F) << 1) | 1
//   Complement property: bm1 = n_active - bm0
// ============================================================
#include "conv_fec.h"

#define WIN_LEN    96                     // decoded bits per output window
#define FLUSH_LEN  96                     // convergence depth (≥ 5×K = 35)
#define CIRC_SIZE  (WIN_LEN + FLUSH_LEN)  // 192 — circular buffer entries

// Expected encoder outputs for predecessor state s encoding bit b
static void branch_outputs(int s, ap_uint<1> b, ap_uint<1>& g0, ap_uint<1>& g1)
{
    g0 = b ^ (ap_uint<1>)(s >> 4) ^ (ap_uint<1>)(s >> 3)
           ^ (ap_uint<1>)(s >> 1) ^ (ap_uint<1>)s;
    g1 = b ^ (ap_uint<1>)(s >> 5) ^ (ap_uint<1>)(s >> 4)
           ^ (ap_uint<1>)(s >> 3) ^ (ap_uint<1>)s;
}

void viterbi_dec(
    hls::stream<ap_uint<8>>& coded_in,
    hls::stream<ap_uint<8>>& data_out,
    rate_t rate,
    int    n_data_bytes)
{
    // Path metrics — fully partitioned into registers (64 × 16 bits = 1024 bits)
    ap_uint<16> pm[N_STATES];
    ap_uint<16> pm_new[N_STATES];
#pragma HLS array_partition variable=pm     complete
#pragma HLS array_partition variable=pm_new complete

    // Circular trellis buffer — 192 × 64 bits = 12,288 bits ≈ 1 BRAM_18K
    // circ[t % CIRC_SIZE][sp] = 0 → chose predecessor p0, 1 → chose p1
    ap_uint<64> circ[CIRC_SIZE];
#pragma HLS bind_storage variable=circ type=RAM_2P impl=BRAM

    // Decoded bits for current window — in registers
    ap_uint<1> win_bits[WIN_LEN];
#pragma HLS array_partition variable=win_bits complete

    // Initialise: all-zeros start state has cost 0, all others max
    for (int s = 0; s < N_STATES; s++) {
#pragma HLS unroll
        pm[s] = (s == 0) ? ap_uint<16>(0) : ap_uint<16>(0x7FFF);
    }

    const int n_data_bits   = n_data_bytes * 8;
    const int n_coded_bits  = (rate == 0) ? (n_data_bits * 2)
                                          : (n_data_bits / 2 * 3);
    const int n_coded_bytes = (n_coded_bits + 7) / 8;

    // Round up to next multiple of WIN_LEN so every window aligns cleanly
    const int n_padded  = ((n_data_bits + WIN_LEN - 1) / WIN_LEN) * WIN_LEN;
    const int total_fwd = n_padded + FLUSH_LEN;

    // Streaming coded-input state
    ap_uint<8> in_byte  = 0;
    int        buf_bits = 0;   // valid bits remaining in in_byte
    int        bytes_rd = 0;   // coded bytes consumed from stream

    // Window counter: 0 .. WIN_LEN-1, resets each window
    int win_cnt = 0;

    // ── Main forward pass + windowed traceback ────────────────
    MAIN_LOOP: for (int fwd = 0; fwd < total_fwd; fwd++) {
#pragma HLS loop_tripcount max=(MAX_DATA_BITS + FLUSH_LEN)

        // ── Get r0 (G0) and r1 (G1) for this trellis stage ───
        ap_uint<2> r0, r1;

        if (fwd < n_data_bits) {
            // G0 — always present
            if (buf_bits == 0) {
                if (bytes_rd < n_coded_bytes) in_byte = coded_in.read();
                bytes_rd++;
                buf_bits = 8;
            }
            buf_bits--;
            r0 = (in_byte >> buf_bits) & 1;

            // G1 — present for rate 1/2; punctured on odd steps for rate 2/3
            if (rate == 0 || (fwd & 1) == 0) {
                if (buf_bits == 0) {
                    if (bytes_rd < n_coded_bytes) in_byte = coded_in.read();
                    bytes_rd++;
                    buf_bits = 8;
                }
                buf_bits--;
                r1 = (in_byte >> buf_bits) & 1;
            } else {
                r1 = 2;  // punctured — treated as erasure (contributes 0)
            }
        } else {
            r0 = 2; r1 = 2;  // flush-padding stages — full erasure
        }

        // ── ACS butterfly: all 64 states in parallel ─────────
        ap_uint<64> pred_word = 0;

        ACS: for (int sp = 0; sp < N_STATES; sp++) {
#pragma HLS unroll
            ap_uint<1> b = (sp >> 5) & 1;       // decoded bit = new-state MSB
            int p0       = (sp & 0x1F) << 1;    // predecessor: old_state[0] = 0
            int p1       = ((sp & 0x1F) << 1) | 1; // predecessor: old_state[0] = 1

            ap_uint<1> g0, g1;
            branch_outputs(p0, b, g0, g1);

            // Branch metric for p0
            ap_uint<2> bm0 = 0;
            if (r0 != 2) bm0 += (ap_uint<1>)(r0[0] ^ g0);
            if (r1 != 2) bm0 += (ap_uint<1>)(r1[0] ^ g1);

            // Branch metric for p1 via complement property: bm0 + bm1 = n_active
            ap_uint<2> n_active = (ap_uint<1>)(r0 != 2) + (ap_uint<1>)(r1 != 2);
            ap_uint<2> bm1      = n_active - bm0;

            ap_uint<16> cost0 = pm[p0] + bm0;
            ap_uint<16> cost1 = pm[p1] + bm1;

            if (cost0 <= cost1) {
                pm_new[sp]    = cost0;
                pred_word[sp] = 0;  // chose p0
            } else {
                pm_new[sp]    = cost1;
                pred_word[sp] = 1;  // chose p1
            }
        }

        // Store packed predecessor decisions in circular buffer
        circ[fwd % CIRC_SIZE] = pred_word;

        // Advance path metrics
        for (int s = 0; s < N_STATES; s++) {
#pragma HLS unroll
            pm[s] = pm_new[s];
        }

        // ── Windowed traceback: every WIN_LEN stages after warmup ─
        // First trigger: fwd = FLUSH_LEN + WIN_LEN - 1 = 191
        // Subsequent:    every WIN_LEN stages thereafter
        if (win_cnt == WIN_LEN - 1 && fwd >= FLUSH_LEN + WIN_LEN - 1) {

            const int out_start = fwd - FLUSH_LEN - WIN_LEN + 1;

            if (out_start < n_data_bits) {

                // Find minimum-metric terminal state
                int best_s = 0;
                ap_uint<16> best_val = pm[0];
                for (int s = 1; s < N_STATES; s++)
                    if (pm[s] < best_val) { best_val = pm[s]; best_s = s; }

                int state = best_s;

                // Flush traceback: FLUSH_LEN steps — resolves initial
                // state uncertainty; decoded bits discarded
                FLUSH_TB: for (int k = 0; k < FLUSH_LEN; k++) {
                    ap_uint<1> p = circ[(fwd - k) % CIRC_SIZE][state];
                    state = ((state & 0x1F) << 1) | (int)p;
                }

                // Output traceback: WIN_LEN steps — fill win_bits in
                // chronological order (win_bits[0] = oldest = out_start)
                WIN_TB: for (int k = 0; k < WIN_LEN; k++) {
                    win_bits[WIN_LEN - 1 - k] = (ap_uint<1>)((state >> 5) & 1);
                    ap_uint<1> p = circ[(fwd - FLUSH_LEN - k) % CIRC_SIZE][state];
                    state = ((state & 0x1F) << 1) | (int)p;
                }

                // Pack win_bits into output bytes (MSB first, 12 bytes/window)
                // Skip bytes beyond n_data_bits (partial last window)
                PACK_WIN: for (int wb = 0; wb < WIN_LEN / 8; wb++) {
                    const int bit_base = wb * 8;
                    if (out_start + bit_base < n_data_bits) {
                        ap_uint<8> ob = 0;
                        for (int bb = 0; bb < 8; bb++) {
#pragma HLS unroll
                            ob[7 - bb] = win_bits[bit_base + bb];
                        }
                        data_out.write(ob);
                    }
                }
            }
        }

        // Advance window counter
        win_cnt = (win_cnt == WIN_LEN - 1) ? 0 : win_cnt + 1;
    }
}
