# ============================================================
# synth_sync_cfo.tcl — DEPRECATED.
#
# sync_cfo has been retired.  The RX front-end IP is now `sync_detect`
# (see tcl/synth_sync_detect.tcl).  This file is kept only so stale
# invocations fail loudly instead of silently rebuilding an orphaned IP.
# ============================================================
puts "ERROR: sync_cfo is retired.  Use synth_sync_detect.tcl instead."
exit 1
