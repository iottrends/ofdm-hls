// ============================================================
// interleaver.h  —  802.11a Two-Step Bit Interleaver / Deinterleaver
//
// Permutes coded bits within each OFDM symbol to spread burst errors
// across non-adjacent subcarriers (step 1) and across constellation
// bit significance levels (step 2).
//
// Parameters (our system, NUM_DATA_SC=200):
//   QPSK  (mod=0): NCBPS = 200×2 = 400 bits = 50 bytes/symbol, s=1
//   16-QAM(mod=1): NCBPS = 200×4 = 800 bits = 100 bytes/symbol, s=2
//
// 802.11a two-step permutation:
//   Step 1: k = (NCBPS/16) × (i mod 16) + ⌊i/16⌋
//   Step 2: j = s×⌊k/s⌋ + (k + NCBPS − ⌊16k/NCBPS⌋) mod s
//
// TX chain (is_rx=0):  scrambler → conv_enc → interleaver → ofdm_tx
// RX chain (is_rx=1):  ofdm_rx → interleaver → viterbi_dec → descrambler
// ============================================================
#pragma once
#include "ofdm_tx.h"

void interleaver(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& data_out,
    mod_t                    mod,     // 0=QPSK, 1=16QAM
    ap_uint<8>               n_syms,  // symbols to process
    ap_uint<1>               is_rx    // 0=interleave (TX), 1=deinterleave (RX)
);
