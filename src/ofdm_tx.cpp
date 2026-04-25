// ============================================================
// ofdm_tx.cpp  —  OFDM Transmitter (Vitis HLS)
// ============================================================
#include "ofdm_tx.h"
#include "ofdm_lut.h"   // PILOT_IDX, DATA_SC_IDX, ZC_I_LUT, ZC_Q_LUT, crc16_hdr
#include <cmath>


// ============================================================
// SECTION 2: Constellation Mappers
// ============================================================

// ── QPSK: 2 bits → complex symbol ────────────────────────────
// Gray coded, average power = 1.0
// bit1 → I:  0 = +0.7071   1 = -0.7071
// bit0 → Q:  0 = +0.7071   1 = -0.7071
static csample_t qpsk_map(ap_uint<2> b) {
    #pragma HLS INLINE
    const sample_t k = sample_t(0.7071);
    return csample_t(
        b[1] ? sample_t(-0.7071) : k,
        b[0] ? sample_t(-0.7071) : k
    );
}

// ── 16-QAM: 4 bits → complex symbol ──────────────────────────
// Gray coded, average power = 1.0  (scale = 1/sqrt(10))
// I: bits[3:2]   Q: bits[1:0]
// Mapping: 00→+3/√10  01→+1/√10  11→-1/√10  10→-3/√10
static csample_t qam16_map(ap_uint<4> b) {
    #pragma HLS INLINE
    const sample_t hi = sample_t(0.9487);    // 3/sqrt(10)
    const sample_t lo = sample_t(0.3162);    // 1/sqrt(10)

    sample_t i_val, q_val;
    switch (b(3, 2)) {
        case 0:  i_val =  hi; break;
        case 1:  i_val =  lo; break;
        case 3:  i_val = -lo; break;
        default: i_val = -hi; break;   // case 2
    }
    switch (b(1, 0)) {
        case 0:  q_val =  hi; break;
        case 1:  q_val =  lo; break;
        case 3:  q_val = -lo; break;
        default: q_val = -hi; break;   // case 2
    }
    return csample_t(i_val, q_val);
}

// ============================================================
// SECTION 3: Bit Unpacker  (DATAFLOW stage 1)
//
// Reads packed bytes from the AXI-Stream input and emits one
// ap_uint<4> per data subcarrier into sym_fifo.
//
// QPSK:  1 byte → 4 symbols  (bits [7:6],[5:4],[3:2],[1:0])
//        50 bytes per OFDM symbol (200 data SCs × 2 bits / 8)
//
// 16QAM: 1 byte → 2 symbols  (upper nibble, lower nibble)
//        100 bytes per OFDM symbol (200 data SCs × 4 bits / 8)
//
// Both branches produce exactly NUM_DATA_SC (200) words into sym_fifo.
// The modulation mode is a runtime signal; HLS synthesises both
// branches with a mux on the bits_in read enable.
// ============================================================
static void unpack_bits(
    hls::stream<ap_uint<8>> &bits_in,
    hls::stream<ap_uint<4>> &sym_fifo,
    mod_t                    mod
) {
    #pragma HLS INLINE off

    if (mod == 0) {
        // QPSK: 4 symbols per byte.
        // Loop trip count = 50.  Each iteration reads 1 byte and writes 4 words.
        // Net throughput: 1 symbol/cycle (II=4 per iteration × 4 outputs).
        UNPACK_QPSK: for (int i = 0; i < NUM_DATA_SC / 4; i++) {
            #pragma HLS PIPELINE II=4
            ap_uint<8> b = bits_in.read();
            sym_fifo.write((ap_uint<4>)b(7, 6));
            sym_fifo.write((ap_uint<4>)b(5, 4));
            sym_fifo.write((ap_uint<4>)b(3, 2));
            sym_fifo.write((ap_uint<4>)b(1, 0));
        }
    } else {
        // 16-QAM: 2 symbols per byte.
        // Loop trip count = 100. Each iteration reads 1 byte and writes 2 words.
        // Net throughput: 1 symbol/cycle (II=2 per iteration × 2 outputs).
        UNPACK_16QAM: for (int i = 0; i < NUM_DATA_SC / 2; i++) {
            #pragma HLS PIPELINE II=2
            ap_uint<8> b = bits_in.read();
            sym_fifo.write(b(7, 4));
            sym_fifo.write(b(3, 0));
        }
    }
}

