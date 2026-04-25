// ============================================================
// ofdm_rx.cpp  —  OFDM Receiver (Vitis HLS)
// ============================================================
#include "ofdm_rx.h"
#include "ofdm_lut.h"   // PILOT_IDX, DATA_SC_IDX, ZC_I/Q_LUT, CORDIC_ATAN_VALUES, crc16_hdr
#include "free_run.h"   // DRAIN_READ_OR (csim escape for blocking iq_in.read)
#include <cmath>
#ifndef __SYNTHESIS__
#include <iostream> // diagnostic dumps in csim
// <thread> for the csim drain-read polling lives in free_run.h's DRAIN_READ_OR macro.
#endif

// ============================================================
// Internal equalized-channel type
//
// geq_t / cgeq_t hold precomputed G_eq = conj(G)/|G|² values.
//   With scale_sch=0xAA (RX FFT ÷256): G ≈ 1/256, G_eq ≈ 256.
//   ap_fixed<32,10>: range [-512, +512), 22 fractional bits.
//   Holds G_eq ≈ 256 with sufficient precision.
// ============================================================
typedef ap_fixed<32, 10>         geq_t;
typedef std::complex<geq_t>     cgeq_t;

// CORDIC angle table  atan(2^-i)  for i = 0..15, in ap_fixed<32,4>.
// Single source of truth shared by fixed_atan2_rx (vectoring mode)
// and cordic_rotate_rx (rotation mode).  Each .cpp that includes
// ofdm_lut.h gets its own private ROM instance — synthesises once
// per RTL module just like the old in-function locals.
static const ap_fixed<32,4> ATAN_LUT[16] = CORDIC_ATAN_VALUES;

// Fixed-point atan2 via 16-iteration CORDIC vectoring mode.
// Returns phase in ap_fixed<32,4> (radians, range [-π, +π)).
// No floating-point — maps to DSP48 shift-add chains.
static ap_fixed<32,4> fixed_atan2_rx(geq_t y_in, geq_t x_in) {
    #pragma HLS INLINE
    typedef ap_fixed<32,4> ang_t;
    typedef ap_fixed<32,12> xy_t;  // enough range for G_eq (~±256 → 9 int bits + margin)

    xy_t x = (xy_t)x_in;
    xy_t y = (xy_t)y_in;
    ang_t angle = ang_t(0);

    // Rotate to Q1 so CORDIC starts with x > 0
    if (x < xy_t(0)) {
        if (y >= xy_t(0)) { x = -x; y = -y; angle = ang_t( 3.14159265); }
        else               { x = -x; y = -y; angle = ang_t(-3.14159265); }
    }

    CORDIC: for (int i = 0; i < 16; i++) {
        #pragma HLS UNROLL
        xy_t  x_tmp = x >> i;
        xy_t  y_tmp = y >> i;
        ang_t a     = ATAN_LUT[i];
        if (y < xy_t(0)) { x -= y_tmp; y += x_tmp; angle -= a; }
        else              { x += y_tmp; y -= x_tmp; angle += a; }
    }
    return angle;
}

// Fixed-point CORDIC rotation mode (Opt-1).
// Rotates complex input `in` by `angle_rad` using 16 shift-add iterations.
// Zero DSP48 multipliers — pure shift-and-add hardware.
//
// CORDIC gain: output magnitude is scaled by K₁₆ ≈ 1.64676 (cumulative
// gain of 16 rotation stages).  We deliberately do NOT renormalize,
// which would cost DSPs.  Consumers compensate:
//   • qpsk_demap: sign-only compare — positive K has no effect.
//   • BPSK header demap: sign-only compare — positive K has no effect.
//   • qam16_demap: THRESH is scaled by K at compile time — still zero DSPs.
//
// Range caution: geq_t = ap_fixed<32,10> caps at ±512.  Input magnitudes
// above ~310 will overflow on the cast back to geq_t (310 × K ≈ 512).
// In ideal channel G_eq ≈ 256 → 256 × K ≈ 422, well within range.
// Only deep-fade / very low SNR channel estimates can breach this.
static cgeq_t cordic_rotate_rx(cgeq_t in, ap_fixed<32,4> angle_rad) {
    #pragma HLS INLINE
    typedef ap_fixed<32,12> xy_t;   // 12 int bits → holds 256 × K ≈ 422
    typedef ap_fixed<32,4>  ang_t;

    xy_t x = (xy_t)in.real();
    xy_t y = (xy_t)in.imag();
    ang_t target_angle  = angle_rad;
    ang_t current_angle = 0;

    // Pre-rotate by ±π if target is outside CORDIC convergence range [-π/2, +π/2].
    // x = -x, y = -y is an exact 180° rotation (gain = 1, no K contribution).
    if (target_angle > ang_t(1.57079632)) {
        x = -x; y = -y; current_angle = ang_t( 3.14159265);
    } else if (target_angle < ang_t(-1.57079632)) {
        x = -x; y = -y; current_angle = ang_t(-3.14159265);
    }

    // 16-iteration shift-add rotation — zero DSPs.
    CORDIC_ROT: for (int i = 0; i < 16; i++) {
        #pragma HLS UNROLL
        xy_t  x_tmp = x >> i;
        xy_t  y_tmp = y >> i;
        ang_t a     = ATAN_LUT[i];
        if (target_angle > current_angle) {
            x -= y_tmp; y += x_tmp; current_angle += a;
        } else {
            x += y_tmp; y -= x_tmp; current_angle -= a;
        }
    }
    return cgeq_t((geq_t)x, (geq_t)y);
}

