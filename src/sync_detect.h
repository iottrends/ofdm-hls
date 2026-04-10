// ============================================================
// sync_detect.h  —  Schmidl-Cox Timing Sync + CFO Estimator
//
// Sits in front of ofdm_rx in the RX chain:
//
//   ADC stream → sync_detect → cfo_correct → ofdm_rx → viterbi_dec
//
// Algorithm: normalized CP correlation timing metric + CFO
//
// Timing (normalized Schmidl-Cox CP metric):
//   For each t ∈ [0, SEARCH_WIN):
//     P(t) = Σ_{k=0}^{L-1} r[t+k] · conj(r[t+k+N])   CP correlation
//     R(t) = Σ_{k=0}^{L-1} |r[t+k+N]|²               energy of delayed window
//     metric(t) = |P(t)|² / R(t)²    ∈ [0, 1]
//   best_t = argmax metric(t) over SEARCH_WIN.
//
//   Normalizing by R(t)² removes signal-power dependence:
//     Guard region  → R ≈ noise, P ≈ 0 → metric ≈ 0 (works on real ADC noise)
//     Symbol CP     → P ≈ R            → metric → 1
//   No threshold to calibrate — hardware-deployable as-is.
//
//   With one guard symbol prepended (SYNC_NL zeros), the preamble CP is
//   at t = 288 and the first data CP boundary at t = 576 = SEARCH_WIN
//   (outside the window).  Global max within SEARCH_WIN is unambiguously
//   the preamble.
//
// CFO estimate (fractional, in subcarrier units):
//   P(best_t) accumulated during the search — no second pass needed.
//   ε_sc = angle(P(best_t)) / (2π),  |ε_sc| ≤ 0.5  (CP-based limit)
//
// CP plateau behaviour:
//   The normalized metric rises over ~CP_LEN samples as the correlation
//   window enters the CP region.  E[metric|noise] ≈ 1/CP_LEN ≈ 0.03.
//   SC_THRESHOLD = 0.7 triggers ~3–5 samples before the exact CP/data
//   boundary — consistently within the CP guard window (CP_LEN=32).
//   The constant offset is absorbed by the preamble channel estimate
//   (both preamble and data symbols see the same cyclic shift → G_eq
//   compensates → BER unaffected).
//
// In simulation (guard zeros + preamble at t=SYNC_NL):
//   best_t = SYNC_NL = 288,  CFO ≈ 0 → cfo_correct is pass-through.
//
// Resource estimate: ~2 BRAM_18K (864-sample search buffer)
// ============================================================
#pragma once
#include "ofdm_rx.h"

#define SYNC_NL      (FFT_SIZE + CP_LEN)        // 288 — one OFDM symbol
#define SEARCH_WIN   (2 * SYNC_NL)              // 576 — search window depth
#define SYNC_BUF_SZ  (SEARCH_WIN + SYNC_NL)    // 864 — buffer: window + one more symbol
                                                 //   needed so candidate t=SEARCH_WIN-1
                                                 //   can access r[t+CP_LEN-1+FFT_SIZE]

// Fractional CFO in subcarrier units: ε_sc = Δf / Δf_sc,  |ε_sc| ≤ 0.5
// ap_fixed<16,2>: range ±1, resolution 2^-14 ≈ 61 µHz/Hz (relative)
typedef ap_fixed<16,2> cfo_t;

// Maximum data symbols per packet (must match MAX_DATA_SYMS in TX)
#define MAX_DATA_SYMS 255

void sync_detect(
    hls::stream<iq_t>& iq_in,
    hls::stream<iq_t>& iq_out,
    cfo_t&             cfo_est,   // fractional CFO output (subcarrier units)
    ap_uint<8>         n_syms     // number of data symbols (preamble not counted)
);
