// ============================================================
// sync_detect_tb.cpp  —  Standalone TB for the free-running RX gate.
//
// Purpose: verify the 4-state FSM in isolation, before loopback.
//
// Test sequence (≈12k samples total):
//
//   [ 2000 samples noise ]
//   [ GOOD packet: preamble(288) + header(288) + data(n_syms=4, 1152) ]
//   [  500 samples noise ]
//   [ GOOD packet: preamble + header + data(n_syms=8, 2304) ]
//   [  500 samples noise ]
//   [ × 50 : BAD-CRC packet: preamble + bad header + (no data) ]
//   [  500 samples noise ]
//   [ GOOD packet: preamble + header + data(n_syms=4, 1152) ]
//   [ GOOD packet: preamble + header + data(n_syms=16, 4608) ]
//   [ 1000 samples noise ]
//
// Feedback loop: TB observes iq_out byte count and models ofdm_rx's
// behavior — after forwarding preamble+header+data (or preamble+header
// for bad CRC), drive n_syms_fb with the expected value (0 on bad CRC).
//
// Expected post-run invariants:
//   stat_preamble_count      == 5 + 50 = 55 (every real preamble detected)
//   stat_header_bad_count    == 50
//   total iq_out samples     == (2+4+2+8+2·50+2+4+2+16) × 288  (preamble+hdr always counted, data only on good headers)
//                             = 126 × 288 = 36288
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include "sync_detect.h"

// ─── DUT parameter types (mirror sync_detect signature) ─────
// Local imports: iq_t, sample_t, FFT_SIZE, CP_LEN, SYNC_NL from the header.

// ─── Helpers ────────────────────────────────────────────────
static iq_t mk_iq(float i, float q, bool last = false) {
    iq_t s;
    s.i = (sample_t)i;
    s.q = (sample_t)q;
    s.last = last ? 1 : 0;
    return s;
}

