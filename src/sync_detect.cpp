// ============================================================
// sync_detect.cpp v5 — Free-running preamble gate (no inline CFO)
//
// One block, one clock, 4-state FSM: SEARCH / FWD_PREHDR / WAIT_NSYMS /
// FWD_DATA.  Algorithm per docs/RX_GATING_DESIGN.md.
//
// CFO derotation removed in v5: the AD9364's own RX-LO calibration leaves
// sub-ppm residual carrier offset, and ofdm_rx already runs a per-symbol
// pilot CPE tracker (compute_pilot_cpe) that absorbs whatever drift gets
// through.  At low SNR the noise on a single-window CP correlation drove
// a spurious phase_step that tilted the constellation enough to flip
// header bits — the cure is worse than the disease.  The atan2 CORDIC,
// 256-entry sin LUT, NCO accumulator, and complex derotator multiply are
// all gone; FWD states now pass buffered samples through unchanged.  If
// a future RFIC needs in-PHY CFO compensation it can be reintroduced
// behind a CSR / compile flag.
//
// Control: ap_ctrl_none, while(1) body.  ap_start is not driven by
// anyone — block auto-starts after reset.
//
// Resource budget (target Artix-50T at 10 ns):
//   BRAM18: ~8 (circular buffer 4096 × 32 bit for FWD replay)
//   DSP48 : ~14 (running sums + threshold cross-mult; CFO mults dropped)
//   SRL   : ~544 LUTs (delay lines for sliding-window reads)
//   LUT   : ~1.5k (excluding SRLs)
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
// <thread> for the csim drain-read polling lives in free_run.h's DRAIN_READ_OR macro.

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

    // (CFO NCO state removed in v5 — derotation lives in pilot CPE inside ofdm_rx.)

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
        // In hardware iq_in is fed continuously by the ADC FIFO and never
        // starves; in csim the input is finite, so DRAIN_READ_OR returns
        // from sync_detect once the testbench has drained the stream.
        // (`return` instead of `break` because the macro's do/while wrapper
        //  would swallow a `break` — see free_run.h.)
        iq_t in_s;
        DRAIN_READ_OR(iq_in, in_s, return);
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

        // ── 4. (CFO speculative-estimate block removed in v5.) ──

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
                fwd_remaining = (int)SYNC_NL * 2;   // 576 = preamble + header
                state = S_FWD_PREHDR;
                preamble_cnt++;
                stat_preamble_count = preamble_cnt;
            }
            break;

        case S_FWD_PREHDR:
        case S_FWD_DATA: {
            // v5: pass-through — pilot CPE in ofdm_rx handles residual phase.
            out_s.i = buf_i[(int)(ap_uint<12>)rd_ptr];
            out_s.q = buf_q[(int)(ap_uint<12>)rd_ptr];
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
            // If not written (downstream full), rd_ptr / fwd_remaining stay
            // put.  wr_ptr still advances below — input samples continue
            // landing in circ_buf.
        }

        // ── 11. Always advance the write pointer ────────────
        wr_ptr = (ap_uint<12>)(wr_ptr + 1);

    FREE_RUN_LOOP_END
}
