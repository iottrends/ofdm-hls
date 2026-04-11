// ============================================================
// ofdm_rx_tb.cpp  —  Vitis HLS C Simulation Testbench for RX
//
// Full RX chain under test:
//   sync_detect → cfo_correct → ofdm_rx → viterbi_dec
//
// Flow:
//   1. Read TX output from  tb_tx_output_hls.txt  (ofdm_tx C-sim output)
//   2. Feed IQ samples into sync_detect() → timing-aligned IQ + CFO estimate
//   3. cfo_correct() applies phase rotation (pass-through when CFO = 0)
//   4. ofdm_rx() → coded bytes
//   5. viterbi_dec() → raw data bytes
//   6. Write decoded bytes to  tb_rx_decoded_hls.bin
//   7. Compare with original  tb_input_to_tx.bin  (raw bytes)
//   8. Report byte error count and BER
//
// Run order:
//   python3 ofdm_reference.py --gen          # create input bits + TX ref
//   [run TX C-sim]                           # creates tb_tx_output_hls.txt
//   [run RX C-sim]                           # this testbench
// ============================================================
#include "sync_detect.h"     // pulls in ofdm_rx.h → ofdm_tx.h
#include "cfo_correct.h"
#include "ofdm_rx.h"
#include "conv_fec.h"
#include "scrambler.h"
#include "interleaver.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>

// tb_path: resolves a filename via HLS_TBDATA_DIR if set (cosim), else relative (csim).
static std::string tb_path(const char* fname) {
    const char* dir = std::getenv("HLS_TBDATA_DIR");
    return dir ? (std::string(dir) + "/" + fname) : fname;
}

// Test configuration — must match TX testbench
#define TB_N_SYMS    255

#define TX_OUT_FILE  "tb_tx_output_hls.txt"    // TX C-sim output (IQ pairs)
#define IN_BITS_FILE "tb_input_to_tx.bin"      // original packed bits
#define RX_OUT_FILE  "tb_rx_decoded_hls.bin"   // decoded bytes from RX

