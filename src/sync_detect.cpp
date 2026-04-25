// ============================================================
// sync_detect.cpp v4 — Free-running preamble gate + inline CFO
//
// Replaces the old one-shot sync_detect + cfo_correct pair.  One block,
// one clock, 4-state FSM: SEARCH / FWD_PREHDR / WAIT_NSYMS / FWD_DATA.
// Algorithm per docs/RX_GATING_DESIGN.md.
//
// Control: ap_ctrl_none, while(1) body.  ap_start is not driven by
// anyone — block auto-starts after reset.
//
// Resource budget (target Artix-50T at 10 ns):
//   BRAM18: ~9 (circular buffer 4096 × 32 bit for FWD replay + SIN_LUT)
//   DSP48 : ~18 (running sums + CFO complex mult + threshold cross-mult)
//   SRL   : ~544 LUTs (delay lines for sliding-window reads)
//   LUT   : ~3k (excluding SRLs)
//
// Sliding-window reads at fixed offsets from wr_ptr (idx_new_N, idx_old,
// idx_old_N) are implemented as SRL shift-register delay lines, leaving
// the BRAM with only 1 write (wr_ptr) + 1 read (rd_ptr in FWD states)
// — fits RAM_2P with no cyclic partitioning or urem overhead.
//
// Target II: 1 for SEARCH; may relax to 2-4 in FWD states due to
// rd_ptr loop-carry — still easily clears 20 MSPS at 100 MHz.
// ============================================================
#include "sync_detect.h"
#include "free_run.h"
#include <cmath>    // atan2f in csim only
#ifndef __SYNTHESIS__
#include <thread>   // std::this_thread::yield for csim drain detection
#endif

// ── Sizing ──────────────────────────────────────────────────
#define BUF_SIZE     4096
#define BUF_MASK     (BUF_SIZE - 1)     // power-of-2 cheap modulo
#define POW_WIN_LEN  64                 // envelope integration window

// ── Fixed-point accumulator types ───────────────────────────
// sample_t = ap_fixed<16,1>    range [-1, +1), 15 frac bits
//
// sample × sample product: range < 1.0, 30 frac bits.  With CP_LEN=32 terms
// accumulated: max magnitude ≤ 32 × 1.0 = 32, 8 integer bits ample.
typedef ap_fixed<24,8>  acc_t;      // running sums P_re, P_im, R, Rl, pow_env
typedef ap_fixed<32,12> prod_t;     // cross-multiply threshold comparisons

// SC threshold squared in ap_ufixed<16,0>.  0.49 ≈ round(0.49 × 2^16) / 2^16.
static const ap_ufixed<16,0> SC_TH_SQ_CONST = ap_ufixed<16,0>(0.49);

// ── CFO phase-step scaling constant ─────────────────────────
// Derivation (see docs/RX_GATING_DESIGN.md):
//   Δφ_rad/sample = 2π · ε_sc / N         where N=FFT_SIZE=256, ε_sc = ang/(2π)
//   Δφ_NCO/sample = Δφ_rad · 2^32 / (2π)  (full circle = 2^32 NCO units)
//                 = ε_sc · 2^32 / 256
//                 = ε_sc · 2^24           (exact, clean shift)
//                 = (ang / (2π)) · 2^24
//                 = ang · [2^24 / (2π)]
// Precomputed constant:  2^24 / (2π) = 16777216 · 0.15915494... ≈ 2670177
// Max rel error: < 2×10⁻⁷  (22-bit signed representation).
//
// ang is ap_fixed<16,4>, so ang.range() returns raw bits × 2^-12.
// Formula:  phase_step = (ang_raw_signed * TWO24_OVER_2PI) >> 12
// Max product before shift: 2^15 × 2670177 ≈ 8.75e10, fits ap_int<40>.
// After shift: ≤ 2^31 (signed), fits ap_int<32>.
static const ap_int<24> TWO24_OVER_2PI = 2670177;

