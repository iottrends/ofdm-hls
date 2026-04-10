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

// ── Top-Level Function ───────────────────────────────────────
// bits_in   : raw bit stream (QPSK: 50 bytes/sym, 16QAM: 100 bytes/sym)
// iq_out    : AXI-Stream IQ samples, TLAST on last sample of last symbol
// ifft_in   : freq-domain data to external xfft IP (ofdm_tx → xfft)
// ifft_out  : time-domain data from external xfft IP (xfft → ofdm_tx)
// mod       : 0=QPSK, 1=16QAM
// n_syms    : number of OFDM data symbols (not counting preamble/header)
void ofdm_tx(
    hls::stream<ap_uint<8>> &bits_in,
    hls::stream<iq_t>        &iq_out,
    hls::stream<iq_t>        &ifft_in,
    hls::stream<iq_t>        &ifft_out,
    mod_t                     mod,
    ap_uint<8>                n_syms
);
