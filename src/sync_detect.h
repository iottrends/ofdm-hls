// ============================================================
// sync_detect.h  —  Free-running RX gatekeeper with inline CFO.
//
// This block is the single IP that used to be `sync_cfo` (= sync_detect
// + cfo_correct in DATAFLOW).  The merge has collapsed because CFO
// correction is now inline inside the gatekeeper FSM — one block,
// one clock, no internal FIFO.
//
// Interface protocol
// ──────────────────
// ap_ctrl_none  : no ap_start/ap_done handshake.  Body is while(1),
//                 block runs from the first cycle after reset.
// iq_in         : AXIS from adc_input_fifo, continuous, never back-pressured
// iq_out        : AXIS to ofdm_rx, ONLY written when the gate is open
// n_syms_fb     : ap_none + ap_vld companion wire from ofdm_rx.n_syms_fb
//                 (decoded n_syms after BPSK header).  Strobe is
//                 single-cycle; this block sticky-latches it.
// pow_threshold : s_axilite `stat` bundle — tunable power trigger level
//                 (absolute magnitude squared).  Default set by driver.
// stats_*       : read-only `stat` bundle CSRs for bring-up visibility.
//
// Thresholds and algorithm
// ────────────────────────
// Continuous sliding-window Schmidl-Cox CP correlation over the most
// recent 4096 samples.  Detection gate = (pow_env > POW_TH) AND
// (|P|² > SC_TH² · R · Rl).  On detection, transitions SEARCH →
// FWD_PREHDR (forwards preamble+header = 576 samples), then
// WAIT_NSYMS (stalls for ofdm_rx header decode via n_syms_fb wire),
// then FWD_DATA (forwards n_syms × 288 samples), then back to SEARCH
// with a 288-cycle deaf window to purge own-signal from accumulators.
// Metric computation is frozen during all three forward/wait states to
// prevent self-trigger on the CP of our own transmitted symbols.
//
// See docs/RX_GATING_DESIGN.md for full architecture rationale.
// ============================================================
#pragma once
#include "ofdm_rx.h"    // iq_t, sample_t, FFT_SIZE, CP_LEN

// Legacy constants preserved for downstream files that reference them
#define SYNC_NL      (FFT_SIZE + CP_LEN)    // 288 — one OFDM symbol
#define MAX_DATA_SYMS 255

// cfo_t kept as a type only for historical API compatibility; the
// gatekeeper now consumes the CFO estimate internally.
typedef ap_fixed<16,2> cfo_t;

void sync_detect(
    hls::stream<iq_t>&  iq_in,
    hls::stream<iq_t>&  iq_out,

    // Per-packet feedback from ofdm_rx.header decode (ap_none + _vld)
    ap_uint<8>          n_syms_fb,
    ap_uint<1>          n_syms_fb_vld,

    // Tunable power threshold (host-programmed during bring-up)
    ap_ufixed<24,8>     pow_threshold,

    // Bring-up readbacks
    ap_uint<32>&        stat_preamble_count,
    ap_uint<32>&        stat_header_bad_count,
    ap_ufixed<24,8>&    stat_pow_env
);