// ============================================================
// SECTION 2: Forward FFT
//
// fwd_inv = true → forward DFT
// scale_sch = 0xAA: Radix-4, 4 stages, 10_10_10_10 → ÷4 each → total ÷256
//
// Same schedule as TX IFFT (also 0xAA, ÷256).  Round-trip:
//   x[n] = IDFT{X[k]} / 256   (TX HLS IFFT)
//   Y[k] = DFT{x[n]}  / 256   (RX HLS FFT)
//   DFT{IDFT{X}/256} = (1/256)·N·X = X  →  Y[k] = X[k] / 256
//
// 0xAA is REQUIRED for input-range safety: a Radix-4 butterfly has
// natural gain ×4; dividing by exactly 4 per stage keeps intermediate
// values bounded for ANY input in [-1,+1).
// 0x55 (÷2/stage) allows 2× net growth per stage → 16× overflow for
// full-scale noisy inputs and MUST NOT be used here.
//
// With Y[k] = X[k]/256:  G ≈ 1/256,  G_eq ≈ 256 (fits ap_fixed<32,10>).
// ============================================================
// Stream 256 time-domain samples to the external xfft IP and read back
// 256 freq-domain samples.  The xfft core is configured for 256-pt forward
// FFT with scale_sch=0xAA (÷256 total) via its s_axis_config port in the BD.
static void run_fft(
    csample_t            in[FFT_SIZE],
    csample_t            out[FFT_SIZE],
    hls::stream<iq32_t> &fft_in,
    hls::stream<iq32_t> &fft_out
) {
    #pragma HLS INLINE off

#ifndef __SYNTHESIS__
    // C-sim: radix-2 Cooley-Tukey FFT, matches hardware xfft scale_sch=0xAA (÷N).
    float re[FFT_SIZE], im[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++) {
        re[i] = (float)in[i].real();
        im[i] = (float)in[i].imag();
    }
    // Bit-reversal permutation
    for (int i = 1, j = 0; i < FFT_SIZE; i++) {
        int bit = FFT_SIZE >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t;
            t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    // Butterfly stages — forward: twiddle = exp(-j2π/len)
    for (int len = 2; len <= FFT_SIZE; len <<= 1) {
        float ang = -2.0f * 3.14159265f / len;
        float wre = cosf(ang), wim = sinf(ang);
        for (int i = 0; i < FFT_SIZE; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float ur = re[i+k],           ui = im[i+k];
                float vr = re[i+k+len/2]*cr - im[i+k+len/2]*ci;
                float vi = re[i+k+len/2]*ci + im[i+k+len/2]*cr;
                re[i+k]       = ur + vr;  im[i+k]       = ui + vi;
                re[i+k+len/2] = ur - vr;  im[i+k+len/2] = ui - vi;
                float nr = cr*wre - ci*wim;  ci = cr*wim + ci*wre;  cr = nr;
            }
        }
    }
    // Scale by 1/N to match hardware xfft
    for (int i = 0; i < FFT_SIZE; i++)
        out[i] = csample_t((sample_t)(re[i] / FFT_SIZE), (sample_t)(im[i] / FFT_SIZE));
    (void)fft_in; (void)fft_out;
#else
    // xfft frame length is fixed at IP-build time (FFT_SIZE=256); no input
    // TLAST needed — xfft generates its own output TLAST.
    FFT_TX: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        iq32_t s;
        s.i = in[i].real();
        s.q = in[i].imag();
        fft_in.write(s);
    }

    FFT_RX: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        iq32_t s = fft_out.read();
        out[i] = csample_t(s.i, s.q);
    }