// ── Quarter-wave sine LUT (copied from cfo_correct.cpp v3) ──
// 256 entries of sin(k × π/512) for k = 0..255.  Quarter-wave symmetry
// gives a full 1024-entry sine table at 1/4 the BRAM cost.
static const sample_t SIN_LUT[256] = {
    sample_t(+0.000000), sample_t(+0.006136), sample_t(+0.012272), sample_t(+0.018407),
    sample_t(+0.024541), sample_t(+0.030675), sample_t(+0.036807), sample_t(+0.042938),
    sample_t(+0.049068), sample_t(+0.055195), sample_t(+0.061321), sample_t(+0.067444),
    sample_t(+0.073565), sample_t(+0.079682), sample_t(+0.085797), sample_t(+0.091909),
    sample_t(+0.098017), sample_t(+0.104122), sample_t(+0.110222), sample_t(+0.116319),
    sample_t(+0.122411), sample_t(+0.128498), sample_t(+0.134581), sample_t(+0.140658),
    sample_t(+0.146730), sample_t(+0.152797), sample_t(+0.158858), sample_t(+0.164913),
    sample_t(+0.170962), sample_t(+0.177004), sample_t(+0.183040), sample_t(+0.189069),
    sample_t(+0.195090), sample_t(+0.201105), sample_t(+0.207111), sample_t(+0.213110),
    sample_t(+0.219101), sample_t(+0.225084), sample_t(+0.231058), sample_t(+0.237024),
    sample_t(+0.242980), sample_t(+0.248928), sample_t(+0.254866), sample_t(+0.260794),
    sample_t(+0.266713), sample_t(+0.272621), sample_t(+0.278520), sample_t(+0.284408),
    sample_t(+0.290285), sample_t(+0.296151), sample_t(+0.302006), sample_t(+0.307850),
    sample_t(+0.313682), sample_t(+0.319502), sample_t(+0.325310), sample_t(+0.331106),
    sample_t(+0.336890), sample_t(+0.342661), sample_t(+0.348419), sample_t(+0.354164),
    sample_t(+0.359895), sample_t(+0.365613), sample_t(+0.371317), sample_t(+0.377007),
    sample_t(+0.382683), sample_t(+0.388345), sample_t(+0.393992), sample_t(+0.399624),
    sample_t(+0.405241), sample_t(+0.410843), sample_t(+0.416430), sample_t(+0.422000),
    sample_t(+0.427555), sample_t(+0.433094), sample_t(+0.438616), sample_t(+0.444122),
    sample_t(+0.449611), sample_t(+0.455084), sample_t(+0.460539), sample_t(+0.465976),
    sample_t(+0.471397), sample_t(+0.476799), sample_t(+0.482184), sample_t(+0.487550),
    sample_t(+0.492898), sample_t(+0.498228), sample_t(+0.503538), sample_t(+0.508830),
    sample_t(+0.514103), sample_t(+0.519356), sample_t(+0.524590), sample_t(+0.529804),
    sample_t(+0.534998), sample_t(+0.540171), sample_t(+0.545325), sample_t(+0.550458),
    sample_t(+0.555570), sample_t(+0.560662), sample_t(+0.565732), sample_t(+0.570781),
    sample_t(+0.575808), sample_t(+0.580814), sample_t(+0.585798), sample_t(+0.590760),
    sample_t(+0.595699), sample_t(+0.600616), sample_t(+0.605511), sample_t(+0.610383),
    sample_t(+0.615232), sample_t(+0.620057), sample_t(+0.624860), sample_t(+0.629638),
    sample_t(+0.634393), sample_t(+0.639124), sample_t(+0.643832), sample_t(+0.648514),
    sample_t(+0.653173), sample_t(+0.657807), sample_t(+0.662416), sample_t(+0.667000),
    sample_t(+0.671559), sample_t(+0.676093), sample_t(+0.680601), sample_t(+0.685084),
    sample_t(+0.689541), sample_t(+0.693971), sample_t(+0.698376), sample_t(+0.702755),
    sample_t(+0.707107), sample_t(+0.711432), sample_t(+0.715731), sample_t(+0.720002),
    sample_t(+0.724247), sample_t(+0.728464), sample_t(+0.732654), sample_t(+0.736816),
    sample_t(+0.740951), sample_t(+0.745058), sample_t(+0.749136), sample_t(+0.753187),
    sample_t(+0.757209), sample_t(+0.761202), sample_t(+0.765167), sample_t(+0.769103),
    sample_t(+0.773010), sample_t(+0.776888), sample_t(+0.780737), sample_t(+0.784557),
    sample_t(+0.788346), sample_t(+0.792107), sample_t(+0.795837), sample_t(+0.799537),
    sample_t(+0.803208), sample_t(+0.806848), sample_t(+0.810457), sample_t(+0.814036),
    sample_t(+0.817585), sample_t(+0.821103), sample_t(+0.824589), sample_t(+0.828045),
    sample_t(+0.831470), sample_t(+0.834863), sample_t(+0.838225), sample_t(+0.841555),
    sample_t(+0.844854), sample_t(+0.848120), sample_t(+0.851355), sample_t(+0.854558),
    sample_t(+0.857729), sample_t(+0.860867), sample_t(+0.863973), sample_t(+0.867046),
    sample_t(+0.870087), sample_t(+0.873095), sample_t(+0.876070), sample_t(+0.879012),
    sample_t(+0.881921), sample_t(+0.884797), sample_t(+0.887640), sample_t(+0.890449),
    sample_t(+0.893224), sample_t(+0.895966), sample_t(+0.898674), sample_t(+0.901349),
    sample_t(+0.903989), sample_t(+0.906596), sample_t(+0.909168), sample_t(+0.911706),
    sample_t(+0.914210), sample_t(+0.916679), sample_t(+0.919114), sample_t(+0.921514),
    sample_t(+0.923880), sample_t(+0.926210), sample_t(+0.928506), sample_t(+0.930767),
    sample_t(+0.932993), sample_t(+0.935184), sample_t(+0.937339), sample_t(+0.939459),
    sample_t(+0.941544), sample_t(+0.943593), sample_t(+0.945607), sample_t(+0.947586),
    sample_t(+0.949528), sample_t(+0.951435), sample_t(+0.953306), sample_t(+0.955141),
    sample_t(+0.956940), sample_t(+0.958703), sample_t(+0.960431), sample_t(+0.962121),
    sample_t(+0.963776), sample_t(+0.965394), sample_t(+0.966976), sample_t(+0.968522),
    sample_t(+0.970031), sample_t(+0.971504), sample_t(+0.972940), sample_t(+0.974339),
    sample_t(+0.975702), sample_t(+0.977028), sample_t(+0.978317), sample_t(+0.979570),
    sample_t(+0.980785), sample_t(+0.981964), sample_t(+0.983105), sample_t(+0.984210),
    sample_t(+0.985278), sample_t(+0.986308), sample_t(+0.987301), sample_t(+0.988258),
    sample_t(+0.989177), sample_t(+0.990058), sample_t(+0.990903), sample_t(+0.991710),
    sample_t(+0.992480), sample_t(+0.993212), sample_t(+0.993907), sample_t(+0.994565),
    sample_t(+0.995185), sample_t(+0.995767), sample_t(+0.996313), sample_t(+0.996820),
    sample_t(+0.997290), sample_t(+0.997723), sample_t(+0.998118), sample_t(+0.998476),
    sample_t(+0.998795), sample_t(+0.999078), sample_t(+0.999322), sample_t(+0.999529),
    sample_t(+0.999699), sample_t(+0.999831), sample_t(+0.999925), sample_t(+0.999981)
};

