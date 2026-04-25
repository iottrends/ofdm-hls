// ============================================================
// fec_rx.cpp  —  Merged RX bit-processing chain (DATAFLOW)
//
//   data_in ──► interleaver(is_rx=1) ──► viterbi_dec ──► scrambler ──► data_out
//                                   s1                s2
// ============================================================
#include "fec_rx.h"
#include "free_run.h"   // WAIT_NONEMPTY_OR (csim escape for stream poll)

void fec_rx(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& data_out,
    modcod_t                 modcod,
    ap_uint<8>               n_syms
) {
    #pragma HLS INTERFACE axis      port=data_in
    #pragma HLS INTERFACE axis      port=data_out
    #pragma HLS INTERFACE ap_none   port=modcod
    #pragma HLS INTERFACE ap_none   port=n_syms
    // Free-running: ap_ctrl_none, body is while(1).  Input stream stalls
    // between packets (upstream sync_detect gate closed) → block idles
    // naturally, no ap_start required.
    #pragma HLS INTERFACE ap_ctrl_none port=return

    static const int BPS_TBL[4] = {25, 33, 50, 66};

    FREE_RUN: while (1) {
        #pragma HLS PIPELINE off

        // csim escape: in hardware data_in stalls between packets and the
        // block idles — but the loop never exits.  In csim, wait briefly for
        // new bytes; if none arrive, return so the testbench can complete.
        WAIT_NONEMPTY_OR(data_in, return);

        // Per-packet: re-sample the ap_none wires.  ofdm_rx drives them
        // with the decoded modcod / n_syms for the current inbound packet.
        mod_t     mod  = (mod_t)modcod[1];
        rate_t    rate = (rate_t)modcod[0];
        ap_uint<16> n_data_bytes = (ap_uint<16>)n_syms * (ap_uint<16>)BPS_TBL[modcod];

        {
            // Single-packet DATAFLOW region — three sub-stages pipelined.
            #pragma HLS DATAFLOW
            hls::stream<ap_uint<8>> s1;
            hls::stream<ap_uint<8>> s2;
            #pragma HLS STREAM variable=s1 depth=128
            #pragma HLS STREAM variable=s2 depth=128

            interleaver(data_in, s1, mod, n_syms, /*is_rx=*/1);
            viterbi_dec(s1, s2, rate, (int)n_data_bytes);
            scrambler(s2, data_out, n_data_bytes);
        }
    }
}