#endif
}

// ============================================================
// SECTION 3: CP Removal
//
// Reads (CP_LEN + FFT_SIZE) samples from stream.
// Discards the first CP_LEN samples (cyclic prefix).
// Stores the remaining FFT_SIZE samples in buf[].
//
// Return value: always true in synthesis (HLS will dead-code-eliminate
// the caller's `if (!...) break;` check).  In csim, returns false if the
// input stream stays empty for a long stretch — signal to the FREE_RUN
// while(1) caller that the testbench has drained and the block should
// exit cleanly.  Synthesis sees only the read; the csim drain logic
// lives entirely under #ifndef __SYNTHESIS__.
// ============================================================
static bool remove_cp_and_read(
    hls::stream<iq_t> &iq_in,
    csample_t          buf[FFT_SIZE]
) {
    #pragma HLS INLINE off
    iq_t s;

    // Discard CP
    SKIP_CP: for (int i = 0; i < CP_LEN; i++) {
        #pragma HLS PIPELINE II=1
        DRAIN_READ_OR(iq_in, s, return false);
        (void)s;
    }

    // Read symbol body into FFT input buffer
    READ_SYM: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        DRAIN_READ_OR(iq_in, s, return false);
        buf[i] = csample_t(s.i, s.q);
    }
    return true;
}

// ============================================================
// SECTION 4: Channel Estimation (with precomputed G_eq)
//
// Two-phase computation:
//
// Phase 1 — Estimate G[d] = Y_pre[k] * conj(ZC[d]) for each data SC:
//   G[d] = Y_pre[k] * conj(ZC[d])
//   In ideal channel: Y_pre[k] = ZC[k]/256, so G[d] ≈ 1/256
//   (real-valued, positive; ZC is unit-magnitude).
//   This loop is PIPELINED → fast, multiple DSP48s in parallel.
//
// Phase 2 — Precompute G_eq[k] = conj(G[d]) / |G[d]|²:
//   G_eq = conj(G) / |G|²
//   In ideal channel: G_eq = (1/256) / (1/256)² = 256
//   This loop has NO pipeline pragma → HLS serializes it → one
//   divider instance is REUSED for all 200 data subcarriers.
//   This is the key resource fix (was 200× dividers before).
//
// Phase 3 — G_eq for pilot SCs (preamble pilots are known +1):
//   G_pilot[p] = Y_pre[PILOT_IDX[p]] × conj(+1) = Y_pre[PILOT_IDX[p]]
//   G_eq at pilot indices is filled by the same conj/|.|² formula.
//   Used per-symbol for CPE phase tracking.
//
// Equalization in hot path then becomes multiply-only:
//   X_hat[k] = Y_data[k] * G_eq[k]
//   In ideal: (X/256) * 256 = X ✓
// ============================================================
static void estimate_channel(
    csample_t  fft_pre[FFT_SIZE],
    cgeq_t     G_eq[FFT_SIZE]       // precomputed conj(G)/|G|², ≈256 ideal
) {
    #pragma HLS INLINE off
    typedef ap_fixed<32, 10> wide_t;  // range [-512,+512): holds G_eq ≈ 256 (ideal)

    // Temporary G storage (data SCs only, indexed by d not k)
    csample_t G[NUM_DATA_SC];

    // Phase 1: compute G[d] = Y_pre[k] * conj(ZC[d])
    CH_DATA: for (int d = 0; d < NUM_DATA_SC; d++) {
        #pragma HLS PIPELINE II=1
        int k = DATA_SC_IDX[d];
        wide_t y_re = (wide_t)fft_pre[k].real();
        wide_t y_im = (wide_t)fft_pre[k].imag();
        wide_t z_re = (wide_t)ZC_I_LUT[d];
        wide_t z_im = (wide_t)ZC_Q_LUT[d];
        // Y * conj(ZC): (y_re*z_re + y_im*z_im) + j*(y_im*z_re - y_re*z_im)
        G[d] = csample_t(sample_t(y_re * z_re + y_im * z_im),
                         sample_t(y_im * z_re - y_re * z_im));
    }

    // Zero G_eq array
    CH_ZERO: for (int k = 0; k < FFT_SIZE; k++) {
        #pragma HLS PIPELINE II=1
        G_eq[k] = cgeq_t(geq_t(0), geq_t(0));
    }

    // Phase 2: G_eq[k] = conj(G[d]) / |G[d]|²
    // Compute ONE reciprocal per SC (1 division), then two multiplies.
    // Halves divider count vs original 2 divisions per SC.
    // No PIPELINE → HLS serializes the division → one divider instance reused.
    //
    // inv_t: wider type for the reciprocal.
    //   denom ≈ (1/256)² = 1/65536  →  inv ≈ 65536.
    //   wide_t = ap_fixed<32,10> only holds ±512 → overflow at 65536.
    //   ap_fixed<40,20> holds ±524288 → no overflow.
    //   Final G_eq = g_re × inv ≈ (1/256) × 65536 = 256 → fits geq_t<32,10>.
    typedef ap_fixed<40,20> inv_t;

    CH_EQ: for (int d = 0; d < NUM_DATA_SC; d++) {
        int k = DATA_SC_IDX[d];
        inv_t g_re = (inv_t)G[d].real();
        inv_t g_im = (inv_t)G[d].imag();
        inv_t denom = g_re * g_re + g_im * g_im;
        if (denom == inv_t(0)) {
            G_eq[k] = cgeq_t(geq_t(0), geq_t(0));
        } else {
            inv_t inv = inv_t(1.0) / denom;   // one division → one divider instance
            G_eq[k] = cgeq_t(geq_t(g_re * inv), geq_t(-g_im * inv));
        }
    }

    // Phase 3: G_eq for pilot SCs (same reciprocal pattern).
    CH_PILOT: for (int p = 0; p < NUM_PILOT_SC; p++) {
        int k = PILOT_IDX[p];
        inv_t y_re = (inv_t)fft_pre[k].real();
        inv_t y_im = (inv_t)fft_pre[k].imag();
        inv_t denom = y_re * y_re + y_im * y_im;
        if (denom == inv_t(0)) {
            G_eq[k] = cgeq_t(geq_t(0), geq_t(0));
        } else {
            inv_t inv = inv_t(1.0) / denom;
            G_eq[k] = cgeq_t(geq_t(y_re * inv), geq_t(-y_im * inv));
        }
    }
}

