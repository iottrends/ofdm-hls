// ============================================================
// sync_cfo.h  —  DEPRECATED.  Kept for include-path compatibility.
//
// `sync_cfo` has been retired.  The combined preamble-gate + CFO block
// is now `sync_detect` (in src/sync_detect.{h,cpp}), which is the sole
// HLS top instantiated in the BD for the RX front end.
//
// Any code that previously `#include "sync_cfo.h"` should be migrated
// to `#include "sync_detect.h"`.  This stub simply re-exports that
// header so old includes keep compiling during the transition.
// ============================================================
#pragma once
#include "sync_detect.h"
