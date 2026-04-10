// ============================================================
// viterbi_dec.cpp v2 — Sliding-Window Hard-Decision Viterbi Decoder
//
// v2 optimisations over v1 (target: ~4,000 LUT, was 13,388 LUT):
//   • ACS:     PIPELINE II=1 + UNROLL factor=16
//               → 16 butterflies in parallel, 4 cycles/stage.
//   • pm/pm_new: keep complete partition (registers, unlimited read ports).
//   • circ_wr_idx counter: replaces fwd % CIRC_SIZE throughout.
//               CIRC_SIZE=192 is non-power-of-2 → eliminates 3 sequential
//               integer dividers (35-36 cycles each) that caused 23 ns
//               critical path and 37 cycles/iter in FLUSH_TB/WIN_TB.
//   • win_bits: ap_uint<1>[96]+complete replaced by ap_uint<WIN_LEN>
//               packed register → eliminates ~1,500 LUT mux.
//   • branch_outputs: INLINE.
//   • COPY_PM: PIPELINE II=1 + UNROLL factor=16 (4 cycles/copy).
//   • BEST_S:  PIPELINE II=1 sequential scan.
//
// Throughput (16-QAM r2/3, 200 data SCs → 536 data bits):
//   total_fwd = 672 stages × ~10 cycles ≈ 6,720 cycles
//   Traceback  7 windows   × ~350 cycles ≈ 2,450 cycles
//   Total                               ≈ 9,170 cycles < 14,400 ✓
// ============================================================
#include "conv_fec.h"

// WIN_LEN=128 (power of 2): n_padded = (n_data_bits+127)&~127 — bit mask, no
// division.  Previous WIN_LEN=96 caused a sequential 32÷96 divider (238 LUT +
// 394 FF).  128 ≥ 5×K=35 so convergence guarantee is still comfortably met.
#define WIN_LEN    128
#define FLUSH_LEN  128
#define CIRC_SIZE  (WIN_LEN + FLUSH_LEN)   // 256 (power of 2, but kept via counter)

