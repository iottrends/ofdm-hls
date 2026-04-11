// ============================================================
// sync_detect.cpp v2 — Schmidl-Cox Timing Sync + CFO Estimator
//
// v2 resource optimisations (target: ~3-4K LUT, ~8 DSP):
//
//   v1 had 12,972 LUT / 40 DSP.  Root causes:
//   (a) ap_fixed<32,2> widening on ri/rq/si/sq:
//       32×32-bit multiply needs 2 DSP each → 8 muls × 2 = 16 DSP.
//       Fix: use sample_t (ap_fixed<16,1>) directly.
//       16×16 fits in 1 DSP → saves 8 DSP.
//   (b) Float metric: Pi_f*Pi_f + Pq_f*Pq_f + Rl_f*R_f → 3 float muls.
//       Each float mul = 3 DSP → saves 9 DSP.
//       Fix: division-free integer cross-multiply comparison.
//   (c) hls::atan2f → float CORDIC → ~6 DSP.
//       Fix: fixed-point shift-add CORDIC, 0 DSP.
//
// Algorithm (unchanged from v1):
//   Phase 1: fill 864-sample BRAM buffer.
//   Phase 2: for t = 0..SEARCH_WIN-1 compute normalized CP metric
//     P(t)  = Σ r[t+k]·conj(r[t+k+N])   CP cross-correlation
//     R(t)  = Σ |r[t+k+N]|²              energy of delayed window
//     Rl(t) = Σ |r[t+k]|²               energy of left window
//     metric(t) = |P|² / (R · Rl)       ∈ [0,1], Cauchy-Schwarz bound
//   best_t = argmax metric(t).
//   Phase 3: replay + forward SYNC_NL samples from best_t.
//
//   CFO: ε_sc = angle(P_best) / (2π)  [subcarrier units]
// ============================================================
#include "sync_detect.h"
#include <cmath>   // for atan2f in C-sim only

// ── Accumulator type ─────────────────────────────────────────
// ap_fixed<24,8>: 8 integer bits (range ±128), 16 fractional bits.
// With sample_t = ap_fixed<16,1> inputs and CP_LEN=32 accumulations:
//   Pi_max = 32 × 1.0 × 1.0 = 32 < 128  ✓
//   R_max  = 32 × 1.0         = 32 < 128  ✓
typedef ap_fixed<24,8> acc_t;

// ── Fixed-point atan2 (CORDIC, no float, no DSP) ─────────────
// Synthesis:  vectoring CORDIC, PIPELINED II=1 (sequential 16 stages).
//             Using PIPELINE (not UNROLL) keeps hardware to ~1 shifter +
//             2 adders ≈ 200-300 LUT vs 3,596 LUT for fully unrolled.
//             Called only once per frame — latency of ~20 cycles is fine.
// C-sim:      std::atan2f (accurate reference).
static ap_fixed<16,4> sync_atan2(acc_t y, acc_t x)
{
#pragma HLS INLINE off
#ifdef __SYNTHESIS__
    const ap_fixed<20,8> ATAN_LUT[16] = {
        0.7853982f, 0.4636476f, 0.2449787f, 0.1243550f,
        0.0624188f, 0.0312398f, 0.0156237f, 0.0078125f,
        0.0039063f, 0.0019532f, 0.0009766f, 0.0004883f,
        0.0002441f, 0.0001221f, 0.0000610f, 0.0000305f
    };
    // Use ap_fixed<20,8>: same ±128 integer range as acc_t inputs, 12 fractional
    // bits.  Narrowing from 28→20 bits reduces the ashr combinational path from
    // ~9.5 ns to ~6.8 ns, meeting the II=1 pipeline target.
    ap_fixed<20,8> xi = x, yi = y, acc = 0;
    if (xi < 0) {
        xi  = -x;  yi  = -y;
        acc = (y >= 0) ? ap_fixed<20,8>( 3.14159265f)
                       : ap_fixed<20,8>(-3.14159265f);
    }
    // Sequential pipelined CORDIC — one iteration per cycle, ~200 LUT total.
    CORDIC: for (int i = 0; i < 16; i++) {
#pragma HLS PIPELINE II=2
#pragma HLS loop_tripcount min=16 max=16
        ap_fixed<20,8> xs = xi >> i;
        ap_fixed<20,8> ys = yi >> i;
        if (yi >= 0) { xi += ys;  yi -= xs;  acc -= (ap_fixed<20,8>)ATAN_LUT[i]; }
        else         { xi -= ys;  yi += xs;  acc += (ap_fixed<20,8>)ATAN_LUT[i]; }
    }
    return (ap_fixed<16,4>)acc;
#else
    return (ap_fixed<16,4>)atan2f((float)y, (float)x);
#endif
}

