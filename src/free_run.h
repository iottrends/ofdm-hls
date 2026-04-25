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

// ── csim drain-read helper ──────────────────────────────────
// Free-running blocks (ap_ctrl_none + while(1)) cannot exit naturally
// in csim because the input stream eventually starves but the loop
// keeps polling.  This macro wraps a stream read with a bounded
// retry-and-yield loop in csim; on real synthesis it collapses to a
// blocking `var = stream.read()`.  When the stream stays empty for
// 100k retries it executes `on_drain`.
//
// IMPORTANT: `on_drain` runs inside a `do { } while (0)` wrapper, so
// `break` is swallowed by the wrapper instead of escaping the caller's
// FREE_RUN loop.  Use `return` (or `return <value>`) from a free-running
// top function, or `return false` from a sub-function called by the
// FREE_RUN body — both unwind cleanly.  Do NOT pass `break`.
//
// Usage:
//   DRAIN_READ_OR(iq_in, sample, return false);   // sub-function
//   DRAIN_READ_OR(iq_in, in_s,   return);         // void top function
#ifdef __SYNTHESIS__
  #define DRAIN_READ_OR(stream, var, on_drain)  do { var = (stream).read(); } while (0)
  // Synthesis: peek-only block has no equivalent — the DATAFLOW region below
  // pulls data_in itself.  Empty body keeps the FREE_RUN loop unrolled cleanly.
  #define WAIT_NONEMPTY_OR(stream, on_drain)    do { } while (0)
#else
  #include <thread>
  #define DRAIN_READ_OR(stream, var, on_drain) do {                        \
      bool __drained = true;                                               \
      for (int __r = 0; __r < 100000; ++__r) {                             \
          if ((stream).read_nb(var)) { __drained = false; break; }         \
          std::this_thread::yield();                                       \
      }                                                                    \
      if (__drained) { on_drain; }                                         \
  } while (0)
  // Wait for the stream to become non-empty (without consuming) — used by
  // blocks whose body reads `stream` later inside a DATAFLOW region.
  #define WAIT_NONEMPTY_OR(stream, on_drain) do {                          \
      bool __drained = true;                                               \
      for (int __r = 0; __r < 100000; ++__r) {                             \
          if (!(stream).empty()) { __drained = false; break; }             \
          std::this_thread::yield();                                       \
      }                                                                    \
      if (__drained) { on_drain; }                                         \
  } while (0)
#endif