static void branch_outputs(int s, ap_uint<1> b, ap_uint<1>& g0, ap_uint<1>& g1)
{
#pragma HLS INLINE
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
    // Path metrics — complete partition (flip-flop registers, free read ports).
    // Within a batch of 16 consecutive sp values, all p0 indices are distinct
    // (batch 0: p0 ∈ {0,2,...,30}; batch 1: {32,...,62}) → no read conflicts.
    ap_uint<16> pm[N_STATES];
    ap_uint<16> pm_new[N_STATES];
#pragma HLS array_partition variable=pm     complete
#pragma HLS array_partition variable=pm_new complete

    // Circular trellis buffer — 192 × 64 bits = 12,288 bits ≈ 1 BRAM_18K
    ap_uint<64> circ[CIRC_SIZE];
#pragma HLS bind_storage variable=circ type=RAM_2P impl=BRAM

    // Decoded bits for current window — 128-bit packed register eliminates MUX.
    ap_uint<WIN_LEN> win_packed = 0;

    // Initialise path metrics
    INIT: for (int s = 0; s < N_STATES; s++) {
#pragma HLS unroll
        pm[s] = (s == 0) ? ap_uint<16>(0) : ap_uint<16>(0x7FFF);
    }

    const int n_data_bits   = n_data_bytes * 8;
    const int n_coded_bits  = (rate == 0) ? (n_data_bits * 2)
                                          : (n_data_bits / 2 * 3);
    const int n_coded_bytes = (n_coded_bits + 7) / 8;
    // Bit-mask padding (WIN_LEN=128 is a power of 2 → no division):
    const int n_padded      = (n_data_bits + (WIN_LEN - 1)) & ~(WIN_LEN - 1);
    const int total_fwd     = n_padded + FLUSH_LEN;

    ap_uint<8> in_byte    = 0;
    int        buf_bits   = 0;
    int        bytes_rd   = 0;
    int        win_cnt    = 0;

    // Circular-buffer write index — replaces fwd % CIRC_SIZE everywhere.
    // Avoids non-power-of-2 integer division (3 × 35-cycle dividers in v1).
    int circ_wr_idx = 0;

    // ── Main forward pass + windowed traceback ────────────────
    MAIN_LOOP: for (int fwd = 0; fwd < total_fwd; fwd++) {
#pragma HLS loop_tripcount max=(MAX_DATA_BITS + FLUSH_LEN)

        // ── Read r0 (G0) and r1 (G1) for this trellis stage ──
        ap_uint<2> r0, r1;

        if (fwd < n_data_bits) {
            if (buf_bits == 0) {
                if (bytes_rd < n_coded_bytes) in_byte = coded_in.read();
                bytes_rd++;
                buf_bits = 8;
            }
            buf_bits--;
            r0 = (in_byte >> buf_bits) & 1;

            if (rate == 0 || (fwd & 1) == 0) {
                if (buf_bits == 0) {
                    if (bytes_rd < n_coded_bytes) in_byte = coded_in.read();
                    bytes_rd++;
                    buf_bits = 8;
                }
                buf_bits--;
                r1 = (in_byte >> buf_bits) & 1;
            } else {
                r1 = 2;   // punctured — erasure contributes 0
            }
        } else {
            r0 = 2; r1 = 2;   // flush-padding stages
        }

        // ── ACS: 16 butterflies/cycle, 4 cycles/stage ─────────
        // UNROLL factor=16: 16 parallel copies; PIPELINE II=1: 4 super-iters.
        // pred_bits[]: complete partition → each bit is an independent 1-bit
        // register write, avoiding the 16-deep bitset chain on pred_word that
        // caused the 23.418 ns critical path.
        ap_uint<1> pred_bits[N_STATES];
#pragma HLS array_partition variable=pred_bits complete

        ACS: for (int sp = 0; sp < N_STATES; sp++) {
#pragma HLS PIPELINE II=1
#pragma HLS unroll factor=16
            ap_uint<1> b = (ap_uint<1>)((sp >> 5) & 1);
            int p0       = (sp & 0x1F) << 1;
            int p1       = p0 | 1;

            ap_uint<1> g0, g1;
            branch_outputs(p0, b, g0, g1);

            ap_uint<2> bm0 = 0;
            if (r0 != 2) bm0 += (ap_uint<1>)(r0[0] ^ g0);
            if (r1 != 2) bm0 += (ap_uint<1>)(r1[0] ^ g1);

            ap_uint<2> n_active = (ap_uint<1>)(r0 != 2) + (ap_uint<1>)(r1 != 2);
            ap_uint<2> bm1      = n_active - bm0;

            ap_uint<16> cost0 = pm[p0] + bm0;
            ap_uint<16> cost1 = pm[p1] + bm1;

            ap_uint<1> chose_p1 = (cost1 < cost0) ? ap_uint<1>(1) : ap_uint<1>(0);
            pm_new[sp]   = chose_p1 ? cost1 : cost0;
            pred_bits[sp] = chose_p1;
        }

        // Pack pred_bits → pred_word: fully unrolled, all bit indices are
        // compile-time constants → direct wires, no combinational chain.
        ap_uint<64> pred_word = 0;
        PACK_PRED: for (int sp = 0; sp < N_STATES; sp++) {
#pragma HLS unroll
            pred_word[sp] = pred_bits[sp];
        }

        // Store packed predecessor word; advance write pointer with wrap.
        circ[circ_wr_idx] = pred_word;
        circ_wr_idx = (circ_wr_idx == CIRC_SIZE - 1) ? 0 : circ_wr_idx + 1;

        // Copy pm_new → pm (4 cycles with unroll=16)
        COPY_PM: for (int s = 0; s < N_STATES; s++) {
#pragma HLS PIPELINE II=1
#pragma HLS unroll factor=16
            pm[s] = pm_new[s];
        }

        // ── Windowed traceback every WIN_LEN stages after warmup ─
        if (win_cnt == WIN_LEN - 1 && fwd >= FLUSH_LEN + WIN_LEN - 1) {

            const int out_start = fwd - FLUSH_LEN - WIN_LEN + 1;

            if (out_start < n_data_bits) {

                // Find minimum-metric terminal state
                int best_s = 0;
                ap_uint<16> best_val = pm[0];
                BEST_S: for (int s = 1; s < N_STATES; s++) {
#pragma HLS PIPELINE II=1
                    if (pm[s] < best_val) { best_val = pm[s]; best_s = s; }
                }

                int state = best_s;

                // tb_idx starts at last-written entry (circ_wr_idx - 1 with wrap)
                // and decrements through the circular buffer during traceback.
                int tb_idx = (circ_wr_idx == 0) ? CIRC_SIZE - 1 : circ_wr_idx - 1;

                // Flush traceback: FLUSH_LEN steps — discard (resolves start
                // state uncertainty; no integer division needed).
                FLUSH_TB: for (int k = 0; k < FLUSH_LEN; k++) {
                    ap_uint<1> p = circ[tb_idx][state];
                    state  = ((state & 0x1F) << 1) | (int)p;
                    tb_idx = (tb_idx == 0) ? CIRC_SIZE - 1 : tb_idx - 1;
                }

                // Output traceback → packed register (chronological order).
                WIN_TB: for (int k = 0; k < WIN_LEN; k++) {
                    win_packed[WIN_LEN - 1 - k] = (ap_uint<1>)((state >> 5) & 1);
                    ap_uint<1> p = circ[tb_idx][state];
                    state  = ((state & 0x1F) << 1) | (int)p;
                    tb_idx = (tb_idx == 0) ? CIRC_SIZE - 1 : tb_idx - 1;
                }

                // Pack win_packed into output bytes (MSB-first, 12 bytes/window)
                PACK_WIN: for (int wb = 0; wb < WIN_LEN / 8; wb++) {
                    const int bit_base = wb * 8;
                    if (out_start + bit_base < n_data_bits) {
                        ap_uint<8> ob = 0;
                        for (int bb = 0; bb < 8; bb++) {
#pragma HLS unroll
                            ob[7 - bb] = win_packed[bit_base + bb];
                        }
                        data_out.write(ob);
                    }
                }
            }
        }

        win_cnt = (win_cnt == WIN_LEN - 1) ? 0 : win_cnt + 1;
    }
}
