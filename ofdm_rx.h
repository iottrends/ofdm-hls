// ============================================================
// ofdm_rx.h  —  OFDM Receiver for Vitis HLS
//
// Processes a packet produced by ofdm_tx:
//   [Preamble (ZC, 288 samp)] [Data sym 0 (288)] ... [Data sym N-1 (288)]
//
// RX chain (timing and CFO assumed corrected by upstream sync_detect + cfo_correct):
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
#include "ofdm_tx.h"   // sample_t, csample_t, iq_t, mod_t, fft_cfg

// Top-level RX function
//   iq_in      : AXI-Stream IQ samples (preamble + header + data symbols)
//   bits_out   : decoded bytes
//                QPSK:  50 bytes/symbol  (4 syms/byte × 200 data SC)
//                16QAM: 100 bytes/symbol (2 syms/byte × 200 data SC)
//   header_err : 1 if CRC-16 on frame header failed; bits_out empty on error.
//                mod and n_syms are extracted from the header symbol (no AXI-Lite).
void ofdm_rx(
    hls::stream<iq_t>       &iq_in,
    hls::stream<ap_uint<8>> &bits_out,
    ap_uint<1>              &header_err
);