// Compute sin/cos from a 32-bit phase accumulator using quadrant symmetry.
// phase_acc[31:30] = quadrant, phase_acc[29:22] = 8-bit LUT index.
static void sincos_lut(ap_uint<32> p, sample_t& sin_v, sample_t& cos_v) {
#pragma HLS INLINE
    ap_uint<2> quad = p(31, 30);
    ap_uint<8> idx  = p(29, 22);
    sample_t s = SIN_LUT[idx];
    sample_t c = SIN_LUT[255 - idx];
    switch (quad.to_uint()) {
        case 0: sin_v =  s; cos_v =  c; break;
        case 1: sin_v =  c; cos_v = -s; break;
        case 2: sin_v = -s; cos_v = -c; break;
        default:sin_v = -c; cos_v =  s; break;
    }
}

// Fixed-point atan2 for CFO estimate.  Returns radians in ap_fixed<16,4>.
static ap_fixed<16,4> sync_atan2(acc_t y, acc_t x) {
#pragma HLS INLINE off
#ifdef __SYNTHESIS__
    const ap_fixed<20,8> ATAN_LUT[16] = {
        0.7853982f, 0.4636476f, 0.2449787f, 0.1243550f,
        0.0624188f, 0.0312398f, 0.0156237f, 0.0078125f,
        0.0039063f, 0.0019532f, 0.0009766f, 0.0004883f,
        0.0002441f, 0.0001221f, 0.0000610f, 0.0000305f
    };
    ap_fixed<20,8> xi = x, yi = y, acc = 0;
    if (xi < 0) { xi = -x; yi = -y; acc = yi < 0 ? ap_fixed<20,8>(-3.141593f) : ap_fixed<20,8>(3.141593f); }
    CORDIC: for (int i = 0; i < 16; i++) {
#pragma HLS PIPELINE II=1
        ap_fixed<20,8> dx = xi >> i, dy = yi >> i;
        if (yi < 0) { xi -= dy; yi += dx; acc -= ATAN_LUT[i]; }
        else        { xi += dy; yi -= dx; acc += ATAN_LUT[i]; }
    }
    return (ap_fixed<16,4>)acc;
#else
    return (ap_fixed<16,4>)::atan2f((float)y, (float)x);
#endif
}