// ============================================================
// SECTION 5: Single-Subcarrier Equalizer (multiply-only)
//
// Computes X_hat = Y * G_eq  where G_eq = conj(G)/|G|²
// was precomputed once during preamble processing (Section 4).
//
// This replaces the original Y*conj(G)/|G|² division with a
// complex multiply.  In ideal channel:
//   Y ≈ X/256,  G_eq ≈ 256  →  X_hat = (X/256)*256 = X ✓
//
// Resource saving: no combinational dividers in the hot path.
// HLS maps each multiply to a DSP48; the loop can be fully
// pipelined without triggering 200× divider instantiation.
// ============================================================
static cgeq_t equalize_sc(csample_t Y, cgeq_t G_eq) {
    #pragma HLS INLINE
    typedef ap_fixed<32, 10> acc_t;  // range [-512,+512): holds noisy equalized value ≈ ±1.4

    acc_t y_re = (acc_t)Y.real();
    acc_t y_im = (acc_t)Y.imag();
    acc_t g_re = (acc_t)G_eq.real();
    acc_t g_im = (acc_t)G_eq.imag();

    // Complex multiply: (y_re + j*y_im) * (g_re + j*g_im)
    acc_t x_re = y_re * g_re - y_im * g_im;
    acc_t x_im = y_re * g_im + y_im * g_re;

    // Return as cgeq_t (ap_fixed<32,10>) — do NOT cast down to sample_t here.
    // With noise, x_re can exceed ±1.0 and would wrap in ap_fixed<16,1>.
    // The demapper only needs sign/threshold comparison, not a narrow type.
    return cgeq_t(geq_t(x_re), geq_t(x_im));
}