int main(int argc, char* argv[]) {
    int TB_MOD      = 1;   // 0=QPSK, 1=16QAM
    int TB_FEC_RATE = 0;   // 0=rate-1/2, 1=rate-2/3
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--mod"  && i + 1 < argc) TB_MOD      = std::atoi(argv[++i]);
        if (std::string(argv[i]) == "--rate" && i + 1 < argc) TB_FEC_RATE = std::atoi(argv[++i]);
    }
    hls::stream<iq_t>        iq_raw("iq_raw");
    hls::stream<iq_t>        iq_aligned("iq_aligned");
    hls::stream<iq_t>        iq_corrected("iq_corrected");
    hls::stream<ap_uint<8>>  coded_out("coded_out");
    hls::stream<ap_uint<8>>  deinterleaved("deinterleaved");
    hls::stream<ap_uint<8>>  raw_out("raw_out");
    hls::stream<ap_uint<8>>  descrambled_out("descrambled_out");

    cfo_t      cfo_est   = cfo_t(0);
    ap_uint<1> header_err = 0;

    // ── Load TX output into IQ stream ─────────────────────────
    // File format: one "I Q\n" per line, floating-point values.
    // Convert to ap_fixed<16,1> (Q0.15).
    std::ifstream ftx(tb_path(TX_OUT_FILE));
    if (!ftx.is_open()) {
        std::cerr << "[TB] ERROR: cannot open " << TX_OUT_FILE
                  << " — run TX C-sim first\n";
        return 1;
    }

    // C2 fix: ofdm_tx now sends SYNC_NL=288 guard zeros before the preamble,
    // so tb_tx_output_hls.txt already starts with the guard.  Total samples =
    // guard + preamble + header + data = (TB_N_SYMS + 3) symbols.
    // No need to prepend zeros manually — doing so would double the guard and
    // push the preamble CP to t=576=SEARCH_WIN (edge), breaking sync_detect.
    const int total_syms    = TB_N_SYMS + 3;  // +guard +preamble +header
    const int total_samples = total_syms * (FFT_SIZE + CP_LEN);

    int samples_loaded = 0;
    double i_val, q_val;
    while (ftx >> i_val >> q_val && samples_loaded < total_samples) {
        iq_t s;
        s.i    = sample_t(i_val);
        s.q    = sample_t(q_val);
        s.last = (samples_loaded == total_samples - 1) ? ap_uint<1>(1) : ap_uint<1>(0);
        iq_raw.write(s);
        samples_loaded++;
    }
    ftx.close();

    if (samples_loaded != total_samples) {
        std::cerr << "[TB] ERROR: expected " << total_samples
                  << " IQ samples, got " << samples_loaded << "\n";
        return 1;
    }
    std::cout << "[TB] Loaded " << samples_loaded << " IQ samples from "
              << TX_OUT_FILE << " (includes " << SYNC_NL << " guard zeros from TX)\n";

    // ── Step 1: Timing sync + CFO estimation ─────────────────
    sync_detect(iq_raw, iq_aligned, cfo_est, (ap_uint<8>)TB_N_SYMS);

    std::cout << "[TB] Timing locked."
              << "  CFO estimate = " << (float)cfo_est
              << " subcarrier spacings"
              << "  (" << (float)cfo_est * 78125.0f << " Hz at 20 MSPS / 256 SC)\n";

    // ── Step 2: CFO correction ────────────────────────────────
    // In simulation CFO = 0, so this is a pass-through.
    // In hardware, this undoes the carrier offset before the FFT.
    cfo_correct(iq_aligned, iq_corrected, cfo_est);

    // ── Step 3: OFDM demodulate → coded bytes ────────────────
    // mod and n_syms are now decoded from the in-band header symbol.
    hls::stream<iq_t> fft_in_s("fft_in"), fft_out_s("fft_out");
    ofdm_rx(iq_corrected, coded_out, fft_in_s, fft_out_s, header_err);
    if (header_err) {
        std::cerr << "[TB] FAIL: ofdm_rx header CRC error\n";
        return 1;
    }

    // ── Step 4a: Deinterleave: frequency order → coded order ──
    interleaver(coded_out, deinterleaved, (mod_t)TB_MOD, (ap_uint<8>)TB_N_SYMS,
                /*is_rx=*/ap_uint<1>(1));

    // ── Step 4b: FEC decode: coded bytes → raw bytes ──────────
    const int coded_per_sym = (TB_MOD == 0) ? (NUM_DATA_SC / 4)
                                            : (NUM_DATA_SC / 2);
    const int total_coded   = TB_N_SYMS * coded_per_sym;
    const int total_raw     = (TB_FEC_RATE == 0) ? (total_coded / 2)
                                                  : (total_coded * 2 / 3);

    viterbi_dec(deinterleaved, raw_out, (rate_t)TB_FEC_RATE, total_raw);

    // ── Step 5: Descramble ────────────────────────────────────
    scrambler(raw_out, descrambled_out, (ap_uint<16>)total_raw);

    // ── Collect decoded raw bytes ─────────────────────────────
    std::vector<unsigned char> decoded;
    decoded.reserve(total_raw);
    while (!descrambled_out.empty()) {
        ap_uint<8> b = descrambled_out.read();
        decoded.push_back((unsigned char)b.to_uint());
    }

    std::cout << "[TB] Decoded " << decoded.size() << " bytes "
              << "(expected " << total_raw << " raw bytes after FEC decode)\n";

    if ((int)decoded.size() != total_raw) {
        std::cerr << "[TB] FAIL: byte count mismatch\n";
        return 1;
    }

    // Write decoded bytes to file
    std::ofstream frx(tb_path(RX_OUT_FILE), std::ios::binary);
    if (!frx.is_open()) {
        std::cerr << "[TB] ERROR: cannot open " << RX_OUT_FILE << " for writing\n";
        return 1;
    }
    for (unsigned char c : decoded)
        frx.write(reinterpret_cast<char*>(&c), 1);
    frx.close();
    std::cout << "[TB] Wrote " << decoded.size() << " decoded bytes to "
              << RX_OUT_FILE << "\n";

    // ── Compare with original bits ────────────────────────────
    std::ifstream fref(tb_path(IN_BITS_FILE), std::ios::binary);
    if (!fref.is_open()) {
        std::cerr << "[TB] WARNING: " << IN_BITS_FILE
                  << " not found — skipping BER check\n";
        return 0;
    }

    std::vector<unsigned char> original;
    original.reserve(total_raw);
    unsigned char b;
    while (fref.read(reinterpret_cast<char*>(&b), 1))
        original.push_back(b);
    fref.close();

    if ((int)original.size() != total_raw) {
        std::cerr << "[TB] WARNING: " << IN_BITS_FILE << " has "
                  << original.size() << " bytes, expected " << total_raw
                  << " — skipping BER check\n";
        return 0;
    }

    // Byte-level comparison + bit error count
    int byte_errors = 0;
    int bit_errors  = 0;
    for (int i = 0; i < total_raw; i++) {
        if (decoded[i] != original[i]) {
            byte_errors++;
            unsigned char diff = decoded[i] ^ original[i];
            while (diff) {
                bit_errors += (diff & 1);
                diff >>= 1;
            }
        }
    }

    const int total_bits = total_raw * 8;
    double ber = (double)bit_errors / total_bits;

    std::cout << "[TB] Byte errors : " << byte_errors << " / "
              << total_raw << "\n";
    std::cout << "[TB] Bit  errors : " << bit_errors  << " / "
              << total_bits    << "\n";
    std::cout << "[TB] BER         : " << ber << "\n";

    if (bit_errors == 0) {
        std::cout << "[TB] PASS  — BER = 0  (perfect decode)\n";
        return 0;
    } else {
        std::cerr << "[TB] FAIL  — " << bit_errors << " bit errors\n";
        return 1;
    }
}