// ── SRL delay-line helper ────────────────────────────────────
// Shift-register delay: returns the sample pushed D iterations ago,
// and pushes 'in' at the head.  With ARRAY_PARTITION complete the
// inner shift loop synthesises to an SRL32 chain on Xilinx — ~1 LUT
// per bit per 32 entries of depth.
template<int D>
static sample_t sr_delay(sample_t sr[D], sample_t in) {
#pragma HLS INLINE
    sample_t out = sr[D - 1];
    SR_SHIFT: for (int k = D - 1; k > 0; k--) {
#pragma HLS UNROLL
        sr[k] = sr[k - 1];
    }
    sr[0] = in;
    return out;
}

// ── Top ─────────────────────────────────────────────────────
void sync_detect(
    hls::stream<iq_t>&  iq_in,
    hls::stream<iq_t>&  iq_out,
    ap_uint<8>          n_syms_fb,
    ap_uint<1>          n_syms_fb_vld,
    ap_ufixed<24,8>     pow_threshold,
    ap_uint<32>&        stat_preamble_count,
    ap_uint<32>&        stat_header_bad_count,
    ap_ufixed<24,8>&    stat_pow_env
) {
#pragma HLS INTERFACE axis        port=iq_in
#pragma HLS INTERFACE axis        port=iq_out
#pragma HLS INTERFACE ap_none     port=n_syms_fb
#pragma HLS INTERFACE ap_none     port=n_syms_fb_vld
#pragma HLS INTERFACE s_axilite   port=pow_threshold         bundle=stat
#pragma HLS INTERFACE s_axilite   port=stat_preamble_count   bundle=stat
#pragma HLS INTERFACE s_axilite   port=stat_header_bad_count bundle=stat
#pragma HLS INTERFACE s_axilite   port=stat_pow_env          bundle=stat
#pragma HLS INTERFACE ap_ctrl_none port=return

    // ── Circular buffer (I and Q in separate BRAMs) ─────────
    static sample_t buf_i[BUF_SIZE];
    static sample_t buf_q[BUF_SIZE];
#pragma HLS BIND_STORAGE variable=buf_i type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=buf_q type=RAM_2P impl=BRAM

    // Power-envelope delay line (64-deep FF register file)
    static acc_t pow_delay[POW_WIN_LEN];
#pragma HLS ARRAY_PARTITION variable=pow_delay complete

    // ── SRL delay lines (replace 3 fixed-offset BRAM reads) ──
    // Total depth: CP_LEN(32) + FFT_SIZE(256) + FFT_SIZE(256) = 544
    // per I/Q component.  ~544 SRL32 LUTs total.
    static sample_t dly_L_i[CP_LEN],    dly_L_q[CP_LEN];     // CP_LEN ago
    static sample_t dly_N_i[FFT_SIZE],  dly_N_q[FFT_SIZE];   // FFT_SIZE ago
    static sample_t dly_LN_i[FFT_SIZE], dly_LN_q[FFT_SIZE];  // CP_LEN+FFT_SIZE ago
#pragma HLS ARRAY_PARTITION variable=dly_L_i   complete
#pragma HLS ARRAY_PARTITION variable=dly_L_q   complete
#pragma HLS ARRAY_PARTITION variable=dly_N_i   complete
#pragma HLS ARRAY_PARTITION variable=dly_N_q   complete
#pragma HLS ARRAY_PARTITION variable=dly_LN_i  complete
#pragma HLS ARRAY_PARTITION variable=dly_LN_q  complete

    ap_uint<6> pow_idx = 0;

    // Running accumulators (persist across loop iterations via `static`)
    static acc_t P_re = 0, P_im = 0;
    static acc_t R    = 0;
    static acc_t Rl   = 0;
    static acc_t pow_env = 0;

    // FSM state
    static ap_uint<2> state = 0;   // 0=SEARCH 1=FWD_PREHDR 2=WAIT_NSYMS 3=FWD_DATA

    static ap_uint<12> wr_ptr  = 0;
    static ap_uint<12> rd_ptr  = 0;
    static int fwd_remaining   = 0;
    static int warmup          = 0;
    static int deaf_counter    = 0;

    // CFO phase NCO
    static ap_uint<32> phase_acc  = 0;
    static ap_int<32>  phase_step = 0;

    // Registered speculative CFO estimate — computed one iteration
    // ahead so the FSM can grab it as a register-to-register copy
    // without a long combinational chain through atan2+multiply.
    static ap_int<32>  reg_cand_ps = 0;

    // Feedback latch (sticky across WAIT_NSYMS)
    static ap_uint<1>  got_fb        = 0;
    static ap_uint<8>  latched_nsyms = 0;

    // Stats
    static ap_uint<32> preamble_cnt    = 0;
    static ap_uint<32> header_bad_cnt  = 0;

    enum { S_SEARCH = 0, S_FWD_PREHDR = 1, S_WAIT_NSYMS = 2, S_FWD_DATA = 3 };

    FREE_RUN_LOOP_BEGIN
#pragma HLS PIPELINE II=5

        // ── 1. Mandatory sample intake (never back-pressures iq_in) ──
#ifdef __SYNTHESIS__
        iq_t in_s = iq_in.read();
#else
        // csim escape: in hardware iq_in is fed continuously by the ADC FIFO
        // and never starves; in csim the input is finite, so a blocking read
        // would hang the testbench.  Treat a long stretch of empty stream as
        // "drained" and break out cleanly — the FREE_RUN_LOOP_BEGIN macro is
        // already a bounded for-loop in csim, so this just tightens it.
        iq_t in_s;
        {
            bool __drained = true;
            for (int __r = 0; __r < 100000; ++__r) {
                if (iq_in.read_nb(in_s)) { __drained = false; break; }
                std::this_thread::yield();
            }
            if (__drained) break;
        }
#endif
        sample_t s_i = in_s.i;
        sample_t s_q = in_s.q;

        // ── 2. Delay-line reads (SRL shift registers) ────────
        //    Replace the old BRAM reads at 3 fixed offsets from wr_ptr.
        //    dly_L  → CP_LEN ago;   dly_N  → FFT_SIZE ago;
        //    dly_LN → CP_LEN+FFT_SIZE ago (chained from dly_L output).
        sample_t rO_i  = sr_delay<CP_LEN>(dly_L_i,  s_i);
        sample_t rO_q  = sr_delay<CP_LEN>(dly_L_q,  s_q);
        sample_t rN_i  = sr_delay<FFT_SIZE>(dly_N_i,  s_i);
        sample_t rN_q  = sr_delay<FFT_SIZE>(dly_N_q,  s_q);
        sample_t rON_i = sr_delay<FFT_SIZE>(dly_LN_i, rO_i);
        sample_t rON_q = sr_delay<FFT_SIZE>(dly_LN_q, rO_q);

        // ── 3. Write the new sample into the circular buffer ──
        //    BRAM now serves FWD replay reads only (1W + 1R = RAM_2P).
        buf_i[(int)(ap_uint<12>)wr_ptr] = s_i;
        buf_q[(int)(ap_uint<12>)wr_ptr] = s_q;

        // ── 4. Speculative CFO estimate (runs every cycle) ────
        //   atan2 + multiply run speculatively; the result is stored
        //   into reg_cand_ps at the END of this iteration.  When the
        //   FSM fires (step 9), it reads reg_cand_ps which holds the
        //   PREVIOUS iteration's result — a register, not a 12 ns
        //   combinational chain.  1-sample staleness is negligible.
        ap_fixed<16,4> ang = sync_atan2(P_im, P_re);
        ap_int<16> ang_raw_s = (ap_int<16>)ang.range();
        ap_int<32> candidate_phase_step = (ap_int<32>)(
            ((ap_int<40>)ang_raw_s * (ap_int<24>)TWO24_OVER_2PI) >> 12);

        // ── 5. Dual-threshold detection (on registered accumulators) ──
        //   Computed BEFORE the accumulator update (step 6) so inputs
        //   are the registered values from the previous iteration —
        //   pure register → multiply → compare, no combinational path
        //   from the accumulator add/sub logic.  1-sample detection
        //   lag is negligible (1/32 of the CP window).
        prod_t P_magsq = (prod_t)(P_re * P_re + P_im * P_im);
        prod_t R_Rl    = (prod_t)R * (prod_t)Rl;
        prod_t thresh  = (prod_t)(R_Rl * SC_TH_SQ_CONST);
        bool sc_above  = (P_magsq > thresh) && (Rl > acc_t(0)) && (R > acc_t(0));
        bool pow_above = pow_env > (acc_t)pow_threshold;

        // ── 6. Update running sums (after threshold snapshot) ───
        //   Live in SEARCH; frozen in FWD_PREHDR / WAIT_NSYMS / FWD_DATA.
        bool metric_live = (state == S_SEARCH);

        // Instantaneous powers
        acc_t new_pow   = (acc_t)(s_i * s_i + s_q * s_q);
        acc_t newN_pow  = (acc_t)(rN_i * rN_i + rN_q * rN_q);
        acc_t oldL_pow  = (acc_t)(rO_i * rO_i + rO_q * rO_q);
        acc_t oldN_pow  = (acc_t)(rON_i * rON_i + rON_q * rON_q);

        // Complex cross-product new × conj(new_N)
        acc_t new_P_re = (acc_t)(s_i * rN_i + s_q * rN_q);
        acc_t new_P_im = (acc_t)(s_q * rN_i - s_i * rN_q);
        // Departing term: old × conj(old_N)
        acc_t old_P_re = (acc_t)(rO_i * rON_i + rO_q * rON_q);
        acc_t old_P_im = (acc_t)(rO_q * rON_i - rO_i * rON_q);

        // Power envelope — always live (needed for trigger and readback)
        acc_t pow_out = pow_delay[(int)pow_idx];
        pow_delay[(int)pow_idx] = new_pow;
        pow_idx = (pow_idx + 1) & (POW_WIN_LEN - 1);
        pow_env = pow_env + new_pow - pow_out;

        if (metric_live) {
            P_re = P_re + new_P_re - old_P_re;
            P_im = P_im + new_P_im - old_P_im;
            R    = R    + newN_pow - oldN_pow;
            Rl   = Rl   + new_pow  - oldL_pow;
        }

        stat_pow_env = (ap_ufixed<24,8>)pow_env;

        // ── 7. Warmup + deaf window ─────────────────────────
        if (warmup < BUF_SIZE) warmup++;
        if (deaf_counter > 0)  deaf_counter--;
        bool gate_armed = (warmup >= BUF_SIZE) && (deaf_counter == 0);

        // ── 8. Sticky-latch the n_syms_fb strobe ────────────
        if (n_syms_fb_vld) {
            latched_nsyms = n_syms_fb;
            got_fb = 1;
        }

        // ── 9. FSM transitions ──────────────────────────────
        iq_t out_s; out_s.i = 0; out_s.q = 0; out_s.last = 0;
        bool want_emit = false;

        switch (state) {
        case S_SEARCH:
            if (gate_armed && sc_above && pow_above) {
                rd_ptr = (ap_uint<12>)(wr_ptr - CP_LEN - FFT_SIZE);
                phase_step = reg_cand_ps;   // register → register (no combo chain)
                phase_acc = 0;
                fwd_remaining = (int)SYNC_NL * 2;   // 576 = preamble + header
                state = S_FWD_PREHDR;
                preamble_cnt++;
                stat_preamble_count = preamble_cnt;
            }
            break;

        case S_FWD_PREHDR:
        case S_FWD_DATA: {
            sample_t rd_i = buf_i[(int)(ap_uint<12>)rd_ptr];
            sample_t rd_q = buf_q[(int)(ap_uint<12>)rd_ptr];
            sample_t c, s;
            sincos_lut(phase_acc, s, c);
            // r_out = r_in × e^{-j·phase}  → derotation
            out_s.i = (sample_t)(rd_i * c + rd_q * s);
            out_s.q = (sample_t)(rd_q * c - rd_i * s);
            out_s.last = 0;
            want_emit = true;
            break;
        }

        case S_WAIT_NSYMS:
            if (got_fb) {
                got_fb = 0;
                if (latched_nsyms > 0) {
                    fwd_remaining = (int)latched_nsyms * (int)SYNC_NL;
                    state = S_FWD_DATA;
                } else {
                    // Header error — accumulators are contaminated; reset
                    // and enter the 288-cycle deaf window before re-arming.
                    header_bad_cnt++;
                    stat_header_bad_count = header_bad_cnt;
                    P_re = 0; P_im = 0; R = 0; Rl = 0; pow_env = 0;
                    POW_CLR_HDR: for (int k = 0; k < POW_WIN_LEN; k++) {
#pragma HLS UNROLL
                        pow_delay[k] = 0;
                    }
                    deaf_counter = (int)SYNC_NL;
                    state = S_SEARCH;
                }
            }
            break;
        }

        // ── 10. Back-pressure-safe output emit ──────────────
        if (want_emit) {
            bool written = iq_out.write_nb(out_s);
            if (written) {
                rd_ptr = (ap_uint<12>)(rd_ptr + 1);
                phase_acc = phase_acc + (ap_uint<32>)phase_step;
                fwd_remaining--;
                if (fwd_remaining == 0) {
                    if (state == S_FWD_PREHDR) {
                        state = S_WAIT_NSYMS;
                    } else {
                        // FWD_DATA done: reset accumulators, open deaf window,
                        // return to SEARCH.
                        P_re = 0; P_im = 0; R = 0; Rl = 0; pow_env = 0;
                        POW_CLR_DATA: for (int k = 0; k < POW_WIN_LEN; k++) {
#pragma HLS UNROLL
                            pow_delay[k] = 0;
                        }
                        deaf_counter = (int)SYNC_NL;
                        state = S_SEARCH;
                    }
                }
            }
            // If not written (downstream full), rd_ptr / fwd_remaining /
            // phase_acc stay put.  wr_ptr still advances below — input
            // samples continue landing in circ_buf.
        }

        // ── 11. Register the speculative CFO for next iteration ──
        reg_cand_ps = candidate_phase_step;

        // ── 12. Always advance the write pointer ────────────
        wr_ptr = (ap_uint<12>)(wr_ptr + 1);

    FREE_RUN_LOOP_END
}