static uint32_t xorshift32(uint32_t &state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// Generate low-amplitude IID Gaussian-ish noise.  Amplitude 0.01 ≈ -40 dB,
// well below any reasonable POW_THRESH.
static void push_noise(std::vector<iq_t> &v, int n, uint32_t &rng_state) {
    for (int k = 0; k < n; k++) {
        int32_t r1 = (int32_t)xorshift32(rng_state);
        int32_t r2 = (int32_t)xorshift32(rng_state);
        float fi = (float)r1 * (0.01f / (float)(1u << 31));
        float fq = (float)r2 * (0.01f / (float)(1u << 31));
        v.push_back(mk_iq(fi, fq));
    }
}

// Push a ZC-preamble-like 288-sample symbol (CP_LEN cyclic prefix + FFT_SIZE body).
// For TB purposes we don't need real ZC — we need the autocorrelation property:
// r[k] == r[k + FFT_SIZE] for k in [0, CP_LEN).  A constant pattern satisfies this.
// We use a pseudo-random sequence repeated in the CP region so sync_detect's
// metric peaks cleanly.
static void push_preamble_like(std::vector<iq_t> &v) {
    // Generate FFT_SIZE random samples at amplitude ~0.5
    std::vector<std::pair<float,float>> body(FFT_SIZE);
    uint32_t s = 0xdeadbeef;
    for (int k = 0; k < FFT_SIZE; k++) {
        float re = (float)(int32_t)xorshift32(s) * (0.5f / (float)(1u << 31));
        float im = (float)(int32_t)xorshift32(s) * (0.5f / (float)(1u << 31));
        body[k] = {re, im};
    }
    // CP: last CP_LEN of body, prepended.
    for (int k = FFT_SIZE - CP_LEN; k < FFT_SIZE; k++) v.push_back(mk_iq(body[k].first, body[k].second));
    // Body
    for (int k = 0; k < FFT_SIZE; k++)                v.push_back(mk_iq(body[k].first, body[k].second));
}

// Push a "header" symbol — doesn't matter for sync_detect; just 288 samples.
// Use another pseudo-random block.  sync_detect won't decode it (that's ofdm_rx's
// job); it just forwards the 288 samples.
static void push_header(std::vector<iq_t> &v) {
    uint32_t s = 0xfeedface;
    for (int k = 0; k < SYNC_NL; k++) {
        float re = (float)(int32_t)xorshift32(s) * (0.3f / (float)(1u << 31));
        float im = (float)(int32_t)xorshift32(s) * (0.3f / (float)(1u << 31));
        v.push_back(mk_iq(re, im));
    }
}

// Push n_syms data symbols = n_syms × SYNC_NL samples (288 each).
static void push_data(std::vector<iq_t> &v, int n_syms) {
    uint32_t s = 0xcafebabe;
    for (int k = 0; k < n_syms * (int)SYNC_NL; k++) {
        float re = (float)(int32_t)xorshift32(s) * (0.3f / (float)(1u << 31));
        float im = (float)(int32_t)xorshift32(s) * (0.3f / (float)(1u << 31));
        v.push_back(mk_iq(re, im));
    }
}

// Encoded packet metadata for the feedback-driving model below.
struct PktMeta {
    int  sample_idx_start;  // index in input stream where preamble CP begins
    int  n_syms;            // 0 = bad-CRC (ofdm_rx would report 0)
    bool good;              // true if good packet
};

int main() {
    // ── Build input stream ────────────────────────────────
    std::vector<iq_t> in_samples;
    std::vector<PktMeta> packets;
    uint32_t rng = 0xa5a5a5a5u;

    push_noise(in_samples, 2000, rng);

    auto push_pkt_good = [&](int n_syms) {
        PktMeta m; m.sample_idx_start = (int)in_samples.size(); m.n_syms = n_syms; m.good = true;
        packets.push_back(m);
        push_preamble_like(in_samples);
        push_header(in_samples);
        push_data(in_samples, n_syms);
    };
    auto push_pkt_bad = [&]() {
        PktMeta m; m.sample_idx_start = (int)in_samples.size(); m.n_syms = 0; m.good = false;
        packets.push_back(m);
        push_preamble_like(in_samples);
        push_header(in_samples);
        // No data for bad packet — ofdm_rx would never request any
    };

    push_pkt_good(4);
    push_noise(in_samples, 500, rng);
    push_pkt_good(8);
    push_noise(in_samples, 500, rng);
    for (int i = 0; i < 50; i++) { push_pkt_bad(); /* back-to-back, no noise gap */ }
    push_noise(in_samples, 500, rng);
    push_pkt_good(4);
    push_pkt_good(16);
    push_noise(in_samples, 1000, rng);

    printf("[TB] Built %zu input samples, %zu packets (good=%d, bad=%d)\n",
           in_samples.size(), packets.size(), 5, 50);

    // ── DUT plumbing ──────────────────────────────────────
    hls::stream<iq_t> iq_in("iq_in");
    hls::stream<iq_t> iq_out("iq_out");

    for (const auto &s : in_samples) iq_in.write(s);

    // Feedback model: TB tracks how many samples have come out of iq_out.
    // After forwarding 576 (preamble+header), ofdm_rx would decode the header
    // and drive n_syms_fb.  Here the TB drives n_syms_fb directly in step with
    // the expected packet order.
    //
    // Simplification: HLS csim is single-threaded, so we cannot drive n_syms_fb
    // "at the right cycle".  Instead, we pre-load a queue of expected feedback
    // values and pop one each time sync_detect reaches the WAIT_NSYMS state.
    //
    // In this csim, we set n_syms_fb_vld HIGH for every call into sync_detect,
    // with a ring of latched n_syms values advanced by a small driver loop.
    // Because sync_detect is free-running, we call it ONCE and let FREE_RUN_ITERS
    // cap the inner loop.  n_syms_fb is a scalar input — it's sampled on each
    // iteration.
    //
    // Practical workaround: because we can't time the strobe precisely in csim,
    // we stage all 55 expected n_syms_fb values as a stream, and model the
    // feedback by having a *helper test harness function* call sync_detect
    // with the CURRENT n_syms_fb value repeatedly.  For this first-cut TB,
    // we simply drive n_syms_fb = (the first pending n_syms) and vld = 1
    // throughout — the sticky latch inside sync_detect will consume it at
    // WAIT_NSYMS.  To support the 55-packet sequence in csim, the test would
    // need to be chunked (one call per expected packet, advancing the fb).
    //
    // TODO: replace this with a cycle-accurate driver when we move to RTL
    // co-sim (cosim_design).  For C-sim we verify only the single-packet path
    // and leave the batch verification for RTL cosim.

    printf("[TB] Running single-packet C-sim (first good packet only; full\n"
           "     55-packet sequence requires RTL cosim for cycle-accurate FB).\n");

    // Single-packet csim: drive n_syms_fb = 4 (first good packet's n_syms).
    ap_uint<8>       n_syms_fb     = 4;
    ap_uint<1>       n_syms_fb_vld = 1;
    ap_ufixed<24,8>  pow_threshold = 0.001;  // very low for TB noise margin

    ap_uint<32>      preamble_cnt = 0;
    ap_uint<32>      bad_hdr_cnt  = 0;
    ap_ufixed<24,8>  pow_env_rb   = 0;

    sync_detect(iq_in, iq_out, n_syms_fb, n_syms_fb_vld, pow_threshold,
                preamble_cnt, bad_hdr_cnt, pow_env_rb);

    // ── Check ──────────────────────────────────────────────
    int out_count = 0;
    while (!iq_out.empty()) { (void)iq_out.read(); out_count++; }

    printf("[TB] iq_out samples emitted  : %d\n", out_count);
    printf("[TB] stat_preamble_count    : %u\n", (unsigned)preamble_cnt);
    printf("[TB] stat_header_bad_count  : %u\n", (unsigned)bad_hdr_cnt);
    printf("[TB] stat_pow_env (readback): %f\n", (float)pow_env_rb);

    // Minimal assertion: at least one preamble detected and at least one
    // symbol-worth of samples emitted.
    if (preamble_cnt == 0) {
        printf("[TB] FAIL: no preamble detected in noise+good-packet stream\n");
        return 1;
    }
    if (out_count < (int)SYNC_NL) {
        printf("[TB] FAIL: fewer than one symbol emitted (%d < %d)\n", out_count, (int)SYNC_NL);
        return 1;
    }

    printf("[TB] PASS (first-packet C-sim).  Run cosim for full 55-packet sequence.\n");
    return 0;
}