// ============================================================
// SECTION 5b: Per-Symbol Equalizer-Rotator Precompute (CORDIC)
//
// Algebraic regrouping of  x̂ = (Y · G_eq) · e^(−jφ_s)  ≡  Y · (G_eq · e^(−jφ_s))
// where φ_s is the per-symbol CPE phase error from the 6 pilots.
//
// Runs once per data symbol: for every active subcarrier k (200 data +
// 6 pilots = 206 SCs), rotates G_eq[k] by −φ_s via CORDIC and stores
// the result in G_eq_final[k].  The hot demap loop then needs only ONE
// complex multiply per SC  (Y · G_eq_final)  instead of two
// (Y · G_eq  followed by  × r_s).
//
// Opt-1 + Opt-2 combined: the rotation uses CORDIC (zero DSPs), so the
// precompute loop itself consumes NO DSP48s — just LUTs and adders.
// The only remaining multiplier is equalize_sc in the hot path.
//
// CORDIC gain K₁₆ ≈ 1.64676 is absorbed into G_eq_final; consumers
// (qpsk_demap, qam16_demap, BPSK header demap) handle it as described
// in cordic_rotate_rx().
// ============================================================
static void compute_geq_final(
    cgeq_t         G_eq[FFT_SIZE],
    ap_fixed<32,4> phase_err,
    cgeq_t         G_eq_final[FFT_SIZE]
) {
    #pragma HLS INLINE off
    // Correct the phase: rotate by −phase_err (the CPE we want to undo).
    ap_fixed<32,4> rot_angle = -phase_err;

    GEQ_DATA_ROT: for (int d = 0; d < NUM_DATA_SC; d++) {
        #pragma HLS PIPELINE II=4
        int k = DATA_SC_IDX[d];
        G_eq_final[k] = cordic_rotate_rx(G_eq[k], rot_angle);
    }
    GEQ_PILOT_ROT: for (int p = 0; p < NUM_PILOT_SC; p++) {
        #pragma HLS PIPELINE II=4
        int k = PILOT_IDX[p];
        G_eq_final[k] = cordic_rotate_rx(G_eq[k], rot_angle);
    }
}

// ============================================================
// SECTION 5c: Pilot-Based CPE Estimation
//
// Computes the common phase error (CPE) for one data symbol using
// the 6 BPSK pilot subcarriers at PILOT_IDX = {50,75,100,154,179,204}.
//
// Pilots are transmitted as known +1 (real), so after equalization any
// residual phase (from residual CFO, oscillator phase noise, etc.) appears
// directly as angle(equalized_pilot).
//
// Averaging 6 pilots reduces noise on the estimate.
// Returns phase_err in radians; caller computes cos/sin once per symbol.
// ============================================================
// Returns phase in ap_fixed<32,4> radians.
// C-sim uses std::atan2 (accurate reference); synthesis uses CORDIC (no float IP).
static ap_fixed<32,4> compute_pilot_cpe(csample_t fft_out[FFT_SIZE], cgeq_t G_eq[FFT_SIZE]) {
    #pragma HLS INLINE off
    geq_t sum_sin = 0;
    geq_t sum_cos = 0;

    CPE_PILOTS: for (int p = 0; p < NUM_PILOT_SC; p++) {
        #pragma HLS PIPELINE II=1
        cgeq_t eq = equalize_sc(fft_out[PILOT_IDX[p]], G_eq[PILOT_IDX[p]]);
        sum_sin += (geq_t)eq.imag();
        sum_cos += (geq_t)eq.real();
    }
#ifdef __SYNTHESIS__
    return fixed_atan2_rx(sum_sin, sum_cos);
#else
    return (ap_fixed<32,4>)atan2((double)sum_sin, (double)sum_cos);
#endif
}

// ============================================================
// SECTION 6: QPSK Demapper
//
// Inverts qpsk_map() from ofdm_tx.cpp:
//   bit1 = sign(I): I < 0 → 1,  I ≥ 0 → 0
//   bit0 = sign(Q): Q < 0 → 1,  Q ≥ 0 → 0
// Returns 2-bit QPSK index matching qpsk_map() encoding.
// ============================================================
static ap_uint<2> qpsk_demap(cgeq_t sym) {
    #pragma HLS INLINE
    // Input is cgeq_t (ap_fixed<32,10>) — wide enough to hold noisy equalized
    // values without overflow (e.g. ±1.4 at 10 dB SNR fits, ±1.0 would wrap
    // in the old ap_fixed<16,1> input type).
    ap_uint<2> result;
    result[1] = (sym.real() < geq_t(0)) ? ap_uint<1>(1) : ap_uint<1>(0);
    result[0] = (sym.imag() < geq_t(0)) ? ap_uint<1>(1) : ap_uint<1>(0);
    return result;
}

