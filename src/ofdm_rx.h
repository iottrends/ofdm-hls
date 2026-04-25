// ============================================================
// ofdm_rx.h  —  OFDM Receiver for Vitis HLS
//
// Processes a packet produced by ofdm_tx:
//   [Preamble (ZC, 288 samp)] [Data sym 0 (288)] ... [Data sym N-1 (288)]
//
// RX chain (timing and CFO corrected upstream by sync_detect, which folds the
// preamble gate and CFO derotator into one free-running block):
//   remove_cp → FFT → channel_estimate (preamble)
//                   → pilot_cpe_track → equalize → demap → pack_bits (data)
//
// FFT: forward, scale_sch=0xAA (Radix-4 ÷256 total).
//   TX IFFT also uses ÷256; round-trip: Y[k] = X[k]/256.
//   Channel estimate G[k] ≈ 1/256; precomputed G_eq = conj(G)/|G|² ≈ 256.
//   Equalization = multiply-only (no per-symbol divider).
//   Pilot phase tracking: 6 BPSK pilots per symbol → common phase error (CPE)
//   correction applied before demapping to handle residual CFO + phase noise.
// ============================================================
#pragma once
#include "ofdm_tx.h"   // sample_t, csample_t, iq_t, mod_t

// Top-level RX function
//   iq_in      : AXI-Stream IQ samples (preamble + header + data symbols)
//   bits_out   : decoded bytes
//                QPSK:  50 bytes/symbol  (4 syms/byte × 200 data SC)
//                16QAM: 100 bytes/symbol (2 syms/byte × 200 data SC)
//   fft_in      : 32-bit time-domain data to external xfft IP (ofdm_rx → xfft)
//   fft_out     : 32-bit freq-domain data from external xfft IP (xfft → ofdm_rx)
//   header_err  : 1 if CRC-16 on frame header failed; bits_out empty on error.
//   modcod_out  : 2-bit {mod,rate} decoded from header (ap_vld per packet).
//   n_syms_out  : 8-bit n_syms decoded from header (ap_vld per packet).
//   n_syms_fb   : feedback to sync_detect — number of data symbols to forward.
//                 Pulsed with n_syms on success, pulsed with 0 on header CRC
//                 error so sync_detect returns to SEARCH cleanly (ap_vld).
//
// Free-running: top body is while(1); ap_start tied high at BD.
void ofdm_rx(
    hls::stream<iq_t>       &iq_in,
    hls::stream<ap_uint<8>> &bits_out,
    hls::stream<iq32_t>     &fft_in,
    hls::stream<iq32_t>     &fft_out,
    ap_uint<1>              &header_err,
    volatile modcod_t       &modcod_out,
    volatile ap_uint<8>     &n_syms_out,
    volatile ap_uint<8>     &n_syms_fb
);
