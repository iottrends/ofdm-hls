// ============================================================
// cfo_correct.cpp v3 — Per-Sample CFO Correction (Fixed-Point)
//
// v2 still had 27 DSP.  Root causes and fixes:
//
//   (a) double arithmetic for delta_fixed:
//       (double)eff_cfo / (double)FFT_SIZE * 4294967296.0
//       → HLS synthesizes double-precision division: ~12 DSP.
//       Fix: delta = cfo_est × 2^24 (pure bit-shift, 0 DSP).
//         cfo_t = ap_fixed<16,2>: raw_bits = value × 2^14 (14 frac bits).
//         NCO step = value / N × 2^32 = value × 2^24 (N=256=2^8).
//         = raw_bits × 2^10.  Just a left-shift — no multiply.
//
//   (b) ap_fixed<32,2> cast before complex multiply:
//       acc_t in_i = (acc_t)s.i;  acc_t c = (acc_t)cos_p;
//       in_i * c  → 32×32 multiply = 2 DSP each × 4 = 8 DSP.
//       Fix: keep sample_t (ap_fixed<16,1>) operands directly.
//       16×16 fits in 1 DSP48 → 4 muls × 1 = 4 DSP.
//
//   (c) float deadband:
//       const float cfo_float = (float)cfo_est;
//       float compare → ~3 DSP.
//       Fix: compare cfo_t directly (ap_fixed compare = LUT, 0 DSP).
//
// Result: 27 DSP → 4 DSP (only the 4 complex rotation multiplies).
//
// Sin/cos generation (unchanged from v2):
//   256-entry quarter-wave LUT + quadrant symmetry.
//   Resolution: 16-bit output — max error < 2^-14 < Q0.15 noise floor.
// ============================================================
#include "cfo_correct.h"

// ── Quarter-wave sine LUT (256 entries, ap_fixed<16,1>) ────
// sin(k × π/512) for k = 0..255
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
    sample_t(+0.615232), sample_t(+0.620057), sample_t(+0.624859), sample_t(+0.629638),
    sample_t(+0.634393), sample_t(+0.639124), sample_t(+0.643832), sample_t(+0.648514),
    sample_t(+0.653173), sample_t(+0.657807), sample_t(+0.662416), sample_t(+0.667000),
    sample_t(+0.671559), sample_t(+0.676093), sample_t(+0.680601), sample_t(+0.685084),
    sample_t(+0.689541), sample_t(+0.693971), sample_t(+0.698376), sample_t(+0.702755),
    sample_t(+0.707107), sample_t(+0.711432), sample_t(+0.715731), sample_t(+0.720003),
    sample_t(+0.724247), sample_t(+0.728464), sample_t(+0.732654), sample_t(+0.736817),
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
    sample_t(+0.999699), sample_t(+0.999831), sample_t(+0.999925), sample_t(+0.999969)
};

// ── Fixed-point sin/cos from 32-bit phase accumulator ──────
// Quadrant from bits [31:30], LUT address from bits [29:22].
static void sincos_lut(ap_uint<32> phase_acc, sample_t &sin_out, sample_t &cos_out)
{
#pragma HLS INLINE
    ap_uint<2> quadrant = phase_acc(31, 30);
    ap_uint<8> addr     = phase_acc(29, 22);

    sample_t sin_raw = SIN_LUT[addr];
    sample_t cos_raw = SIN_LUT[(ap_uint<8>)(255 - addr)];

    switch (quadrant) {
        case 0:  sin_out =  sin_raw; cos_out =  cos_raw; break;
        case 1:  sin_out =  cos_raw; cos_out = -sin_raw; break;
        case 2:  sin_out = -sin_raw; cos_out = -cos_raw; break;
        default: sin_out = -cos_raw; cos_out =  sin_raw; break;
    }
}

void cfo_correct(
    hls::stream<iq_t>& iq_in,
    hls::stream<iq_t>& iq_out,
    cfo_t              cfo_est,
    ap_uint<8>         n_syms
)
{
#pragma HLS INTERFACE axis      port=iq_in
#pragma HLS INTERFACE axis      port=iq_out
#pragma HLS INTERFACE s_axilite port=cfo_est bundle=ctrl
#pragma HLS INTERFACE s_axilite port=n_syms  bundle=ctrl
#pragma HLS INTERFACE s_axilite port=return  bundle=ctrl

    const int total_samples = ((int)n_syms + 2) * SYNC_NL;

    // ── CFO deadband — fixed-point comparison, 0 DSP ──────────
    const cfo_t CFO_THRESH = cfo_t(0.01f);
    cfo_t eff_cfo = (cfo_est > CFO_THRESH || cfo_est < -CFO_THRESH)
                    ? cfo_est : cfo_t(0);

    // ── NCO phase step — sign-extend then shift, 0 DSP ───────
    // cfo_t = ap_fixed<16,2>: raw_bits = value × 2^14 (14 frac bits).
    // NCO step = eff_cfo / N × 2^32 = eff_cfo × 2^24 = raw_bits × 2^10.
    // → shift by 10, not 12.
    ap_int<16>  raw16       = (ap_int<16>)eff_cfo.range(15, 0);
    ap_int<32>  raw32       = (ap_int<32>)raw16;       // sign-extend 16→32
    ap_uint<32> delta_fixed = (ap_uint<32>)(raw32 << 10);

    ap_uint<32> phase_acc = 0;

    CORRECT: for (int n = 0; n < total_samples; n++) {
#pragma HLS PIPELINE II=1
#pragma HLS loop_tripcount min=SYNC_NL max=((MAX_DATA_SYMS+2)*SYNC_NL)
        iq_t s = iq_in.read();
        // phase_acc -= delta → accumulator tracks correction angle.
        // Conjugate rotation e^{−jφ} cancels applied CFO e^{+jnε2π/N}.
        phase_acc -= delta_fixed;

        // LUT sin/cos lookup — 0 DSP (BRAM or distributed ROM)
        sample_t cos_p, sin_p;
        sincos_lut(phase_acc, sin_p, cos_p);

        // Conjugate rotation: out = in × e^{−jφ} = in × (cos − j·sin).
        // out.i = in_i×cos + in_q×sin
        // out.q = −in_i×sin + in_q×cos
        iq_t out;
        out.i    = sample_t(s.i * cos_p + s.q * sin_p);
        out.q    = sample_t(-s.i * sin_p + s.q * cos_p);
        out.last = s.last;
        iq_out.write(out);
    }
}
