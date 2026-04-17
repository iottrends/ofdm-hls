// ============================================================
// ofdm_rx.cpp  —  OFDM Receiver (Vitis HLS)
// ============================================================
#include "ofdm_rx.h"
#include <cmath>

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

// ============================================================
// Fixed-point sin/cos and atan2 — replaces hls::sincosf / hls::atan2f
//
// Quarter-wave LUT (256 entries): sin(k × π/512), k=0..255
// Full sin/cos derived by quadrant symmetry from phase_acc[31:30].
// Max error < 2^-14 — below Q0.15 quantisation floor.
//
// fixed_atan2: 16-iteration CORDIC, output in ap_fixed<16,4> (radians).
// Max error: ~0.0002 rad (~0.01°), sufficient for pilot CPE.
// ============================================================
static const sample_t SIN_LUT_RX[256] = {
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

// sin/cos from 32-bit phase accumulator (same quadrant-symmetry as cfo_correct_v2)
static void sincos_lut_rx(ap_uint<32> phase_acc, sample_t &sin_out, sample_t &cos_out) {
    #pragma HLS INLINE
    #pragma HLS PIPELINE II=1
    ap_uint<2> quadrant = phase_acc(31, 30);
    ap_uint<8> addr     = phase_acc(29, 22);
    sample_t sin_raw = SIN_LUT_RX[addr];
    sample_t cos_raw = SIN_LUT_RX[(ap_uint<8>)(255 - addr)];
    switch (quadrant) {
        case 0:  sin_out =  sin_raw; cos_out =  cos_raw; break;
        case 1:  sin_out =  cos_raw; cos_out = -sin_raw; break;
        case 2:  sin_out = -sin_raw; cos_out = -cos_raw; break;
        default: sin_out = -cos_raw; cos_out =  sin_raw; break;
    }
}

// Fixed-point atan2 via 16-iteration CORDIC vectoring mode.
// Returns phase in ap_fixed<32,4> (radians, range [-π, +π)).
// No floating-point — maps to DSP48 shift-add chains.
static ap_fixed<32,4> fixed_atan2_rx(geq_t y_in, geq_t x_in) {
    #pragma HLS INLINE
    // CORDIC angle LUT: atan(2^-i) for i = 0..15, scaled to ap_fixed<32,4>
    static const ap_fixed<32,4> ATAN_LUT[16] = {
        ap_fixed<32,4>(0.7853981634),  // atan(2^0)  = π/4
        ap_fixed<32,4>(0.4636476090),  // atan(2^-1)
        ap_fixed<32,4>(0.2449786631),  // atan(2^-2)
        ap_fixed<32,4>(0.1243549945),  // atan(2^-3)
        ap_fixed<32,4>(0.0624188100),  // atan(2^-4)
        ap_fixed<32,4>(0.0312398334),  // atan(2^-5)
        ap_fixed<32,4>(0.0156237286),  // atan(2^-6)
        ap_fixed<32,4>(0.0078123766),  // atan(2^-7)
        ap_fixed<32,4>(0.0039062301),  // atan(2^-8)
        ap_fixed<32,4>(0.0019531226),  // atan(2^-9)
        ap_fixed<32,4>(0.0009765622),  // atan(2^-10)
        ap_fixed<32,4>(0.0004882812),  // atan(2^-11)
        ap_fixed<32,4>(0.0002441406),  // atan(2^-12)
        ap_fixed<32,4>(0.0001220703),  // atan(2^-13)
        ap_fixed<32,4>(0.0000610352),  // atan(2^-14)
        ap_fixed<32,4>(0.0000305176),  // atan(2^-15)
    };

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

// Convert fixed-point angle (radians) to 32-bit phase accumulator
static ap_uint<32> angle_to_phase_acc(ap_fixed<32,4> angle) {
    #pragma HLS INLINE
    // Map angle in [-π, +π) → phase_acc in [0, 2^32) with wrapping.
    // Multiply by 2^32/(2π) ≈ 683565275.576; result is in [-2^31, +2^31).
    // Casting ap_fixed → ap_int<32> keeps the lower 32 integer bits (mod-2^32 wrap).
    // ap_uint<32> reinterprets as unsigned — standard NCO phase accumulator pattern.
    const ap_fixed<40,32> K = ap_fixed<40,32>(683565275.576);  // 2^32/(2π)
    ap_fixed<72,36> result = (ap_fixed<72,36>)angle * (ap_fixed<72,36>)K;
    return (ap_uint<32>)(ap_int<32>)result;
}

// ============================================================
// SECTION 1: ROM Tables  (must match ofdm_tx.cpp exactly)
// ============================================================

static const int PILOT_IDX[NUM_PILOT_SC] = {50, 75, 100, 154, 179, 204};

static const int DATA_SC_IDX[NUM_DATA_SC] = {
    // Positive half (bins 25–127, pilots at 50, 75, 100 excluded)
    25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,
    51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,
    76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,
    101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,
    120,121,122,123,124,125,126,127,
    // Negative half (bins 129–231, pilots at 154, 179, 204 excluded)
    129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,
    148,149,150,151,152,153,
    155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,
    174,175,176,177,178,
    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,
    199,200,201,202,203,
    205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231
};

// Zadoff-Chu LUT: Re and Im of zc[DATA_SC_IDX[d]] for d = 0..199
// Generated by gen_zc_lut.py — root u=25, N=256
// ZC_I_LUT[d] = Re(zc[DATA_SC_IDX[d]])
// ZC_Q_LUT[d] = Im(zc[DATA_SC_IDX[d]])
static const sample_t ZC_I_LUT[NUM_DATA_SC] = {
    sample_t(-0.073565), sample_t(-0.170962), sample_t(0.857729), sample_t(-0.595699), sample_t(-0.992480), sample_t(-0.844854), sample_t(-0.923880), sample_t(-0.923880),
    sample_t(0.219101), sample_t(0.788346), sample_t(-0.989177), sample_t(0.970031), sample_t(-0.575808), sample_t(-0.653173), sample_t(0.471397), sample_t(0.881921),
    sample_t(0.870087), sample_t(0.405241), sample_t(-0.740951), sample_t(-0.427555), sample_t(0.893224), sample_t(-0.914210), sample_t(0.555570), sample_t(0.555570),
    sample_t(-0.689541), sample_t(-0.998795), sample_t(-0.903989), sample_t(-0.024541), sample_t(0.992480), sample_t(-0.773010), sample_t(0.634393), sample_t(-0.893224),
    sample_t(0.844854), sample_t(0.595699), sample_t(-0.242980), sample_t(-0.492898), sample_t(-0.170962), sample_t(0.707107), sample_t(0.707107), sample_t(-0.985278),
    sample_t(0.870087), sample_t(-0.970031), sample_t(0.803208), sample_t(0.534998), sample_t(-0.449611), sample_t(-0.773010), sample_t(-0.634393), sample_t(0.122411),
    sample_t(0.999699), sample_t(-0.049068), sample_t(-0.073565), sample_t(0.724247), sample_t(-0.831470), sample_t(-0.831470), sample_t(-0.405241), sample_t(-0.449611),
    sample_t(-0.903989), sample_t(-0.671559), sample_t(0.914210), sample_t(-0.492898), sample_t(0.471397), sample_t(-0.881921), sample_t(0.757209), sample_t(0.817585),
    sample_t(0.242980), sample_t(0.146730), sample_t(0.615232), sample_t(0.975702), sample_t(-0.382683), sample_t(-0.382683), sample_t(0.534998), sample_t(-0.122411),
    sample_t(-0.803208), sample_t(0.985278), sample_t(0.997290), sample_t(0.956940), sample_t(0.290285), sample_t(-0.963776), sample_t(0.359895), sample_t(-0.049068),
    sample_t(0.336890), sample_t(-0.949528), sample_t(0.359895), sample_t(0.980785), sample_t(0.980785), sample_t(0.999699), sample_t(0.653173), sample_t(-0.671559),
    sample_t(-0.336890), sample_t(0.724247), sample_t(-0.615232), sample_t(-0.098017), sample_t(0.995185), sample_t(0.313682), sample_t(-0.219101), sample_t(-0.146730),
    sample_t(0.514103), sample_t(0.963776), sample_t(-0.575808), sample_t(0.000000), sample_t(0.575808), sample_t(-0.963776), sample_t(-0.514103), sample_t(0.146730),
    sample_t(0.219101), sample_t(-0.313682), sample_t(-0.995185), sample_t(0.098017), sample_t(0.615232), sample_t(-0.724247), sample_t(0.336890), sample_t(0.671559),
    sample_t(-0.653173), sample_t(-0.999699), sample_t(-0.980785), sample_t(-0.980785), sample_t(-0.359895), sample_t(0.949528), sample_t(-0.336890), sample_t(0.049068),
    sample_t(-0.359895), sample_t(0.963776), sample_t(-0.290285), sample_t(-0.956940), sample_t(-0.997290), sample_t(-0.514103), sample_t(0.803208), sample_t(0.122411),
    sample_t(-0.534998), sample_t(0.382683), sample_t(0.382683), sample_t(-0.975702), sample_t(-0.615232), sample_t(-0.146730), sample_t(-0.242980), sample_t(-0.817585),
    sample_t(-0.757209), sample_t(0.881921), sample_t(-0.471397), sample_t(0.492898), sample_t(-0.914210), sample_t(0.671559), sample_t(0.903989), sample_t(0.449611),
    sample_t(0.405241), sample_t(0.831470), sample_t(0.831470), sample_t(-0.724247), sample_t(0.073565), sample_t(0.427555), sample_t(-0.999699), sample_t(-0.122411),
    sample_t(0.634393), sample_t(0.773010), sample_t(0.449611), sample_t(-0.534998), sample_t(-0.803208), sample_t(0.970031), sample_t(-0.870087), sample_t(0.985278),
    sample_t(-0.707107), sample_t(-0.707107), sample_t(0.170962), sample_t(0.492898), sample_t(0.242980), sample_t(-0.595699), sample_t(-0.844854), sample_t(0.893224),
    sample_t(-0.634393), sample_t(0.773010), sample_t(-0.992480), sample_t(0.024541), sample_t(0.903989), sample_t(0.997290), sample_t(0.689541), sample_t(-0.555570),
    sample_t(-0.555570), sample_t(0.914210), sample_t(-0.893224), sample_t(0.427555), sample_t(0.740951), sample_t(-0.405241), sample_t(-0.870087), sample_t(-0.881921),
    sample_t(-0.471397), sample_t(0.653173), sample_t(0.575808), sample_t(-0.970031), sample_t(0.989177), sample_t(-0.788346), sample_t(-0.219101), sample_t(0.923880),
    sample_t(0.923880), sample_t(0.844854), sample_t(0.992480), sample_t(0.595699), sample_t(-0.857729), sample_t(0.170962), sample_t(0.073565), sample_t(0.290285),
};

static const sample_t ZC_Q_LUT[NUM_DATA_SC] = {
    sample_t(0.997290), sample_t(-0.985278), sample_t(0.514103), sample_t(0.803208), sample_t(-0.122411), sample_t(-0.534998), sample_t(-0.382683), sample_t(0.382683),
    sample_t(0.975702), sample_t(-0.615232), sample_t(0.146730), sample_t(-0.242980), sample_t(0.817585), sample_t(-0.757209), sample_t(-0.881921), sample_t(-0.471397),
    sample_t(-0.492898), sample_t(-0.914210), sample_t(-0.671559), sample_t(0.903989), sample_t(-0.449611), sample_t(0.405241), sample_t(-0.831470), sample_t(0.831470),
    sample_t(0.724247), sample_t(-0.049068), sample_t(0.427555), sample_t(0.999699), sample_t(-0.122411), sample_t(-0.634393), sample_t(0.773010), sample_t(-0.449611),
    sample_t(-0.534998), sample_t(0.803208), sample_t(0.970031), sample_t(0.870087), sample_t(0.985278), sample_t(0.707107), sample_t(-0.707107), sample_t(-0.170962),
    sample_t(0.492898), sample_t(-0.242980), sample_t(-0.595699), sample_t(0.844854), sample_t(0.893224), sample_t(0.634393), sample_t(0.773010), sample_t(0.992480),
    sample_t(0.024541), sample_t(0.998795), sample_t(-0.997290), sample_t(0.689541), sample_t(0.555570), sample_t(-0.555570), sample_t(-0.914210), sample_t(-0.893224),
    sample_t(-0.427555), sample_t(0.740951), sample_t(0.405241), sample_t(-0.870087), sample_t(0.881921), sample_t(-0.471397), sample_t(-0.653173), sample_t(0.575808),
    sample_t(0.970031), sample_t(0.989177), sample_t(0.788346), sample_t(-0.219101), sample_t(-0.923880), sample_t(0.923880), sample_t(-0.844854), sample_t(0.992480),
    sample_t(-0.595699), sample_t(-0.170962), sample_t(0.073565), sample_t(-0.290285), sample_t(-0.956940), sample_t(-0.266713), sample_t(0.932993), sample_t(-0.998795),
    sample_t(0.941544), sample_t(-0.313682), sample_t(-0.932993), sample_t(-0.195090), sample_t(0.195090), sample_t(-0.024541), sample_t(-0.757209), sample_t(-0.740951),
    sample_t(0.941544), sample_t(-0.689541), sample_t(0.788346), sample_t(-0.995185), sample_t(0.098017), sample_t(0.949528), sample_t(0.975702), sample_t(0.989177),
    sample_t(0.857729), sample_t(-0.266713), sample_t(-0.817585), sample_t(0.999969), sample_t(0.817585), sample_t(0.266713), sample_t(-0.857729), sample_t(-0.989177),
    sample_t(-0.975702), sample_t(-0.949528), sample_t(-0.098017), sample_t(0.995185), sample_t(-0.788346), sample_t(0.689541), sample_t(-0.941544), sample_t(0.740951),
    sample_t(0.757209), sample_t(0.024541), sample_t(-0.195090), sample_t(0.195090), sample_t(0.932993), sample_t(0.313682), sample_t(-0.941544), sample_t(0.998795),
    sample_t(-0.932993), sample_t(0.266713), sample_t(0.956940), sample_t(0.290285), sample_t(-0.073565), sample_t(0.857729), sample_t(0.595699), sample_t(-0.992480),
    sample_t(0.844854), sample_t(-0.923880), sample_t(0.923880), sample_t(0.219101), sample_t(-0.788346), sample_t(-0.989177), sample_t(-0.970031), sample_t(-0.575808),
    sample_t(0.653173), sample_t(0.471397), sample_t(-0.881921), sample_t(0.870087), sample_t(-0.405241), sample_t(-0.740951), sample_t(0.427555), sample_t(0.893224),
    sample_t(0.914210), sample_t(0.555570), sample_t(-0.555570), sample_t(-0.689541), sample_t(0.997290), sample_t(0.903989), sample_t(-0.024541), sample_t(-0.992480),
    sample_t(-0.773010), sample_t(-0.634393), sample_t(-0.893224), sample_t(-0.844854), sample_t(0.595699), sample_t(0.242980), sample_t(-0.492898), sample_t(0.170962),
    sample_t(0.707107), sample_t(-0.707107), sample_t(-0.985278), sample_t(-0.870087), sample_t(-0.970031), sample_t(-0.803208), sample_t(0.534998), sample_t(0.449611),
    sample_t(-0.773010), sample_t(0.634393), sample_t(0.122411), sample_t(-0.999699), sample_t(-0.427555), sample_t(-0.073565), sample_t(-0.724247), sample_t(-0.831470),
    sample_t(0.831470), sample_t(-0.405241), sample_t(0.449611), sample_t(-0.903989), sample_t(0.671559), sample_t(0.914210), sample_t(0.492898), sample_t(0.471397),
    sample_t(0.881921), sample_t(0.757209), sample_t(-0.817585), sample_t(0.242980), sample_t(-0.146730), sample_t(0.615232), sample_t(-0.975702), sample_t(-0.382683),
    sample_t(0.382683), sample_t(0.534998), sample_t(0.122411), sample_t(-0.803208), sample_t(-0.514103), sample_t(0.985278), sample_t(-0.997290), sample_t(0.956940),
};

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
// ============================================================
static void remove_cp_and_read(
    hls::stream<iq_t> &iq_in,
    csample_t          buf[FFT_SIZE]
) {
    #pragma HLS INLINE off
    iq_t s;

    // Discard CP
    SKIP_CP: for (int i = 0; i < CP_LEN; i++) {
        #pragma HLS PIPELINE II=1
        s = iq_in.read();
        (void)s;
    }

    // Read symbol body into FFT input buffer
    READ_SYM: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        s = iq_in.read();
        buf[i] = csample_t(s.i, s.q);
    }
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
// SECTION 5b: CPE Rotation Helper
//
// Applies the inverse of the common phase error to a single equalized symbol.
//   cpe_cos = cos(phase_err),  cpe_sin = sin(phase_err)  — both geq_t ∈ [-1,+1)
//   corrected = sym × exp(-j·phase_err)
//   re' =  re × cpe_cos + im × cpe_sin
//   im' = -re × cpe_sin + im × cpe_cos
//
// All arithmetic in ap_fixed<32,10> — 4 DSP48 multiplies per call.
// Caller converts sincosf float output to geq_t once per symbol.
// ============================================================
static cgeq_t apply_cpe(cgeq_t sym, geq_t cpe_cos, geq_t cpe_sin) {
    #pragma HLS INLINE
    return cgeq_t( sym.real() * cpe_cos + sym.imag() * cpe_sin,
                  -sym.real() * cpe_sin + sym.imag() * cpe_cos);
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
    const geq_t THRESH = geq_t(0.6325);   // 2/sqrt(10), in ap_fixed<32,10>

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
// SECTION 8: Equalize, CPE-Correct, Demap, and Pack one Data Symbol
//
// 1 SC per loop iteration, #pragma HLS PIPELINE II=1.
// No loop-unrolling: the byte accumulator has a 1-cycle carry dependency
// (ap_uint OR) which is the loop-carried critical path — well within II=1.
//
// cpe_cos/cpe_sin: geq_t (ap_fixed<32,10>), converted once per symbol by caller.
// apply_cpe(): 4 DSP48 multiplies (fixed-point), no float casts.
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
    cgeq_t                  G_eq[FFT_SIZE],
    mod_t                   mod,
    geq_t                   cpe_cos,   // cos(phase_err), geq_t ∈ [-1,+1)
    geq_t                   cpe_sin,   // sin(phase_err), geq_t ∈ [-1,+1)
    hls::stream<ap_uint<8>> &bits_out
) {
    #pragma HLS INLINE off

    if (mod == 0) {
        // QPSK: 2 bits/SC → 4 SCs per byte
        ap_uint<8> byte_acc = 0;
        QPSK_PACK: for (int i = 0; i < NUM_DATA_SC; i++) {
            #pragma HLS PIPELINE II=1
            cgeq_t eq  = equalize_sc(fft_out[DATA_SC_IDX[i]], G_eq[DATA_SC_IDX[i]]);
            ap_uint<2> sym = qpsk_demap(apply_cpe(eq, cpe_cos, cpe_sin));

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
            cgeq_t eq  = equalize_sc(fft_out[DATA_SC_IDX[i]], G_eq[DATA_SC_IDX[i]]);
            ap_uint<4> sym = qam16_demap(apply_cpe(eq, cpe_cos, cpe_sin));

            // Even SC → upper nibble; odd SC → lower nibble.
            ap_uint<8> shifted = ((i & 1) == 0) ? (ap_uint<8>)((ap_uint<8>)sym << 4)
                                                 : (ap_uint<8>)sym;
            byte_acc = ((i & 1) == 0) ? shifted : (byte_acc | shifted);

            if ((i & 1) == 1) bits_out.write(byte_acc);
        }
    }
}

// ============================================================
// SECTION 8b: CRC-16/CCITT (polynomial 0x1021, init 0xFFFF)
//
// Identical to the TX-side crc16_hdr — computes CRC over the
// 10-bit header payload for verification.
// Fully unrolled → pure combinational.
// ============================================================
static ap_uint<16> crc16_hdr(ap_uint<10> data) {
    ap_uint<16> crc = 0xFFFF;
    for (int i = 9; i >= 0; i--) {
        #pragma HLS UNROLL
        ap_uint<1> x = crc[15] ^ data[i];
        crc = (ap_uint<16>)(crc << 1);
        if (x) crc ^= ap_uint<16>(0x1021);
    }
    return crc;
}

// ============================================================
// SECTION 9: Top-Level OFDM RX
//
// Timing and CFO are corrected upstream by sync_detect + cfo_correct.
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
    cgeq_t    G_eq[FFT_SIZE];       // precomputed equalizer: conj(G)/|G|², ≈256 ideal

    // Free-running: once ap_start fires (tied high at BD), process
    // packet-after-packet forever.  Each loop iteration waits on iq_in —
    // which only delivers samples when sync_cfo's gate is open.
    FREE_RUN: while (1) {
        #pragma HLS PIPELINE off

    // ── 1. Preamble: estimate channel, precompute G_eq ─────────
    remove_cp_and_read(iq_in, time_buf);
    run_fft(time_buf, freq_buf, fft_in, fft_out);
    estimate_channel(freq_buf, G_eq);

    // ── 2. Header symbol: BPSK demap 26 SCs → extract mod, n_syms ──
    remove_cp_and_read(iq_in, time_buf);
    run_fft(time_buf, freq_buf, fft_in, fft_out);

    ap_fixed<32,4> hdr_phase_err = compute_pilot_cpe(freq_buf, G_eq);
    ap_uint<32> hdr_phase_acc = angle_to_phase_acc(hdr_phase_err);
    sample_t hdr_sin, hdr_cos;
    sincos_lut_rx(hdr_phase_acc, hdr_sin, hdr_cos);
    geq_t hdr_cos_g = (geq_t)hdr_cos;
    geq_t hdr_sin_g = (geq_t)hdr_sin;

    // Demap 26 BPSK header bits, MSB first (d=0 → bit 25).
    // Separate raw-bit array avoids loop-carried read-modify-write on hdr_bits.
    ap_uint<1> hdr_raw[26];
    #pragma HLS ARRAY_PARTITION variable=hdr_raw complete
    HDR_DEMAP: for (int d = 0; d < 26; d++) {
        #pragma HLS PIPELINE II=1
        cgeq_t eq  = equalize_sc(freq_buf[DATA_SC_IDX[d]], G_eq[DATA_SC_IDX[d]]);
        cgeq_t rot = apply_cpe(eq, hdr_cos_g, hdr_sin_g);
        hdr_raw[d] = (rot.real() < geq_t(0)) ? ap_uint<1>(1) : ap_uint<1>(0);
    }

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
        // Drain no longer needed — sync_cfo is the gatekeeper.  Pulse
        // n_syms_fb = 0 so sync_cfo returns to SEARCH without forwarding
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
    // Feedback to sync_cfo: "forward this many data-symbols' worth of samples".
    n_syms_fb = n_syms;

    // ── 3. Data symbols: pilot CPE track → equalize → demap → pack ──
    RX_SYMBOL_LOOP: for (ap_uint<8> s = 0; s < n_syms; s++) {
        remove_cp_and_read(iq_in, time_buf);
        run_fft(time_buf, freq_buf, fft_in, fft_out);

        // Estimate common phase error from 6 BPSK pilots — fixed-point, no float
        ap_fixed<32,4> phase_err = compute_pilot_cpe(freq_buf, G_eq);
        ap_uint<32> phase_acc = angle_to_phase_acc(phase_err);
        sample_t cpe_sin_s, cpe_cos_s;
        sincos_lut_rx(phase_acc, cpe_sin_s, cpe_cos_s);
        geq_t cpe_cos = (geq_t)cpe_cos_s;
        geq_t cpe_sin = (geq_t)cpe_sin_s;

        equalize_demap_pack(freq_buf, G_eq, mod, cpe_cos, cpe_sin, bits_out);
    }
    } // FREE_RUN while(1)
}
