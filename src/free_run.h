// ============================================================
// free_run.h  —  Shared constants for free-running HLS blocks.
//
// Any block whose top function is a while(1) FSM (sync_detect,
// ofdm_rx, fec_rx) should include this header and:
//   - gate its infinite loop via FREE_RUN_WHILE() so csim terminates
//   - put stats-only s_axilite registers on the `stat` bundle
// ============================================================
#pragma once

// Number of loop iterations to run in csim before exiting.
// Large enough for a handful of packets of the longest modcod.
#define FREE_RUN_ITERS  (200000)

// Macro wrapping the loop so csim is bounded but synthesis stays
// infinite.  Usage:
//   FREE_RUN_LOOP_BEGIN
//       ... FSM body ...
//   FREE_RUN_LOOP_END
#ifdef __SYNTHESIS__
  #define FREE_RUN_LOOP_BEGIN  FREE_RUN: while (1) {
  #define FREE_RUN_LOOP_END    }
#else
  #define FREE_RUN_LOOP_BEGIN  \
      FREE_RUN: for (int __csim_iter = 0; __csim_iter < FREE_RUN_ITERS; __csim_iter++) {
  #define FREE_RUN_LOOP_END    }
#endif

// Stats bundle name — hosts read-only counters / readbacks that do
// not participate in any per-packet CSR write sequence.  Using a
// separate bundle from per-packet `ctrl` keeps the BD smartconnect
// map clean.
#define S_AXILITE_STAT_BUNDLE  "stat"