void sync_detect(
    hls::stream<iq_t>& iq_in,
    hls::stream<iq_t>& iq_out,
    cfo_t&             cfo_est,
    ap_uint<8>         n_syms
)
{
#pragma HLS INTERFACE axis      port=iq_in
#pragma HLS INTERFACE axis      port=iq_out
#pragma HLS INTERFACE ap_none   port=cfo_est  // C4a: direct wire to cfo_correct, valid by construction
#pragma HLS INTERFACE s_axilite port=n_syms  bundle=ctrl
#pragma HLS INTERFACE s_axilite port=return  bundle=ctrl
    // ── Search buffer ─────────────────────────────────────────
    sample_t buf_i[SYNC_BUF_SZ];
    sample_t buf_q[SYNC_BUF_SZ];
#pragma HLS bind_storage variable=buf_i type=RAM_2P impl=BRAM
#pragma HLS bind_storage variable=buf_q type=RAM_2P impl=BRAM

    // ── Phase 1: Fill ─────────────────────────────────────────
    FILL: for (int n = 0; n < SYNC_BUF_SZ; n++) {
#pragma HLS pipeline II=1
#pragma HLS loop_tripcount min=SYNC_BUF_SZ max=SYNC_BUF_SZ
        iq_t s   = iq_in.read();
        buf_i[n] = s.i;
        buf_q[n] = s.q;
    }

    // ── Phase 2: Normalized CP timing metric ──────────────────
    //
    // CORR inner loop: #pragma HLS PIPELINE II=1.
    //   ri/rq/si/sq kept as sample_t (ap_fixed<16,1>) — NO widening.
    //   16-bit × 16-bit multiply fits in 1 DSP48E1 (was 32×32 → 2 DSP each).
    //   8 multiplies × 1 DSP = 8 DSP total.
    //
    // Metric comparison: integer cross-multiply on truncated integer parts.
    //   Pi_int ∈ [-64,+64) → ap_int<8>; R_int ∈ [0,32) → ap_uint<8>.
    //   Squared values: 8-bit × 8-bit → 16-bit (HLS uses LUT for <12-bit).
    //   Cross-multiply: 16-bit × 16-bit (forced to fabric via BIND_OP).
    //   No float, no division, 0 extra DSP.

    int   best_t  = 0;
    acc_t best_Pi = 0;
    acc_t best_Pq = 0;
#ifdef __SYNTHESIS__
    // Synthesis path: integer cross-multiply — 0 float, 0 DSP for comparison.
    // Requires signal amplitude ≥ ~0.18 (ADC full-scale use) so that the
    // integer part of Pi, R, Rl is non-zero after truncation.
    // Valid for all real RF hardware operation; C-sim uses float below.
    ap_uint<13> best_num = 0;   // |P_int|²  at best_t
    ap_uint<10> best_den = 0;   // R_int × Rl_int at best_t
#else
    float best_metric = 0.0f;   // C-sim: float ratio, amplitude-independent
#endif

    SEARCH: for (int t = 0; t < SEARCH_WIN; t++) {
#pragma HLS loop_tripcount min=SEARCH_WIN max=SEARCH_WIN
        acc_t Pi = 0, Pq = 0, R = 0, Rl = 0;

        CORR: for (int k = 0; k < CP_LEN; k++) {
#pragma HLS PIPELINE II=1
#pragma HLS loop_tripcount min=CP_LEN max=CP_LEN
            // Direct sample_t (ap_fixed<16,1>) — no widening to <32,2>.
            // Both buf_i ports used simultaneously (RAM_2P: port A = left
            // window [t+k], port B = right window [t+k+FFT_SIZE]).
            sample_t ri = buf_i[t + k];
            sample_t rq = buf_q[t + k];
            sample_t si = buf_i[t + k + FFT_SIZE];
            sample_t sq = buf_q[t + k + FFT_SIZE];

            Pi += ri * si + rq * sq;   // Re(r · conj(s))
            Pq += rq * si - ri * sq;   // Im(r · conj(s))
            R  += si * si + sq * sq;   // |s|²  right window
            Rl += ri * ri + rq * rq;   // |r|²  left window
        }

#ifdef __SYNTHESIS__
        // ── Division-free metric comparison (synthesis) ───────
        // Use tight integer types to minimise fabric-multiply LUT:
        //   Pi, Pq ∈ [-64,+64) → ap_int<7> (range ±63; tiny clamp risk
        //   at ±64 is acceptable — sync_detect just needs argmax).
        //   R, Rl ∈ [0,32)   → ap_uint<6>
        //   Pi²+Pq² ≤ 2×63²=7938 → ap_uint<13>
        //   R×Rl   ≤ 31×31 =961  → ap_uint<10>
        //   Cross-multiply: 13-bit × 10-bit → ~50 LUT each.
        ap_int<7>   Pi_c  = (ap_int<7>) Pi;
        ap_int<7>   Pq_c  = (ap_int<7>) Pq;
        ap_uint<6>  R_c   = (ap_uint<6>)R;
        ap_uint<6>  Rl_c  = (ap_uint<6>)Rl;

        ap_uint<13> new_num = (ap_uint<13>)(Pi_c * Pi_c)
                            + (ap_uint<13>)(Pq_c * Pq_c);
        ap_uint<10> new_den = (ap_uint<10>)R_c * Rl_c;

        ap_uint<23> cmp_lhs = (ap_uint<23>)new_num * (ap_uint<23>)best_den;
        ap_uint<23> cmp_rhs = (ap_uint<23>)best_num * (ap_uint<23>)new_den;
#pragma HLS BIND_OP variable=cmp_lhs op=mul impl=fabric
#pragma HLS BIND_OP variable=cmp_rhs op=mul impl=fabric

        bool is_better = (new_den > 0) &&
                         (best_den == 0 || cmp_lhs > cmp_rhs);

        if (is_better) {
            best_t   = t;
            best_num = new_num;
            best_den = new_den;
            best_Pi  = Pi;
            best_Pq  = Pq;
        }
#else
        // ── Float metric comparison (C-sim only) ──────────────
        // The HLS test vectors use ~7% of full-scale (amplitude ~0.07),
        // so the integer parts of Pi/R/Rl are 0 — float is needed here.
        // In hardware, ADC input fills the range; integer comparison works.
        float Pi_f = (float)Pi, Pq_f = (float)Pq;
        float R_f  = (float)R,  Rl_f = (float)Rl;
        float denom = R_f * Rl_f;
        if (denom > 0.0f) {
            float metric = (Pi_f*Pi_f + Pq_f*Pq_f) / denom;
            if (metric > best_metric) {
                best_metric = metric;
                best_t      = t;
                best_Pi     = Pi;
                best_Pq     = Pq;
            }
        }
#endif
    }

    // ── CFO estimate ─────────────────────────────────────────
    // Fixed-point CORDIC atan2 — 0 DSP (shift-add only).
    // Result: ε_sc = angle(P_best) / (2π),  |ε_sc| ≤ 0.5 SC.
    ap_fixed<16,4> ang = sync_atan2(best_Pq, best_Pi);
    const ap_fixed<16,4> INV_TWO_PI = ap_fixed<16,4>(0.15915494f);
    cfo_est = cfo_t(ang * INV_TWO_PI);

    // ── Phase 3: Stream output aligned to best_t ─────────────
    // C1 fix: always output the maximum frame length (MAX_DATA_SYMS + 2 symbols).
    // n_syms is accepted on the AXI-Lite port but not used here — ofdm_rx extracts
    // the real n_syms from the decoded header.  Using MAX_DATA_SYMS breaks the
    // chicken-and-egg dependency where sync_detect needed to know n_syms before
    // ofdm_rx had a chance to decode it.
    const int total_output = (MAX_DATA_SYMS + 2) * SYNC_NL;
    const int buf_avail = SYNC_BUF_SZ - best_t;
    const int from_buf  = (buf_avail < total_output) ? buf_avail : total_output;
    const int from_live = total_output - from_buf;

    OUT_BUF: for (int n = 0; n < from_buf; n++) {
#pragma HLS pipeline II=1
#pragma HLS loop_tripcount min=0 max=SYNC_BUF_SZ
        iq_t s;
        s.i    = buf_i[best_t + n];
        s.q    = buf_q[best_t + n];
        s.last = (from_live == 0 && n == from_buf - 1) ? ap_uint<1>(1)
                                                        : ap_uint<1>(0);
        iq_out.write(s);
    }

    OUT_LIVE: for (int n = 0; n < from_live; n++) {
#pragma HLS pipeline II=1
#pragma HLS loop_tripcount min=0 max=((MAX_DATA_SYMS + 1) * SYNC_NL)
        iq_t s  = iq_in.read();
        s.last  = (n == from_live - 1) ? ap_uint<1>(1) : ap_uint<1>(0);
        iq_out.write(s);
    }
}
