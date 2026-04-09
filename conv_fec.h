// ============================================================
// conv_fec.h  —  K=7 Convolutional FEC, 802.11a polynomials
//
// Rates supported:
//   rate_t = 0  →  rate 1/2  (no puncturing)
//   rate_t = 1  →  rate 2/3  (802.11a puncture matrix [[1,1],[1,0]])
//
// Polynomials (MSB = current input):
//   G0 = 0133 octal = 1011011 binary = 0x5B
//   G1 = 0171 octal = 1111001 binary = 0x79
//
// Byte format: MSB first throughout (matches ofdm_tx unpack_bits convention)
// ============================================================
#pragma once
#include <ap_int.h>
#include <hls_stream.h>

typedef ap_uint<1> rate_t;   // 0 = rate 1/2,  1 = rate 2/3

// K=7 code parameters
#define CONV_K      7
#define N_STATES    64        // 2^(K-1)
#define TB_DEPTH    96        // Viterbi traceback depth (≥5×K)

// C-sim sizing — covers 16-QAM rate 2/3 255 symbols (136,000 data bits)
// NOTE: synthesis requires sliding-window Viterbi (TB_DEPTH×64 decision bits)
#define MAX_DATA_BITS  145000

// ── Function declarations ─────────────────────────────────────

// Encoder: raw data bytes → packed coded bytes (MSB first)
//   rate 1/2:  n_coded_bytes = 2 × n_data_bytes
//   rate 2/3:  n_coded_bytes = (3/2) × n_data_bytes  (n_data_bytes must be even)
void conv_enc(
    hls::stream<ap_uint<8>>& data_in,
    hls::stream<ap_uint<8>>& coded_out,
    rate_t rate,
    int    n_data_bytes);

// Decoder: packed coded bytes → raw data bytes
//   n_data_bytes: number of expected output bytes (determines trellis length)
void viterbi_dec(
    hls::stream<ap_uint<8>>& coded_in,
    hls::stream<ap_uint<8>>& data_out,
    rate_t rate,
    int    n_data_bytes);