// ============================================================
// SECTION 4: Frequency Buffer Filler  (DATAFLOW stage 2)
//
// Reads 200 symbol words from sym_fifo, maps to constellation,
// and writes them into the correct FFT bins.
// Also zeros null bins and inserts fixed BPSK pilots.
// ============================================================
static void fill_freq_buffer(
    hls::stream<ap_uint<4>> &sym_fifo,
    csample_t                freq_buf[FFT_SIZE],
    mod_t                    mod
) {
    #pragma HLS INLINE off

    // Zero all 256 bins first (covers guards, DC, Nyquist)
    ZERO_LOOP: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        freq_buf[i] = csample_t(sample_t(0), sample_t(0));
    }

    // Insert BPSK pilots (+1 on I, 0 on Q)
    // NOTE: ap_fixed<16,1> range is [-1, +1) — +1.0 wraps to -1.0.
    // Use max representable value: 1 - 2^-15 = 0.999969.
    PILOT_LOOP: for (int p = 0; p < NUM_PILOT_SC; p++) {
        #pragma HLS UNROLL
        freq_buf[PILOT_IDX[p]] = csample_t(sample_t(0.999969), sample_t(0));
    }

    // Map symbols to data bins using precomputed index ROM.
    // DATA_SC_IDX is a ROM → HLS resolves the address each cycle → II=1.
    DATA_MAP: for (int d = 0; d < NUM_DATA_SC; d++) {
        #pragma HLS PIPELINE II=1
        ap_uint<4> bits = sym_fifo.read();
        csample_t sym = (mod == 0) ? qpsk_map(bits(1, 0)) : qam16_map(bits);
        freq_buf[DATA_SC_IDX[d]] = sym;
    }
}

// ============================================================
// SECTION 5: IFFT  (DATAFLOW stage 3)
//
// Calls the Xilinx HLS FFT IP in inverse (IFFT) mode.
// Scaling schedule 0x2AB: conservative per-stage scaling for
// 256-point transform — prevents overflow for typical OFDM PAPR.
//
// Separate in/out arrays are required for DATAFLOW so HLS can
// insert ping-pong buffers between this stage and its neighbours.
// ============================================================
// Stream 256 freq-domain samples to the external xfft IP and read back
// 256 time-domain samples.  The xfft core is configured for 256-pt IFFT
// with scale_sch=0xAA (÷256 total) via its s_axis_config port in the BD.
static void run_ifft(
    csample_t            in[FFT_SIZE],
    csample_t            out[FFT_SIZE],
    hls::stream<iq32_t> &ifft_in,
    hls::stream<iq32_t> &ifft_out
) {
    #pragma HLS INLINE off

#ifndef __SYNTHESIS__
    // C-sim: radix-2 Cooley-Tukey IFFT, matches hardware xfft scale_sch=0xAA (÷N).
    float re[FFT_SIZE], im[FFT_SIZE];
    for (int i = 0; i < FFT_SIZE; i++) {
        re[i] = (float)in[i].real();
        im[i] = (float)in[i].imag();
    }
    // Bit-reversal permutation
    for (int i = 1, j = 0; i < FFT_SIZE; i++) {
        int bit = FFT_SIZE >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t;
            t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    // Butterfly stages — inverse: twiddle = exp(+j2π/len)
    for (int len = 2; len <= FFT_SIZE; len <<= 1) {
        float ang = +2.0f * 3.14159265f / len;
        float wre = cosf(ang), wim = sinf(ang);
        for (int i = 0; i < FFT_SIZE; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float ur = re[i+k],           ui = im[i+k];
                float vr = re[i+k+len/2]*cr - im[i+k+len/2]*ci;
                float vi = re[i+k+len/2]*ci + im[i+k+len/2]*cr;
                re[i+k]       = ur + vr;  im[i+k]       = ui + vi;
                re[i+k+len/2] = ur - vr;  im[i+k+len/2] = ui - vi;
                float nr = cr*wre - ci*wim;  ci = cr*wim + ci*wre;  cr = nr;
            }
        }
    }
    // Scale by 1/N
    for (int i = 0; i < FFT_SIZE; i++)
        out[i] = csample_t((sample_t)(re[i] / FFT_SIZE), (sample_t)(im[i] / FFT_SIZE));
    (void)ifft_in; (void)ifft_out;
#else
    // No TLAST driven to xfft: frame length is fixed at IP-build time
    // (FFT_SIZE=256), so the xfft generates its own output TLAST and
    // ignores s_axis_data_tlast on its input port.
    IFFT_TX: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        iq32_t s;
        s.i = in[i].real();
        s.q = in[i].imag();
        ifft_in.write(s);
    }

    IFFT_RX: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        iq32_t s = ifft_out.read();
        out[i] = csample_t(s.i, s.q);
    }
