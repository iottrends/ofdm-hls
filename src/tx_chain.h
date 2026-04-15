// ============================================================
// tx_chain.h  —  Merged TX bit-processing chain
//
// scrambler → conv_enc → interleaver  (single HLS IP, DATAFLOW)
//
// Reduces Vivado block-design IP count: 3 separate HLS IPs (each with
// its own AXIS + s_axilite ports) collapse into one block that still
// runs its sub-stages concurrently via hls::stream FIFOs.
//
// Control interface
// ─────────────────
//   n_data_bytes : input payload size (before scrambling)
//   rate         : 0 = rate 1/2,  1 = rate 2/3         (conv_enc)
//   mod          : 0 = QPSK,      1 = 16-QAM           (interleaver)
//   n_syms       : OFDM symbols after interleaving     (interleaver)
//
// Bit / byte-count bookkeeping is the responsibility of the caller —
// the same way it was when the three blocks were separate IPs.
// ============================================================
#pragma once
#include "scrambler.h"
#include "conv_fec.h"
#include "interleaver.h"

void tx_chain(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& data_out,
    ap_uint<16>              n_data_bytes,
    modcod_t                 modcod,       // 2-bit: modcod[1]=mod, modcod[0]=rate
    ap_uint<8>               n_syms
);
