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

// ── Modulation / rate selectors ──────────────────────────────
typedef ap_uint<1> mod_t;   // 0 = QPSK,  1 = 16-QAM
// modcod: single 2-bit control carrying both mod and rate together.
// Decoded consistently across the chain:
//   00 = QPSK  rate-1/2    (mod=0, rate=0)
//   01 = QPSK  rate-2/3    (mod=0, rate=1)
//   10 = 16QAM rate-1/2    (mod=1, rate=0)
//   11 = 16QAM rate-2/3    (mod=1, rate=1)
// Convention: modcod[1] → mod, modcod[0] → rate.
typedef ap_uint<2> modcod_t;

// ── AXI-Stream Structs ───────────────────────────────────────
// iq_t: DAC/ADC-facing AXIS — carries packet framing via TLAST.
//       HLS packs this to 40-bit TDATA (16+16+1 padded) which is fine
//       because the DAC/ADC interface needs the packet-end marker.
struct iq_t {
    sample_t   i;     // In-phase sample
    sample_t   q;     // Quadrature sample
    ap_uint<1> last;  // AXI TLAST: 1 on final sample of packet
};

// iq32_t: xfft-facing AXIS — exactly 32-bit TDATA (imag[31:16], real[15:0]).
//         No `last` field: the external xfft IP is configured with fixed
//         frame length (FFT_SIZE = 256) and generates its own output TLAST,
//         so no TLAST needs to be driven into `s_axis_data_tlast`.
//         Keeping only two 16-bit fields means HLS emits a 32-bit port that
//         matches the xfft `s_axis_data_tdata` width exactly.
struct iq32_t {
    sample_t i;
    sample_t q;
};

// ── Top-Level Function ───────────────────────────────────────
// bits_in   : raw bit stream (QPSK: 50 bytes/sym, 16QAM: 100 bytes/sym)
// iq_out    : AXI-Stream IQ samples to DAC, TLAST on last sample of last symbol
// ifft_in   : 32-bit freq-domain data to external xfft IP (ofdm_tx → xfft)
// ifft_out  : 32-bit time-domain data from external xfft IP (xfft → ofdm_tx)
// mod       : 0=QPSK, 1=16QAM
// n_syms    : number of OFDM data symbols (not counting preamble/header)
void ofdm_tx(
    hls::stream<ap_uint<8>> &bits_in,
    hls::stream<iq_t>        &iq_out,
    hls::stream<iq32_t>      &ifft_in,
    hls::stream<iq32_t>      &ifft_out,
    modcod_t                  modcod,    // 2-bit: {mod, rate}
    ap_uint<8>                n_syms
);