#endif
}

// ============================================================
// SECTION 6: Cyclic Prefix Insertion + Output  (DATAFLOW stage 4)
//
// Emits [CP (32 samples)][Symbol (256 samples)] = 288 samples total.
// Sets iq_t.last = 1 on the final sample of the last symbol only.
// ============================================================
static void insert_cp_and_send(
    csample_t         sym[FFT_SIZE],
    hls::stream<iq_t> &out,
    bool               is_last
) {
    #pragma HLS INLINE off

    iq_t s;

    // Cyclic prefix: last CP_LEN samples of the IFFT output
    CP_LOOP: for (int i = FFT_SIZE - CP_LEN; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        s.i    = sym[i].real();
        s.q    = sym[i].imag();
        s.last = 0;
        out.write(s);
    }

    // Symbol body
    SYM_LOOP: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        s.i    = sym[i].real();
        s.q    = sym[i].imag();
        s.last = (is_last && (i == FFT_SIZE - 1)) ? ap_uint<1>(1) : ap_uint<1>(0);
        out.write(s);
    }
}

// ============================================================
// SECTION 7: Symbol Processor  (DATAFLOW wrapper)
//
// Chains the four stages with HLS DATAFLOW so they run as a
// pipeline across consecutive OFDM symbols.
//
// Within a single symbol:
//   unpack_bits   →  sym_fifo (stream, depth=200)
//   fill_freq_buf →  freq_buf (array, ping-pong buffered by HLS)
//   run_ifft      →  time_buf (array, ping-pong buffered by HLS)
//   insert_cp_and_send → iq_out (AXI stream)
//
// Across symbols: while symbol N is in run_ifft, symbol N+1 is
// already being filled (fill_freq_buf runs concurrently on the
// other buffer bank).
// ============================================================
static void process_symbol(
    hls::stream<ap_uint<8>> &bits_in,
    hls::stream<iq_t>        &iq_out,
    hls::stream<iq32_t>      &ifft_in,
    hls::stream<iq32_t>      &ifft_out,
    mod_t                     mod,
    bool                      is_last
) {
    #pragma HLS INLINE off
    // DATAFLOW removed: external stream IFFT cannot pipeline across symbols
    // on the same port — sequential execution is correct and simpler.

    hls::stream<ap_uint<4>> sym_fifo("sym_fifo");
    #pragma HLS STREAM variable=sym_fifo depth=NUM_DATA_SC

    csample_t freq_buf[FFT_SIZE];
    csample_t time_buf[FFT_SIZE];
    #pragma HLS BIND_STORAGE variable=freq_buf type=RAM_1P impl=BRAM
    #pragma HLS BIND_STORAGE variable=time_buf type=RAM_1P impl=BRAM

    unpack_bits(bits_in, sym_fifo, mod);
    fill_freq_buffer(sym_fifo, freq_buf, mod);
    run_ifft(freq_buf, time_buf, ifft_in, ifft_out);
    insert_cp_and_send(time_buf, iq_out, is_last);
}

