#!/usr/bin/env tclsh
# ============================================================
# export_all_ip.tcl — Export all OFDM HLS blocks as Vivado IP
#
# Usage:
#   source /path/to/Vitis_HLS/2025.2/settings64.sh
#   cd ofdm-hls
#   vitis_hls -f export_all_ip.tcl
#
# Output: ip_repo/ directory with all IP cores ready for
#         Vivado IP Integrator or LiteX add_sources().
#
# Target: xc7a50tcsg325-2 @ 100 MHz (10 ns clock)
# ============================================================

set PART      "xc7a50tcsg325-2"
set CLK_NS    10
set IP_REPO   "./ip_repo"

file mkdir $IP_REPO

# ── Helper: create project, synth, export ──────────────────
proc export_hls_ip {name top_fn src_files ip_repo part clk_ns} {
    puts "=========================================="
    puts " Exporting: $name (top=$top_fn)"
    puts "=========================================="

    open_project ${name}_proj -reset
    set_top $top_fn

    foreach f $src_files {
        add_files $f
    }

    open_solution "solution1" -flow_target vivado -reset
    set_part $part
    create_clock -period $clk_ns -name default

    # C synthesis
    csynth_design

    # Export as Vivado IP catalog entry
    export_design \
        -format ip_catalog \
        -output "${ip_repo}/${name}" \
        -vendor "hallycon.in" \
        -library "ofdm" \
        -version "1.0" \
        -display_name "Hallycon OFDM ${name}"

    close_project
    puts "  → Exported to ${ip_repo}/${name}"
    puts ""
}

# ── Export each block ──────────────────────────────────────

# 1. OFDM TX (IFFT + modulation + CP insertion)
export_hls_ip "ofdm_tx" "ofdm_tx" \
    {ofdm_tx.cpp ofdm_tx.h} \
    $IP_REPO $PART $CLK_NS

# 2. OFDM RX (FFT + channel est + equalize + demap)
export_hls_ip "ofdm_rx" "ofdm_rx" \
    {ofdm_rx.cpp ofdm_rx.h ofdm_tx.h} \
    $IP_REPO $PART $CLK_NS

# 3. Timing sync + CFO estimator
export_hls_ip "sync_detect" "sync_detect" \
    {sync_detect.cpp sync_detect.h ofdm_rx.h ofdm_tx.h} \
    $IP_REPO $PART $CLK_NS

# 4. CFO phase correction
export_hls_ip "cfo_correct" "cfo_correct" \
    {cfo_correct.cpp cfo_correct.h sync_detect.h ofdm_rx.h ofdm_tx.h} \
    $IP_REPO $PART $CLK_NS

# 5. Scrambler / descrambler
export_hls_ip "scrambler" "scrambler" \
    {scrambler.cpp scrambler.h} \
    $IP_REPO $PART $CLK_NS

# 6. Bit interleaver / deinterleaver
export_hls_ip "interleaver" "interleaver" \
    {interleaver.cpp interleaver.h ofdm_tx.h} \
    $IP_REPO $PART $CLK_NS

# 7. Convolutional encoder
export_hls_ip "conv_enc" "conv_enc" \
    {conv_enc.cpp conv_fec.h} \
    $IP_REPO $PART $CLK_NS

# 8. Viterbi decoder
export_hls_ip "viterbi_dec" "viterbi_dec" \
    {viterbi_dec.cpp conv_fec.h} \
    $IP_REPO $PART $CLK_NS

puts "============================================"
puts " All IP exported to: $IP_REPO/"
puts "============================================"
puts ""
puts " Next steps:"
puts "   1. In Vivado: Settings → IP → Repository → Add $IP_REPO"
puts "   2. Or in LiteX: platform.add_source_dir('$IP_REPO')"
puts "   3. Run synthesis to verify resource utilization per block"
puts ""
