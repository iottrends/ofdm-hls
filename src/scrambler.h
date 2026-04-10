// ============================================================
// scrambler.h  —  802.11a LFSR Scrambler / Descrambler
//
// Polynomial : x⁷ + x⁴ + 1
// Seed       : 0x7F (all ones, fixed per packet — no dynamic seeding)
// Bit order  : MSB first within each byte (matches conv_enc / viterbi_dec)
//
// The LFSR output is XOR'd with each input bit.  Because XOR is
// self-inverse, the same function scrambles on TX and descrambles on RX.
//
// TX chain:  raw_bytes → scrambler → conv_enc → ofdm_tx
// RX chain:  ofdm_rx → viterbi_dec → scrambler → raw_bytes
// ============================================================
#pragma once
#include "ofdm_tx.h"   // ap_uint, hls::stream, ap_fixed types

void scrambler(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& data_out,
    ap_uint<16>               n_bytes   // number of bytes to process
);