// ============================================================
// SECTION 8: Shared IFFT + CP + Send helper
//
// Called sequentially by send_preamble and send_header.
// INLINE off → one RTL module; both callers share it.
// ============================================================
static void send_freq_symbol(
    csample_t             freq[FFT_SIZE],
    hls::stream<iq_t>    &out,
    hls::stream<iq32_t>  &ifft_in,
    hls::stream<iq32_t>  &ifft_out,
    bool                  is_last
) {
    #pragma HLS INLINE off
    csample_t time_buf[FFT_SIZE];
    #pragma HLS BIND_STORAGE variable=time_buf type=RAM_1P impl=BRAM

    run_ifft(freq, time_buf, ifft_in, ifft_out);
    insert_cp_and_send(time_buf, out, is_last);
}

// ============================================================
// SECTION 8a: Preamble
//
// One OFDM symbol using the Zadoff-Chu sequence (root u=25).
// ZC has constant envelope in time → ideal for AGC settling,
// timing detection, and initial frequency offset estimation.
//
// Pilot bins carry BPSK +1 (same as data symbols) for
// consistency; data bins carry the precomputed ZC LUT values.
// ============================================================
static void send_preamble(
    hls::stream<iq_t>    &out,
    hls::stream<iq32_t>  &ifft_in,
    hls::stream<iq32_t>  &ifft_out
) {
    #pragma HLS INLINE  // inlined into ofdm_tx so send_freq_symbol can be shared

    csample_t freq[FFT_SIZE];
    #pragma HLS BIND_STORAGE variable=freq type=RAM_1P impl=BRAM

    // Zero all bins
    PRE_ZERO: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        freq[i] = csample_t(sample_t(0), sample_t(0));
    }

    // Pilot bins: BPSK +1 (max Q0.15 value — 1.0 overflows ap_fixed<16,1>)
    PRE_PILOT: for (int p = 0; p < NUM_PILOT_SC; p++) {
        #pragma HLS UNROLL
        freq[PILOT_IDX[p]] = csample_t(sample_t(0.999969), sample_t(0));
    }

    // Data bins: ZC sequence from ROM
    PRE_DATA: for (int d = 0; d < NUM_DATA_SC; d++) {
        #pragma HLS PIPELINE II=1
        freq[DATA_SC_IDX[d]] = csample_t(ZC_I_LUT[d], ZC_Q_LUT[d]);
    }

    // Preamble is never the last symbol (header follows)
    send_freq_symbol(freq, out, ifft_in, ifft_out, false);
}

