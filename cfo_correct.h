// ============================================================
// cfo_correct.h  —  Per-Sample CFO Phase Rotation
//
// Sits between sync_detect and ofdm_rx:
//
//   sync_detect → cfo_correct → ofdm_rx
//
// Applies the correction:
//   r_out[n] = r_in[n] × e^{−j·2π·ε_sc·n / N}
//
// where ε_sc is the fractional CFO in subcarrier units from sync_detect,
// and N = FFT_SIZE = 256.
//
// Phase increment per sample: Δφ = 2π × ε_sc / N
//
// C-sim: uses hls::sincosf (CORDIC in synthesis)
// For zero CFO (ε_sc = 0): Δφ = 0, block is a lossless pass-through.
//
// Resource estimate (synthesis): ~3 DSP (complex multiply), 1 CORDIC
// ============================================================
#pragma once
#include "sync_detect.h"   // brings in iq_t, cfo_t, SYNC_NL, FFT_SIZE

void cfo_correct(
    hls::stream<iq_t>& iq_in,
    hls::stream<iq_t>& iq_out,
    cfo_t              cfo_est,   // from sync_detect (subcarrier units)
    ap_uint<8>         n_syms     // data symbols (preamble not counted)
);
