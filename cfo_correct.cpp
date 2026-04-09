// ============================================================
// cfo_correct.cpp  —  Per-Sample CFO Phase Rotation
//
// Corrects carrier frequency offset estimated by sync_detect.
//
// For each sample n (0 = first sample of the aligned preamble):
//   phase[n] = −n × Δφ      (negative: undo the CFO advance)
//   out[n]   = in[n] × exp(j·phase[n])
//
//   out_i =  in_i·cos(phase) + in_q·sin(phase)
//   out_q = −in_i·sin(phase) + in_q·cos(phase)
//
// Phase accumulator (NCO):
//   Instead of computing n × Δφ (float multiply every sample), a 32-bit
//   unsigned phase accumulator is decremented by a fixed step each cycle:
//     phase_acc -= delta_fixed   (1-cycle ap_uint subtract, II=1 guaranteed)
//   The full ap_uint<32> range [0, 2^32) maps to one period [0, 2π).
//   Natural unsigned overflow IS the correct modulo-2π reduction — no
//   explicit range-reduction needed.  Resolution: 2π/2^32 ≈ 1.46 nrad/step,
//   giving 32 fractional bits (> the 24-bit minimum for 73 k-sample frames).
//
//   Accumulated error over 73,728 samples at max CFO (0.5 SC):
//     Δ = 2π/2^32 × 73728 ≈ 0.11 mrad — well within 16-QAM margin (~0.26 rad)
//
// hls::sincosf maps to Vivado float-CORDIC IP (~15-cycle latency, pipelined
// at II=1 — sustained 1 sample/cycle with HLS retiming).
// Future: replace with a fixed-point CORDIC (~6 DSPs) to reduce DSP usage
// from ~20–30 to ~6.
//
// Zero-CFO case (simulation): delta_fixed = 0, phase_acc = 0 every sample,
// cos=1, sin=0 → block is a perfect pass-through.
// ============================================================
#include "cfo_correct.h"
#include <hls_math.h>

#ifndef M_PI_F
#define M_PI_F 3.14159265358979323846f
#endif

void cfo_correct(
    hls::stream<iq_t>& iq_in,
    hls::stream<iq_t>& iq_out,
    cfo_t              cfo_est,
    ap_uint<8>         n_syms
)
{
    // Total samples: preamble + n_syms data symbols
    const int total_samples = ((int)n_syms + 2) * SYNC_NL;  // preamble + header + data

    // Minimum detectable CFO: below this threshold the estimate is dominated
    // by CP-correlation noise rather than a real frequency offset.
    //
    // Why this matters: a noise-induced error of ε_noise ≈ 0.006 sc accumulates
    // to 73728 × 2π × 0.006 / 256 ≈ 11 radians over a 255-symbol frame, which
    // destroys equalization on later symbols.
    //
    // Threshold = 0.01 sc ≈ 781 Hz:
    //   Simulation (no real CFO, noise ≈ 0.006 sc) → suppressed → pass-through
    //   Hardware AD9364 (crystal offset ≥ ±10 ppm at 2.4 GHz ≈ ±24 kHz = ±0.3 sc)
    //     → well above threshold → correction applied ✓
    const float cfo_float   = (float)cfo_est;
    const float cfo_min_sc  = 0.01f;
    const float eff_cfo     = (cfo_float > cfo_min_sc || cfo_float < -cfo_min_sc)
                               ? cfo_float : 0.0f;

    // Fixed-point phase accumulator (NCO).
    // delta_fixed: phase step per sample in ap_uint<32> units.
    //   Δφ = eff_cfo × 2π / N  rad/sample
    //   delta_fixed = Δφ × 2^32 / (2π) = eff_cfo × 2^32 / N
    // Using double for this one-time conversion avoids float precision loss
    // at the 2^32 scale (2^32 > 2^24 float mantissa).
    // Route through int32_t to avoid UB: casting negative double → ap_uint<32>
    // directly is undefined in C++.  (int32_t) of a negative double is defined
    // (truncate toward zero); (ap_uint<32>) of a negative int32_t is defined
    // (two's complement modulo 2^32).  Negative eff_cfo → large unsigned step
    // → phase_acc increments (negative rotation) — correct.
    ap_uint<32> delta_fixed = (ap_uint<32>)(int32_t)((double)eff_cfo / (double)FFT_SIZE
                                                     * 4294967296.0);
    ap_uint<32> phase_acc = 0;

    CORRECT: for (int n = 0; n < total_samples; n++) {
#pragma HLS pipeline II=1
#pragma HLS loop_tripcount min=SYNC_NL max=((MAX_DATA_SYMS+1)*SYNC_NL)
        iq_t s = iq_in.read();

        // Decrement accumulator (undo CFO advance).
        // ap_uint<32> wraps at 2^32 — this IS correct modulo-2π wrap.
        phase_acc -= delta_fixed;

        // Map [0, 2^32) → (−π, +π]: reinterpret bits as int32, scale by π/2^31.
        float phase_f = (float)(ap_int<32>)phase_acc * (M_PI_F / 2147483648.0f);

        float cos_p, sin_p;
        hls::sincosf(phase_f, &sin_p, &cos_p);

        float in_i = (float)s.i;
        float in_q = (float)s.q;

        // Rotation by −phase_f (corrects the CFO phase advance):
        //   [out_i]   [ cos  sin] [in_i]
        //   [out_q] = [−sin  cos] [in_q]
        iq_t out;
        out.i    = sample_t(in_i * cos_p + in_q * sin_p);
        out.q    = sample_t(-in_i * sin_p + in_q * cos_p);
        out.last = s.last;
        iq_out.write(out);
    }
}