// ============================================================
// SECTION 7: 16-QAM Demapper
//
// Inverts qam16_map() from ofdm_tx.cpp.
// Each axis is sliced into 4 regions using threshold ±2/√10:
//   v ≥ +2/√10          → bits 00  (+3/√10)
//   0 ≤ v < +2/√10      → bits 01  (+1/√10)
//   -2/√10 ≤ v < 0      → bits 11  (-1/√10)
//   v < -2/√10           → bits 10  (-3/√10)
// Returns 4-bit index [I_bits(3:2), Q_bits(1:0)].
// ============================================================
static ap_uint<4> qam16_demap(cgeq_t sym) {
    #pragma HLS INLINE
    // Base threshold = 2/sqrt(10) ≈ 0.6325.  Pre-scaled by CORDIC gain
    // K₁₆ ≈ 1.64676 to match the CORDIC-rotated equalized symbols from
    // cordic_rotate_rx() — compile-time constant, zero DSPs at runtime.
    //   THRESH = 0.6325 × 1.64676 ≈ 1.04157
    const geq_t THRESH = geq_t(1.04157);

    ap_uint<2> i_bits, q_bits;

    if (sym.real() >= THRESH)
        i_bits = ap_uint<2>(0b00);
    else if (sym.real() >= geq_t(0))
        i_bits = ap_uint<2>(0b01);
    else if (sym.real() >= -THRESH)
        i_bits = ap_uint<2>(0b11);
    else
        i_bits = ap_uint<2>(0b10);

    if (sym.imag() >= THRESH)
        q_bits = ap_uint<2>(0b00);
    else if (sym.imag() >= geq_t(0))
        q_bits = ap_uint<2>(0b01);
    else if (sym.imag() >= -THRESH)
        q_bits = ap_uint<2>(0b11);
    else
        q_bits = ap_uint<2>(0b10);

    // Explicit 4-bit container to avoid ap_uint<2><<2 truncation.
    ap_uint<4> result;
    result[3] = i_bits[1];
    result[2] = i_bits[0];
    result[1] = q_bits[1];
    result[0] = q_bits[0];
    return result;
}

// ============================================================
// SECTION 8: Equalize, Demap, and Pack one Data Symbol
//
// Takes G_eq_final (= CORDIC-rotated G_eq precomputed by compute_geq_final)
// so a SINGLE complex multiply per SC replaces the previous equalize_sc +
// apply_cpe pair.  x̂[k] = Y[k] · G_eq_final[k].  G_eq_final includes
// the CORDIC gain K₁₆ ≈ 1.647 (see cordic_rotate_rx for details).
//
// 1 SC per loop iteration, #pragma HLS PIPELINE II=1.
// No loop-unrolling: the byte accumulator has a 1-cycle carry dependency
// (ap_uint OR) which is the loop-carried critical path — well within II=1.
//
// QPSK  (mod=0): 200 iterations, write byte every 4th  → 50 bytes/symbol
// 16QAM (mod=1): 200 iterations, write byte every 2nd  → 100 bytes/symbol
//
// Byte layout (unchanged from TX):
//   QPSK:  [sc0[7:6]|sc1[5:4]|sc2[3:2]|sc3[1:0]]
//   16QAM: [sc0[7:4]|sc1[3:0]]
// ============================================================
static void equalize_demap_pack(
    csample_t               fft_out[FFT_SIZE],
    cgeq_t                  G_eq_final[FFT_SIZE],   // CORDIC-rotated equalizer (K₁₆ gain absorbed)
    mod_t                   mod,
    hls::stream<ap_uint<8>> &bits_out
) {
    #pragma HLS INLINE off

    if (mod == 0) {
        // QPSK: 2 bits/SC → 4 SCs per byte
        ap_uint<8> byte_acc = 0;
        QPSK_PACK: for (int i = 0; i < NUM_DATA_SC; i++) {
            #pragma HLS PIPELINE II=1
            cgeq_t eq  = equalize_sc(fft_out[DATA_SC_IDX[i]], G_eq_final[DATA_SC_IDX[i]]);
            ap_uint<2> sym = qpsk_demap(eq);

            // Place 2-bit symbol at the correct offset within the byte.
            // Reset accumulator at the start of each new byte (i%4==0).
            ap_uint<8> shifted = (ap_uint<8>)sym << ((3 - (i & 3)) * 2);
            byte_acc = ((i & 3) == 0) ? shifted : (byte_acc | shifted);

            if ((i & 3) == 3) bits_out.write(byte_acc);
        }
    } else {
        // 16-QAM: 4 bits/SC → 2 SCs per byte
        ap_uint<8> byte_acc = 0;
        QAM16_PACK: for (int i = 0; i < NUM_DATA_SC; i++) {
            #pragma HLS PIPELINE II=1
            cgeq_t eq  = equalize_sc(fft_out[DATA_SC_IDX[i]], G_eq_final[DATA_SC_IDX[i]]);
            ap_uint<4> sym = qam16_demap(eq);

            // Even SC → upper nibble; odd SC → lower nibble.
            ap_uint<8> shifted = ((i & 1) == 0) ? (ap_uint<8>)((ap_uint<8>)sym << 4)
                                                 : (ap_uint<8>)sym;
            byte_acc = ((i & 1) == 0) ? shifted : (byte_acc | shifted);

            if ((i & 1) == 1) bits_out.write(byte_acc);
        }
    }
}

