// ============================================================
// ofdm_tx_tb.cpp  —  Vitis HLS C Simulation Testbench
//
// Flow:
//   1. Read raw data bytes from  tb_input_to_tx.bin
//   2. FEC encode (conv_enc) → coded bytes
//   3. Run ofdm_tx() C simulation
//   4. Write IQ output to  tb_tx_output_hls.txt  (one "I Q" pair per line)
//   5. Python then loads both files and compares (see ofdm_reference.py)
// ============================================================
#include "ofdm_tx.h"
#include "conv_fec.h"
#include "scrambler.h"
#include "interleaver.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstring>

// tb_path: resolves a filename relative to HLS_TBDATA_DIR if set.
// During C-sim  : env var unset → uses plain relative path (current dir = build/).
// During RTL cosim: set HLS_TBDATA_DIR to project root in the TCL script so
//                   cosim.tv.exe (which runs in wrapc/) can find the test vectors.
static std::string tb_path(const char* fname) {
    const char* dir = std::getenv("HLS_TBDATA_DIR");
    return dir ? (std::string(dir) + "/" + fname) : fname;
}

// Test configuration — must match values used in ofdm_reference.py
#define TB_N_SYMS   255
#define TB_FEC_RATE 0   // 0 = rate 1/2, 1 = rate 2/3

// Expected output file written by this testbench
#define OUT_FILE   "tb_tx_output_hls.txt"
// Input bits file written by ofdm_reference.py
#define IN_FILE    "tb_input_to_tx.bin"

int main(int argc, char* argv[]) {
    // Runtime modulation: --mod 0 = QPSK, --mod 1 = 16QAM (default)
    int TB_MOD = 1;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--mod" && i + 1 < argc)
            TB_MOD = std::atoi(argv[++i]);
    hls::stream<ap_uint<8>> raw_in("raw_in");
    hls::stream<ap_uint<8>> scrambled("scrambled");
    hls::stream<ap_uint<8>> coded_in("coded_in");
    hls::stream<ap_uint<8>> interleaved("interleaved");
    hls::stream<iq_t>       iq_out("iq_out");

    // Coded bytes = what ofdm_tx consumes (one byte per packed constellation)
    // QPSK:  4 symbols/byte → 50 coded bytes/symbol
    // 16QAM: 2 symbols/byte → 100 coded bytes/symbol
    const int coded_per_sym  = (TB_MOD == 0) ? (NUM_DATA_SC / 4)
                                              : (NUM_DATA_SC / 2);
    const int total_coded    = TB_N_SYMS * coded_per_sym;

    // Raw bytes = data before FEC encoding
    // rate 1/2: half as many raw bytes as coded bytes
    // rate 2/3: two thirds as many raw bytes as coded bytes
    const int total_raw = (TB_FEC_RATE == 0) ? (total_coded / 2)
                                              : (total_coded * 2 / 3);

    // ── Load raw data bytes ───────────────────────────────────
    std::ifstream fin(tb_path(IN_FILE), std::ios::binary);
    if (!fin.is_open()) {
        std::cerr << "[TB] ERROR: cannot open " << IN_FILE
                  << " — run ofdm_reference.py first\n";
        return 1;
    }

    int bytes_read = 0;
    while (bytes_read < total_raw && fin.good()) {
        unsigned char b;
        fin.read(reinterpret_cast<char*>(&b), 1);
        if (fin.gcount() == 1) {
            raw_in.write((ap_uint<8>)b);
            bytes_read++;
        }
    }
    fin.close();

    if (bytes_read != total_raw) {
        std::cerr << "[TB] ERROR: expected " << total_raw
                  << " raw bytes, got " << bytes_read << "\n";
        return 1;
    }
    std::cout << "[TB] Loaded " << bytes_read << " raw bytes"
              << "  (FEC rate=" << (TB_FEC_RATE == 0 ? "1/2" : "2/3")
              << "  coded=" << total_coded << " bytes)\n";

    // ── Scramble: raw bytes → scrambled bytes ─────────────────
    scrambler(raw_in, scrambled, (ap_uint<16>)total_raw);

    // ── FEC encode: scrambled bytes → coded bytes ─────────────
    conv_enc(scrambled, coded_in, (rate_t)TB_FEC_RATE, total_raw);

    // ── Interleave: coded bytes → frequency-domain order ──────
    interleaver(coded_in, interleaved, (mod_t)TB_MOD, (ap_uint<8>)TB_N_SYMS,
                /*is_rx=*/ap_uint<1>(0));

    // ── Run OFDM TX ───────────────────────────────────────────
    hls::stream<iq_t> ifft_in_s("ifft_in"), ifft_out_s("ifft_out");
    ofdm_tx(interleaved, iq_out, ifft_in_s, ifft_out_s, (mod_t)TB_MOD, (ap_uint<8>)TB_N_SYMS);

    // ── Dump output ───────────────────────────────────────────
    // Format: one "I Q\n" pair per sample, floating-point.
    // Includes preamble symbol + TB_N_SYMS data symbols.
    // Total expected samples: (TB_N_SYMS + 1) × (FFT_SIZE + CP_LEN)
    //                       = (4 + 1) × 288 = 1440
    std::ofstream fout(tb_path(OUT_FILE));
    if (!fout.is_open()) {
        std::cerr << "[TB] ERROR: cannot open " << OUT_FILE << " for writing\n";
        return 1;
    }

    int sample_count = 0;
    while (!iq_out.empty()) {
        iq_t s = iq_out.read();
        fout << s.i.to_double() << " " << s.q.to_double() << "\n";
        sample_count++;
    }
    fout.close();

    const int expected_samples = (TB_N_SYMS + 2) * (FFT_SIZE + CP_LEN);  // +preamble +header
    std::cout << "[TB] Wrote " << sample_count << " samples to " << OUT_FILE
              << " (expected " << expected_samples << ")\n";

    if (sample_count != expected_samples) {
        std::cerr << "[TB] FAIL: sample count mismatch\n";
        return 1;
    }

    std::cout << "[TB] PASS: sample count correct — run ofdm_reference.py "
                 "to compare values\n";
    return 0;
}
