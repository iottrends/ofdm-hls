// ============================================================
// ofdm_rx_tb.cpp  —  Vitis HLS C Simulation Testbench for RX
//
// Full RX chain under test (post-merge architecture):
//   sync_detect → ofdm_rx → viterbi_dec → scrambler
//
// sync_detect now folds cfo_correct inside its FSM, so the standalone
// cfo_correct call is gone.  The block is also free-running (ap_ctrl_none
// + while(1)); the csim escape in src/sync_detect.cpp returns when iq_raw
// drains.  Same for ofdm_rx.
//
// n_syms_fb feedback: in hardware, ofdm_rx pulses n_syms_fb_vld for one
// cycle after BPSK header decode.  In this single-frame csim TB we
// hard-wire vld=1 and n_syms_fb=TB_N_SYMS — the FSM transitions through
// SEARCH → FWD_PREHDR → WAIT_NSYMS → FWD_DATA exactly once, exercising
// every state.  Multi-frame / channel-impairment tests need the pthread
// orchestration from docs/RX_GATING_DESIGN.md.
//
// Flow:
//   1. Read TX output from  tb_tx_output_hls.txt  (ofdm_tx C-sim output)
//   2. Pre-pad iq_raw with 4096 zeros to clear sync_detect's warmup gate
//   3. Feed IQ samples into sync_detect() → derotated, gated IQ
//   4. ofdm_rx() → coded bytes
//   5. interleaver(rx)/viterbi_dec/scrambler → raw data bytes
//   6. Write decoded bytes to tb_rx_decoded_hls.bin
//   7. Compare with original tb_input_to_tx.bin (raw bytes)
//   8. Report byte error count and BER
// ============================================================
#include "sync_detect.h"     // pulls in ofdm_rx.h → ofdm_tx.h
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
    hls::stream<ap_uint<8>>  coded_out("coded_out");
    hls::stream<ap_uint<8>>  deinterleaved("deinterleaved");
    hls::stream<ap_uint<8>>  raw_out("raw_out");
    hls::stream<ap_uint<8>>  descrambled_out("descrambled_out");

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

    // sync_detect's warmup gate needs ≥ BUF_SIZE (4096) samples in the SRL
    // delay lines + power-envelope before it can arm.  The TX file starts
    // with only SYNC_NL=288 guard zeros, so pre-pad iq_raw with extra
    // zeros to clear warmup.  Tail-pad with another symbol so FWD_DATA can
    // complete after the trigger consumes ~preamble samples of slack.
    const int warmup_pad    = 4096;
    const int tail_pad      = FFT_SIZE + CP_LEN;   // 288 — covers trigger lag
    const int total_syms    = TB_N_SYMS + 3;       // +guard +preamble +header
    const int total_samples = warmup_pad + total_syms * (FFT_SIZE + CP_LEN) + tail_pad;

    // Push warmup zeros first
    for (int k = 0; k < warmup_pad; k++) {
        iq_t s;
        s.i = sample_t(0); s.q = sample_t(0); s.last = 0;
        iq_raw.write(s);
    }

    int samples_loaded = warmup_pad;
    double i_val, q_val;
    const int file_end = warmup_pad + total_syms * (FFT_SIZE + CP_LEN);
    while (ftx >> i_val >> q_val && samples_loaded < file_end) {
        iq_t s;
        s.i    = sample_t(i_val);
        s.q    = sample_t(q_val);
        s.last = (samples_loaded == file_end - 1) ? ap_uint<1>(1) : ap_uint<1>(0);
        iq_raw.write(s);
        samples_loaded++;
    }
    ftx.close();

    if (samples_loaded != file_end) {
        std::cerr << "[TB] ERROR: expected " << (file_end - warmup_pad)
                  << " samples from file, got " << (samples_loaded - warmup_pad) << "\n";
        return 1;
    }

    // Tail-pad zeros so FWD_DATA can complete past the last data symbol.
    for (int k = 0; k < tail_pad; k++) {
        iq_t s;
        s.i = sample_t(0); s.q = sample_t(0); s.last = 0;
        iq_raw.write(s);
        samples_loaded++;
    }

    std::cout << "[TB] Loaded " << samples_loaded << " IQ samples ("
              << warmup_pad << " warmup zeros + "
              << (file_end - warmup_pad) << " from " << TX_OUT_FILE
              << " + " << tail_pad << " tail zeros)\n";

    // ── Step 1: sync_detect — preamble gate + inline CFO derotation ──
    // Hard-wire n_syms_fb_vld=1 + n_syms_fb=TB_N_SYMS so the FSM
    // transitions WAIT_NSYMS → FWD_DATA without needing the real-time
    // feedback wire from ofdm_rx (csim is sequential).
    ap_ufixed<24,8> pow_threshold = ap_ufixed<24,8>(0.001);
    ap_uint<32>     stat_preamble_count   = 0;
    ap_uint<32>     stat_header_bad_count = 0;
    ap_ufixed<24,8> stat_pow_env          = 0;

    sync_detect(iq_raw, iq_aligned,
                (ap_uint<8>)TB_N_SYMS, (ap_uint<1>)1,
                pow_threshold,
                stat_preamble_count, stat_header_bad_count, stat_pow_env);

    std::cout << "[TB] sync_detect drained.  preamble_count="
              << (uint32_t)stat_preamble_count
              << "  header_bad_count=" << (uint32_t)stat_header_bad_count
              << "  iq_aligned size=" << iq_aligned.size() << "\n";

    if (iq_aligned.size() == 0) {
        std::cerr << "[TB] FAIL: sync_detect produced no aligned samples — "
                  << "preamble not detected within warmup window\n";
        return 1;
    }

    // ── Step 2: ofdm_rx — preamble equalize, header decode, data demap ──
    hls::stream<iq32_t> fft_in_s("fft_in"), fft_out_s("fft_out");
    modcod_t   rx_modcod_out = 0;
    ap_uint<8> rx_nsyms_out  = 0;
    ap_uint<8> rx_nsyms_fb   = 0;
    ofdm_rx(iq_aligned, coded_out, fft_in_s, fft_out_s, header_err,
            rx_modcod_out, rx_nsyms_out, rx_nsyms_fb);
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
