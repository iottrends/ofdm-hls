// ============================================================
// conv_fec_tb.cpp  —  Standalone FEC Codec Testbench
//
// Tests:
//   1. Rate 1/2 clean  — encode → decode, expect BER = 0
//   2. Rate 2/3 clean  — encode → decode, expect BER = 0
//   3. Rate 1/2 noisy  — encode → flip bits → decode, measure BER
//   4. Rate 2/3 noisy  — encode → flip bits → decode, measure BER
//
// Usage: run as HLS C-sim (no external files needed)
// ============================================================
#include "conv_fec.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>

// Test parameters
#define TEST_BYTES      200      // data bytes per test (even, works for both rates)
#define NOISE_RATE_PCT    5      // % of coded bits to flip in noisy test
#define SEED             42

// ── Helpers ──────────────────────────────────────────────────

static void fill_random(unsigned char* buf, int n, unsigned int seed) {
    unsigned int s = seed;
    for (int i = 0; i < n; i++) {
        s = s * 1664525u + 1013904223u;
        buf[i] = (unsigned char)(s >> 24);
    }
}

static int count_bit_errors(const unsigned char* a, const unsigned char* b, int n) {
    int errs = 0;
    for (int i = 0; i < n; i++) {
        unsigned char diff = a[i] ^ b[i];
        while (diff) { errs += diff & 1; diff >>= 1; }
    }
    return errs;
}

// Push bytes into an HLS stream
static void push_bytes(hls::stream<ap_uint<8>>& s, const unsigned char* buf, int n) {
    for (int i = 0; i < n; i++) s.write((ap_uint<8>)buf[i]);
}

// Pull bytes from an HLS stream into a buffer
static void pull_bytes(hls::stream<ap_uint<8>>& s, unsigned char* buf, int n) {
    for (int i = 0; i < n; i++) buf[i] = (unsigned char)s.read().to_uint();
}

// Run encode then decode, return bit error count
// If noise_pct > 0, flip that percentage of coded bits before decoding
static int run_codec(rate_t rate, const unsigned char* data_in,
                     unsigned char* data_out, int n_data_bytes, int noise_pct)
{
    // Encoded byte count
    int n_data_bits  = n_data_bytes * 8;
    int n_coded_bits = (rate == 0) ? (n_data_bits * 2) : (n_data_bits / 2 * 3);
    int n_coded_bytes = (n_coded_bits + 7) / 8;

    // Encode
    static unsigned char coded_buf[TEST_BYTES * 2 + 4];
    {
        hls::stream<ap_uint<8>> din("enc_in"), dout("enc_out");
        push_bytes(din, data_in, n_data_bytes);
        conv_enc(din, dout, rate, n_data_bytes);
        pull_bytes(dout, coded_buf, n_coded_bytes);
    }

    // Optionally corrupt coded bits
    if (noise_pct > 0) {
        // Unpack, flip, repack
        static unsigned char tmp_bits[TEST_BYTES * 16];
        // Unpack
        for (int i = 0; i < n_coded_bits; i++) {
            tmp_bits[i] = (coded_buf[i/8] >> (7 - (i%8))) & 1;
        }
        // Flip
        unsigned int rng = SEED ^ 0xDEAD;
        int flips = (n_coded_bits * noise_pct) / 100;
        for (int f = 0; f < flips; f++) {
            rng = rng * 1664525u + 1013904223u;
            int idx = (rng >> 8) % n_coded_bits;
            tmp_bits[idx] ^= 1;
        }
        // Repack
        memset(coded_buf, 0, n_coded_bytes);
        for (int i = 0; i < n_coded_bits; i++) {
            if (tmp_bits[i]) coded_buf[i/8] |= (1 << (7 - (i%8)));
        }
    }

    // Decode
    {
        hls::stream<ap_uint<8>> cin("dec_in"), cout_("dec_out");
        push_bytes(cin, coded_buf, n_coded_bytes);
        viterbi_dec(cin, cout_, rate, n_data_bytes);
        pull_bytes(cout_, data_out, n_data_bytes);
    }

    return count_bit_errors(data_in, data_out, n_data_bytes);
}

// ── Main ──────────────────────────────────────────────────────

