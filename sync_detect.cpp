// ============================================================
// sync_detect.cpp  —  Schmidl-Cox Timing Sync + CFO Estimator
//
// Operation
// ─────────
// Phase 1 (FILL): read SYNC_BUF_SZ = 864 samples into BRAM buffer.
//
// Phase 2 (SEARCH): for each t = 0..SEARCH_WIN-1, compute the normalized
//   CP timing metric:
//     P(t)      = Σ_{k=0}^{L-1} r[t+k] · conj(r[t+k+N])   complex
//     R(t)      = Σ_{k=0}^{L-1} |r[t+k+N]|²                energy
//     metric(t) = |P(t)|² / R(t)²                           ∈ [0,1]
//   best_t = argmax metric(t).  P(best_t) retained for CFO.
//   Normalization removes ADC noise floor dependence — no threshold needed.
//
// Phase 3 (OUTPUT): stream (n_syms+1)×SYNC_NL samples starting from best_t.
//   First portion: replayed from BRAM buffer.
//   Remainder: forwarded live from iq_in.
//
// CFO: ε_sc = angle(P(best_t)) / (2π)  [subcarrier units, |ε_sc| ≤ 0.5]
//
// Synthesis notes:
//   buf_i / buf_q: RAM_2P BRAM — two reads per cycle (ports A and B used
//   simultaneously for r[t+k] and r[t+k+N] in the CORR inner loop).
//   SEARCH outer loop: sequential, no conditional break.
//   CORR inner loop: II=1 (loop-carried ap_fixed adder, 1-cycle latency).
// ============================================================
#include "sync_detect.h"
#include <hls_math.h>

#ifndef M_PI_F
#define M_PI_F 3.14159265358979323846f
#endif

void sync_detect(
    hls::stream<iq_t>& iq_in,
    hls::stream<iq_t>& iq_out,
    cfo_t&             cfo_est,
    ap_uint<8>         n_syms
)
{
    // ── Search buffer ─────────────────────────────────────────
    // Stores the first SYNC_BUF_SZ samples so both the search
    // and the output replay can access them without re-reading the stream.
    sample_t buf_i[SYNC_BUF_SZ];
    sample_t buf_q[SYNC_BUF_SZ];
#pragma HLS bind_storage variable=buf_i type=RAM_2P impl=BRAM
#pragma HLS bind_storage variable=buf_q type=RAM_2P impl=BRAM

    // ── Phase 1: Fill buffer ──────────────────────────────────
    FILL: for (int n = 0; n < SYNC_BUF_SZ; n++) {
#pragma HLS pipeline II=1
#pragma HLS loop_tripcount min=SYNC_BUF_SZ max=SYNC_BUF_SZ
        iq_t s  = iq_in.read();
        buf_i[n] = s.i;
        buf_q[n] = s.q;
    }

    // ── Phase 2: Normalized CP timing metric — global maximum ────
    //
    // For each candidate timing offset t ∈ [0, SEARCH_WIN):
    //
    //   P(t) = Σ_{k=0}^{L-1} r[t+k] · conj(r[t+k+N])     CP correlation
    //   R(t) = Σ_{k=0}^{L-1} |r[t+k+N]|²                 energy of delayed window
    //   metric(t) = |P(t)|² / R(t)²                        ∈ [0, 1]
    //
    // Normalization removes signal-power dependence.
    // metric = 1.0 at the CP boundary, ≈ 0 in noise/guard regions.
    //
    // Global maximum (not first-above-threshold):
    //   First-above-threshold fires on the RISING EDGE of the metric plateau,
    //   typically 2–4 samples before the true CP boundary, causing a
    //   timing offset that misaligns every subsequent symbol.
    //   Global max correctly locks at the peak (t = SYNC_NL = 288 with
    //   one guard symbol prepended).
    //
    //   SEARCH_WIN = 2 × SYNC_NL excludes the first data symbol CP
    //   (which starts at t = SEARCH_WIN), so the global max within
    //   the window is unambiguously the preamble.
    //
    // Full SEARCH_WIN scan always performed (no early exit):
    //   outer SEARCH loop is sequential; inner CORR pipelines at II=1.

    int best_t = 0;
    float best_metric = -1.0f;
    ap_fixed<32,8> best_Pi = 0, best_Pq = 0;

    SEARCH: for (int t = 0; t < SEARCH_WIN; t++) {
#pragma HLS loop_tripcount min=SEARCH_WIN max=SEARCH_WIN
        ap_fixed<32,8> Pi = 0, Pq = 0, R = 0, Rl = 0;

        CORR: for (int k = 0; k < CP_LEN; k++) {
#pragma HLS PIPELINE II=1
#pragma HLS loop_tripcount min=CP_LEN max=CP_LEN
            ap_fixed<32,2> ri = buf_i[t + k];
            ap_fixed<32,2> rq = buf_q[t + k];
            ap_fixed<32,2> si = buf_i[t + k + FFT_SIZE];
            ap_fixed<32,2> sq = buf_q[t + k + FFT_SIZE];
            Pi += ri * si + rq * sq;   // Re(r · conj(s))
            Pq += rq * si - ri * sq;   // Im(r · conj(s))
            R  += si * si + sq * sq;   // |s|²  right window
            Rl += ri * ri + rq * rq;   // |r|²  left window
        }

        // Symmetric normalization: |P|² / (Rl × R) ∈ [0,1] by Cauchy-Schwarz.
        // Asymmetric ÷R² exceeds 1.0 when windows have unequal energy
        // (e.g. dense ZC preamble body vs sparse header symbol → false peak).
        float Pi_f = (float)Pi;
        float Pq_f = (float)Pq;
        float R_f  = (float)R;
        float Rl_f = (float)Rl;
        float M    = Pi_f * Pi_f + Pq_f * Pq_f;
        float metric = (R_f > 1e-6f && Rl_f > 1e-6f) ? (M / (Rl_f * R_f)) : 0.0f;

        // Track global maximum
        if (metric > best_metric) {
            best_metric = metric;
            best_t      = t;
            best_Pi     = Pi;
            best_Pq     = Pq;
        }
    }


    // ── CFO estimate ─────────────────────────────────────────
    // angle(P_best) = 2π × Δf × N/Fs = 2π × ε_sc
    // ε_sc = angle(P_best) / (2π),   |ε_sc| ≤ 0.5
    // hls::atan2f maps to a CORDIC in synthesis.
    float ang = hls::atan2f((float)best_Pq, (float)best_Pi);
    cfo_est = cfo_t(ang / (2.0f * M_PI_F));

    // ── Phase 3: Stream output aligned to detected timing ────
    // Total output: (n_syms + 2) symbols = preamble + header + data
    const int total_output = ((int)n_syms + 2) * SYNC_NL;

    // How many samples are available in the buffer from best_t onward
    const int buf_avail = SYNC_BUF_SZ - best_t;
    const int from_buf  = (buf_avail < total_output) ? buf_avail : total_output;
    const int from_live = total_output - from_buf;

    OUT_BUF: for (int n = 0; n < from_buf; n++) {
#pragma HLS pipeline II=1
#pragma HLS loop_tripcount min=0 max=SYNC_BUF_SZ
        iq_t s;
        s.i    = buf_i[best_t + n];
        s.q    = buf_q[best_t + n];
        // Set last only if no live phase follows and this is the final sample
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
