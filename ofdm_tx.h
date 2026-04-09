// ============================================================
// ofdm_tx.h  —  OFDM Transmitter for Vitis HLS
//
// Subcarrier plan (256 total):
//   Data      : 200  subcarriers
//   Pilots    : 6    subcarriers (BPSK, value = +1)
//   Guard/Null: 50   subcarriers (DC + Nyquist + guard bands)
//
// Guard bands:
//   Upper guard : FFT indices   1 –  24  (positive freq edge)
//   Lower guard : FFT indices 232 – 255  (negative freq edge)
//   DC null     : FFT index    0
//   Nyquist null: FFT index  128
//
// Pilot positions:
//   Positive half: 50, 75, 100
//   Negative half: 154, 179, 204
//
// Cyclic prefix : 32 samples (12.5% overhead)
// Modulation    : QPSK (2 bps) or 16-QAM (4 bps)
// Preamble      : 1 OFDM symbol using Zadoff-Chu sequence
// ============================================================
#pragma once

#include "ap_fixed.h"
#include "ap_int.h"
#include "hls_stream.h"
#include "hls_fft.h"
#include <complex>

// ── System Parameters ────────────────────────────────────────
#define FFT_SIZE        256
#define CP_LEN          32
#define NUM_DATA_SC     200
#define NUM_PILOT_SC    6
#define NUM_NULL_SC     50

// ── Fixed-Point Type ─────────────────────────────────────────
// ap_fixed<W, I>: W=total bits, I=integer bits (including sign)
// Q1.14: range [-2, +2), resolution ~6e-5
// Normalized QPSK/16QAM symbols fit within ±1.0
// Q0.15: range [-1, +1), resolution ~3e-5
// hls::ip_fft in 2025.2 requires plain ap_fixed<W, 1> (no rounding/overflow mode)
typedef ap_fixed<16, 1> sample_t;
typedef std::complex<sample_t>           csample_t;

// ── Modulation Selector ──────────────────────────────────────
typedef ap_uint<1> mod_t;   // 0 = QPSK,  1 = 16-QAM

// ── AXI-Stream Output ────────────────────────────────────────
struct iq_t {
    sample_t   i;     // In-phase sample
    sample_t   q;     // Quadrature sample
    ap_uint<1> last;  // AXI TLAST: 1 on final sample of packet
};

// ── Xilinx FFT IP Configuration ──────────────────────────────
// Replace hls::fft<fft_cfg>() with Vivado IP if preferred.
struct fft_cfg : hls::ip_fft::params_t {
    // 2025.2 API: use new field names only
    static const unsigned log2_transform_length                              = 8;     // 2^8 = 256 pts
    static const bool     run_time_configurable_transform_length             = false;
    static const unsigned implementation_options                             = hls::ip_fft::pipelined_streaming_io;
    static const unsigned output_ordering                                    = hls::ip_fft::natural_order;
    static const bool     ovflo                                              = false;
    static const unsigned input_width                                        = 16;
    static const unsigned output_width                                       = 16;
};

// ── Top-Level Function ───────────────────────────────────────
// bits_in  : raw bit stream, 1 byte per call
//            QPSK  → lower 2 bits used per subcarrier (200 bytes/symbol)
//            16QAM → lower 4 bits used per subcarrier (200 bytes/symbol)
// iq_out   : AXI-Stream IQ samples, TLAST on last sample of last symbol
// mod      : 0=QPSK, 1=16QAM
// n_syms   : number of OFDM data symbols to transmit (not counting preamble)
void ofdm_tx(
    hls::stream<ap_uint<8>> &bits_in,
    hls::stream<iq_t>        &iq_out,
    mod_t                     mod,
    ap_uint<8>                n_syms
);