// ============================================================
// SECTION 8b: Frame Header Symbol
// (CRC-16/CCITT helper crc16_hdr() lives in ofdm_lut.h)
//
// One OFDM symbol carrying the 26-bit frame header on the first
// 26 data subcarriers (DATA_SC_IDX[0..25]) using BPSK:
//   bit=0 → +0.999969,  bit=1 → -0.999969
//
// Header layout (MSB first in frequency order, d=0 → MSB):
//   bits [25:10] = CRC-16/CCITT over bits [9:0]
//   bits [9:2]   = n_syms[7:0]  (number of data symbols)
//   bits [1]     = 0  (reserved)
//   bits [0]     = mod (0=QPSK, 1=16QAM)
//
// DATA_SC_IDX[26..199] = zero.
// Pilots at PILOT_IDX carry +1 (BPSK, same as preamble and data symbols).
// Header is never the last symbol (data follows).
// ============================================================
static void send_header(
    hls::stream<iq_t>    &out,
    hls::stream<iq32_t>  &ifft_in,
    hls::stream<iq32_t>  &ifft_out,
    modcod_t              modcod,
    ap_uint<8>            n_syms
) {
    #pragma HLS INLINE  // inlined into ofdm_tx so send_freq_symbol can be shared

    // Build 26-bit header word.
    // payload layout: [ n_syms(8) : modcod(2) ]  →  modcod = {mod, rate}
    ap_uint<10> payload = ((ap_uint<10>)n_syms << 2) | ap_uint<10>(modcod & 0x3);
    ap_uint<16> crc     = crc16_hdr(payload);
    ap_uint<26> hdr     = ((ap_uint<26>)crc << 10) | (ap_uint<26>)payload;

    csample_t freq[FFT_SIZE];
    #pragma HLS BIND_STORAGE variable=freq type=RAM_1P impl=BRAM

    // Zero all bins
    HDR_ZERO: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        freq[i] = csample_t(sample_t(0), sample_t(0));
    }

    // Pilot bins: BPSK +1
    HDR_PILOT: for (int p = 0; p < NUM_PILOT_SC; p++) {
        #pragma HLS UNROLL
        freq[PILOT_IDX[p]] = csample_t(sample_t(0.999969), sample_t(0));
    }

    // Header bits on first 26 data SCs; remaining data SCs stay zero
    HDR_DATA: for (int d = 0; d < 26; d++) {
        #pragma HLS PIPELINE II=1
        ap_uint<1> bit = hdr[25 - d];   // MSB first
        sample_t val = bit ? sample_t(-0.999969) : sample_t(0.999969);
        freq[DATA_SC_IDX[d]] = csample_t(val, sample_t(0));
    }

    // Header is never the last symbol (data follows)
    send_freq_symbol(freq, out, ifft_in, ifft_out, false);
}

// ============================================================
// SECTION 9: Top-Level OFDM TX
// ============================================================
void ofdm_tx(
    hls::stream<ap_uint<8>> &bits_in,
    hls::stream<iq_t>        &iq_out,
    hls::stream<iq32_t>      &ifft_in,
    hls::stream<iq32_t>      &ifft_out,
    modcod_t                  modcod,    // 2-bit: modcod[1]=mod, modcod[0]=rate
    ap_uint<8>                n_syms
) {
    #pragma HLS INTERFACE axis      port=bits_in
    #pragma HLS INTERFACE axis      port=iq_out
    #pragma HLS INTERFACE axis      port=ifft_in
    #pragma HLS INTERFACE axis      port=ifft_out
    #pragma HLS INTERFACE s_axilite port=modcod  bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=n_syms  bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return  bundle=ctrl

    // Decode modcod: mod drives the QAM mapper in unpack_bits / process_symbol.
    mod_t mod = (mod_t)modcod[1];

    // C2 fix: send FFT_SIZE+CP_LEN (288) null samples before the ZC preamble.
    // sync_detect's search window is designed for best_t = 288 samples,
    // meaning the preamble CP starts at sample 288 of the received stream.
    // Without this guard, the ADC stream has no guaranteed silence before the
    // preamble — sync_detect can lock to noise and output the wrong offset.
    GUARD: for (int i = 0; i < FFT_SIZE + CP_LEN; i++) {
        #pragma HLS PIPELINE II=1
        iq_t s;
        s.i    = sample_t(0);
        s.q    = sample_t(0);
        s.last = ap_uint<1>(0);
        iq_out.write(s);
    }

    send_preamble(iq_out, ifft_in, ifft_out);
    send_header(iq_out, ifft_in, ifft_out, modcod, n_syms);

    SYMBOL_LOOP: for (ap_uint<8> s = 0; s < n_syms; s++) {
        bool is_last = (s == (ap_uint<8>)(n_syms - 1));
        process_symbol(bits_in, iq_out, ifft_in, ifft_out, mod, is_last);
    }
}
