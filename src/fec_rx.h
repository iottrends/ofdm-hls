// ============================================================
// fec_rx.h  —  Merged RX bit-processing chain
//
//   data_in ──► interleaver(RX) ──► viterbi_dec ──► scrambler ──► data_out
//
// Combines the three RX bit-domain IPs into one HLS block.  All three
// sub-stages run concurrently under #pragma HLS DATAFLOW via internal
// hls::stream FIFOs.  The scrambler is self-inverse, so the same
// function descrambles on RX with the fixed 0x7F seed.
//
// Control interface (all s_axilite, bundle=ctrl):
//   mod          : 0=QPSK, 1=16-QAM         (interleaver)
//   n_syms       : OFDM symbols in payload  (interleaver)
//   rate         : 0=rate 1/2, 1=rate 2/3   (viterbi_dec)
//   n_data_bytes : expected payload bytes   (viterbi_dec, scrambler)
//
// Timing / throughput
// ───────────────────
// This block is intended to run on its own 200 MHz clock domain
// (set create_clock -period 5 in the Vitis HLS solution; cross
// into the 100 MHz system clock at the AXIS boundaries via the
// Vivado AXI-Stream Clock Converter).
//
// The viterbi_dec inside this block is v3 (unroll=64 ACS, 1 cycle
// per trellis stage).  At 200 MHz it keeps up with 16-QAM r=2/3
// real-time (≈2.7 ms of compute vs. 3.67 ms of air time per
// 255-symbol frame).  v2 (unroll=16 @ 100 MHz) was ~3.7× too slow
// for that mode — that is why we merged and re-tuned here.
// ============================================================
#pragma once
#include "interleaver.h"
#include "conv_fec.h"
#include "scrambler.h"

// modcod, n_syms are ap_none scalar inputs — driven at BD level directly from
// ofdm_rx.modcod_out / ofdm_rx.n_syms_out (decoded from the BPSK air header).
// n_data_bytes is derived internally from n_syms × bytes_per_sym(modcod) so
// there's no extra wire.
void fec_rx(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& data_out,
    modcod_t                 modcod,     // ap_none — from ofdm_rx.modcod_out
    ap_uint<8>               n_syms      // ap_none — from ofdm_rx.n_syms_out
);
