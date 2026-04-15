// ============================================================
// viterbi_dec.cpp v3 — Sliding-Window Hard-Decision Viterbi Decoder
//
// v3 throughput upgrade for 16-QAM rate-2/3 real-time decoding.
//
// Problem with v2 (unroll=16 @ 100 MHz)
// ─────────────────────────────────────
//   ACS ran 4 cycles/stage → ~10 cycles/trellis stage at 100 MHz.
//   16-QAM r2/3, 255 syms → 135,915 data bits × 10 cycles = 1.36 M cy
//   = 13.6 ms on 100 MHz clock, vs. 3.67 ms real-time budget.
//   → v2 is ~3.7× too slow.  Cannot keep up with 16-QAM r1/2 or r2/3.
//
// v3 fixes
// ────────
//   • ACS:     UNROLL factor=64 → 1 cycle/stage (64 parallel butterflies).
//   • COPY_PM: UNROLL full      → 1 cycle (64 parallel FF-to-FF copies).
//   • FLUSH_TB / WIN_TB: PIPELINE II=1 explicit.
//   • Solution clock period must be set to 5 ns (200 MHz) in the HLS
//     solution (create_clock -period 5).  That is the only "knob" that
//     actually lifts Fmax — pragmas alone don't.
//
// Expected throughput at 200 MHz (16-QAM r2/3, 255 syms):
//   Forward : 136k stages × 2 cy       ≈ 272k cy
//   Traceback: 1,060 wins × 256 cy II=1 ≈ 272k cy
//   Total    ≈ 544k cy / 200 MHz       ≈ 2.7 ms  <  3.67 ms ✓
//
// Timing-closure note (5 ns is tight)
// ──────────────────────────────────
//   Each butterfly's critical path is:
//     pm[] FF read → 16-bit add (bm) → 16-bit compare cost0/cost1 → mux
//     → pm_new[] FF write.
//   On Artix-7 -1 that is ~4.0-4.5 ns — fits 5 ns but with little slack.
//   If 200 MHz closure fails in P&R, the correct next move is to split
//   the ACS into two pipeline stages (II=2 across the add/compare
//   boundary) — NOT to reduce unroll.  II=2 at 200 MHz still beats
//   II=4 at 100 MHz.
//
// Resource cost
// ─────────────
//   ACS logic grows ~4× (16 → 64 parallel adders/compares), roughly
//   4,000 LUT for the ACS itself.  pm/pm_new remain complete-partitioned
//   (64 × 16 = 1024 FF each).  On Artix-50T (32,600 LUT) this leaves
//   ample headroom.
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
#pragma HLS INTERFACE axis      port=coded_in
#pragma HLS INTERFACE axis      port=data_out
#pragma HLS INTERFACE s_axilite port=rate         bundle=ctrl
#pragma HLS INTERFACE s_axilite port=n_data_bytes bundle=ctrl
#pragma HLS INTERFACE s_axilite port=return       bundle=ctrl
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
#pragma HLS unroll factor=64
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

        // Copy pm_new → pm (1 cycle, fully unrolled — 64 parallel FF copies)
        COPY_PM: for (int s = 0; s < N_STATES; s++) {
#pragma HLS unroll
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
#pragma HLS PIPELINE II=1
                    ap_uint<1> p = circ[tb_idx][state];
                    state  = ((state & 0x1F) << 1) | (int)p;
                    tb_idx = (tb_idx == 0) ? CIRC_SIZE - 1 : tb_idx - 1;
                }

                // Output traceback → packed register (chronological order).
                WIN_TB: for (int k = 0; k < WIN_LEN; k++) {
#pragma HLS PIPELINE II=1
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