int main() {
    std::cout << "\n========================================\n";
    std::cout << "  FEC Codec Testbench — K=7 Conv Code\n";
    std::cout << "========================================\n\n";

    static unsigned char data_in[TEST_BYTES];
    static unsigned char data_out[TEST_BYTES];
    fill_random(data_in, TEST_BYTES, SEED);

    int total_pass = 0, total_fail = 0;

    // ── Test 1: Rate 1/2, clean ───────────────────────────────
    std::cout << "Test 1: Rate 1/2, clean (no noise)\n";
    {
        int errs = run_codec(0, data_in, data_out, TEST_BYTES, 0);
        int total_bits = TEST_BYTES * 8;
        if (errs == 0) {
            std::cout << "  [PASS]  BER = 0  (0/" << total_bits << " bit errors)\n";
            total_pass++;
        } else {
            std::cout << "  [FAIL]  " << errs << "/" << total_bits << " bit errors\n";
            total_fail++;
        }
    }

    // ── Test 2: Rate 2/3, clean ───────────────────────────────
    std::cout << "Test 2: Rate 2/3, clean (no noise)\n";
    {
        int errs = run_codec(1, data_in, data_out, TEST_BYTES, 0);
        int total_bits = TEST_BYTES * 8;
        if (errs == 0) {
            std::cout << "  [PASS]  BER = 0  (0/" << total_bits << " bit errors)\n";
            total_pass++;
        } else {
            std::cout << "  [FAIL]  " << errs << "/" << total_bits << " bit errors\n";
            total_fail++;
        }
    }

    // ── Test 3: Rate 1/2, noisy ───────────────────────────────
    std::cout << "Test 3: Rate 1/2, " << NOISE_RATE_PCT << "% coded bit flips\n";
    {
        int n_coded_bits = TEST_BYTES * 8 * 2;
        int flips = n_coded_bits * NOISE_RATE_PCT / 100;
        int errs = run_codec(0, data_in, data_out, TEST_BYTES, NOISE_RATE_PCT);
        int total_bits = TEST_BYTES * 8;
        double ber = (double)errs / total_bits;
        std::cout << "  [INFO]  " << flips << " coded bits flipped (~"
                  << NOISE_RATE_PCT << "% BER before FEC)\n";
        std::cout << "  [INFO]  " << errs << "/" << total_bits
                  << " data bit errors after Viterbi  BER=" << ber << "\n";
        // Rate 1/2 should correct 5% BER easily (coding gain ~5 dB)
        if (errs < total_bits / 10) {
            std::cout << "  [PASS]  Viterbi corrected most errors\n";
            total_pass++;
        } else {
            std::cout << "  [FAIL]  Too many residual errors\n";
            total_fail++;
        }
    }

    // ── Test 4: Rate 2/3, noisy ───────────────────────────────
    std::cout << "Test 4: Rate 2/3, " << NOISE_RATE_PCT << "% coded bit flips\n";
    {
        int n_data_bits  = TEST_BYTES * 8;
        int n_coded_bits = n_data_bits / 2 * 3;
        int flips = n_coded_bits * NOISE_RATE_PCT / 100;
        int errs = run_codec(1, data_in, data_out, TEST_BYTES, NOISE_RATE_PCT);
        int total_bits = n_data_bits;
        double ber = (double)errs / total_bits;
        std::cout << "  [INFO]  " << flips << " coded bits flipped (~"
                  << NOISE_RATE_PCT << "% BER before FEC)\n";
        std::cout << "  [INFO]  " << errs << "/" << total_bits
                  << " data bit errors after Viterbi  BER=" << ber << "\n";
        if (errs < total_bits / 10) {
            std::cout << "  [PASS]  Viterbi corrected most errors\n";
            total_pass++;
        } else {
            std::cout << "  [FAIL]  Too many residual errors\n";
            total_fail++;
        }
    }

    // ── Summary ───────────────────────────────────────────────
    std::cout << "\n========================================\n";
    std::cout << "  Tests passed: " << total_pass << " / " << (total_pass+total_fail) << "\n";
    if (total_fail == 0) {
        std::cout << "  RESULT: ALL PASS\n";
    } else {
        std::cout << "  RESULT: " << total_fail << " FAILED\n";
    }
    std::cout << "========================================\n\n";

    return (total_fail == 0) ? 0 : 1;
}