// ============================================================
// SECTION 9: Top-Level OFDM RX
// (CRC-16/CCITT helper crc16_hdr() lives in ofdm_lut.h)
//
// Timing and CFO are corrected upstream by sync_detect (its FSM folds in
// the CFO derotator that used to live in the separate cfo_correct block).
// Frame structure: [Preamble] [Header] [Data × n_syms]
//
// Header symbol: BPSK on DATA_SC_IDX[0..25], MSB first.
//   bits[25:10] = CRC-16/CCITT over bits[9:0]
//   bits[9:2]   = n_syms[7:0]
//   bits[1]     = reserved (0)
//   bits[0]     = mod (0=QPSK, 1=16QAM)
// CRC failure: header_err=1, function returns without writing bits_out.
// ============================================================
void ofdm_rx(
    hls::stream<iq_t>       &iq_in,
    hls::stream<ap_uint<8>> &bits_out,
    hls::stream<iq32_t>     &fft_in,
    hls::stream<iq32_t>     &fft_out,
    ap_uint<1>              &header_err,
    volatile modcod_t       &modcod_out,
    volatile ap_uint<8>     &n_syms_out,
    volatile ap_uint<8>     &n_syms_fb
) {
    #pragma HLS INTERFACE axis        port=iq_in
    #pragma HLS INTERFACE axis        port=bits_out
    #pragma HLS INTERFACE axis        port=fft_in
    #pragma HLS INTERFACE axis        port=fft_out
    // Free-running: ap_ctrl_none, body is while(1).  No host/MAC start.
    #pragma HLS INTERFACE s_axilite   port=header_err bundle=stat
    #pragma HLS INTERFACE ap_none      port=modcod_out
    #pragma HLS INTERFACE ap_none      port=n_syms_out
    #pragma HLS INTERFACE ap_none      port=n_syms_fb
    #pragma HLS INTERFACE ap_ctrl_none port=return

    csample_t time_buf[FFT_SIZE];
    csample_t freq_buf[FFT_SIZE];
    cgeq_t    G_eq[FFT_SIZE];       // preamble equalizer: conj(G)/|G|², ≈256 ideal
    cgeq_t    G_eq_final[FFT_SIZE]; // per-symbol CORDIC-rotated equalizer (K₁₆ gain absorbed)

    // Free-running: once ap_start fires (tied high at BD), process
    // packet-after-packet forever.  Each loop iteration waits on iq_in —
    // which only delivers samples when sync_detect's gate is open.
    FREE_RUN: while (1) {
        #pragma HLS PIPELINE off

    // ── 1. Preamble: estimate channel, precompute G_eq ─────────
#ifdef __SYNTHESIS__
    remove_cp_and_read(iq_in, time_buf);
#else
    if (!remove_cp_and_read(iq_in, time_buf)) return;  // csim drain
#endif
    run_fft(time_buf, freq_buf, fft_in, fft_out);
    estimate_channel(freq_buf, G_eq);

    // ── 2. Header symbol: BPSK demap 26 SCs → extract mod, n_syms ──
#ifdef __SYNTHESIS__
    remove_cp_and_read(iq_in, time_buf);
#else
    if (!remove_cp_and_read(iq_in, time_buf)) return;  // csim drain
#endif
    run_fft(time_buf, freq_buf, fft_in, fft_out);

    ap_fixed<32,4> hdr_phase_err = compute_pilot_cpe(freq_buf, G_eq);

#ifndef __SYNTHESIS__
    // Diagnostic dump — same checkpoints as Python decode_header for direct comparison.
    {
        float pe_deg = (float)hdr_phase_err * 57.2957795f;
        std::cerr << "[HLS] phase_err = " << pe_deg << " deg ("
                  << (float)hdr_phase_err << " rad)\n";
        std::cerr << "[HLS] freq_buf[pilot 50,75,100,154,179,204]: ";
        for (int p : {50, 75, 100, 154, 179, 204})
            std::cerr << "(" << (float)freq_buf[p].real() << "," << (float)freq_buf[p].imag() << ") ";
        std::cerr << "\n";
        std::cerr << "[HLS] G_eq[pilot 50,75,100,154,179,204]: ";
        for (int p : {50, 75, 100, 154, 179, 204})
            std::cerr << "(" << (float)G_eq[p].real() << "," << (float)G_eq[p].imag() << ") ";
        std::cerr << "\n";
    }
#endif

    // Opt-1: CORDIC-rotate G_eq by −hdr_phase_err, reuse for all 26 header SCs.
    // No NCO (angle→phase_acc→sincos LUT) needed — CORDIC takes the angle directly.
    compute_geq_final(G_eq, hdr_phase_err, G_eq_final);

    // Demap 26 BPSK header bits, MSB first (d=0 → bit 25).
    // Separate raw-bit array avoids loop-carried read-modify-write on hdr_bits.
    ap_uint<1> hdr_raw[26];
    #pragma HLS ARRAY_PARTITION variable=hdr_raw complete
#ifndef __SYNTHESIS__
    float hdr_eq_real_dbg[26];
#endif
    HDR_DEMAP: for (int d = 0; d < 26; d++) {
        #pragma HLS PIPELINE II=1
        cgeq_t eq = equalize_sc(freq_buf[DATA_SC_IDX[d]], G_eq_final[DATA_SC_IDX[d]]);
        hdr_raw[d] = (eq.real() < geq_t(0)) ? ap_uint<1>(1) : ap_uint<1>(0);
#ifndef __SYNTHESIS__
        hdr_eq_real_dbg[d] = (float)eq.real();
#endif
    }
#ifndef __SYNTHESIS__
    {
        float lo = hdr_eq_real_dbg[0], hi = hdr_eq_real_dbg[0];
        for (int d = 1; d < 26; d++) {
            if (hdr_eq_real_dbg[d] < lo) lo = hdr_eq_real_dbg[d];
            if (hdr_eq_real_dbg[d] > hi) hi = hdr_eq_real_dbg[d];
        }
        std::cerr << "[HLS] hdr eq.real range: [" << lo << ", " << hi << "]\n";
        std::cerr << "[HLS] hdr eq.real (26 bits): ";
        for (int d = 0; d < 26; d++) std::cerr << hdr_eq_real_dbg[d] << " ";
        std::cerr << "\n";
    }
#endif

    ap_uint<26> hdr_bits = 0;
    for (int d = 0; d < 26; d++) {
        #pragma HLS UNROLL
        hdr_bits[25 - d] = hdr_raw[d];
    }

    ap_uint<16> rx_crc     = hdr_bits(25, 10);
    ap_uint<10> rx_payload = hdr_bits(9, 0);
    ap_uint<16> exp_crc    = crc16_hdr(rx_payload);

    if (rx_crc != exp_crc) {
        header_err = 1;
        modcod_out = 0;
        n_syms_out = 0;
        // Drain no longer needed — sync_detect is the gatekeeper.  Pulse
        // n_syms_fb = 0 so sync_detect returns to SEARCH without forwarding
        // any data samples; the no-op RX path naturally restarts.
        n_syms_fb = 0;
        continue;   // back to WAIT_PREAMBLE
    }
    header_err = 0;
    // New header layout: payload[1:0] = modcod {mod, rate}, payload[9:2] = n_syms.
    modcod_t   modcod = (modcod_t)rx_payload(1, 0);
    mod_t      mod    = (mod_t)modcod[1];
    ap_uint<8> n_syms = rx_payload(9, 2);
    // Surface decoded fields to the MAC via ap_vld output wires.
    modcod_out = modcod;
    n_syms_out = n_syms;
    // Feedback to sync_detect: "forward this many data-symbols' worth of samples".
    n_syms_fb = n_syms;

    // ── 3. Data symbols: pilot CPE track → equalize → demap → pack ──
    RX_SYMBOL_LOOP: for (ap_uint<8> s = 0; s < n_syms; s++) {
#ifdef __SYNTHESIS__
        remove_cp_and_read(iq_in, time_buf);
#else
        if (!remove_cp_and_read(iq_in, time_buf)) return;  // csim drain
#endif
        run_fft(time_buf, freq_buf, fft_in, fft_out);

        // Estimate common phase error from 6 BPSK pilots — fixed-point, no float
        ap_fixed<32,4> phase_err = compute_pilot_cpe(freq_buf, G_eq);

        // Opt-1 + Opt-2: CORDIC-rotate G_eq by −phase_err in the precompute
        // (zero DSPs), then single complex multiply per SC in the hot loop.
        compute_geq_final(G_eq, phase_err, G_eq_final);
        equalize_demap_pack(freq_buf, G_eq_final, mod, bits_out);
    }
    } // FREE_RUN while(1)
}
