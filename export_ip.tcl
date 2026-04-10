# export_ip.tcl — Export all OFDM HLS blocks as Vivado IP
# Uses EXISTING synthesized projects (preserves all pragmas/optimizations).
# Run from: /mnt/d/work/HW/ofdm_tx_hls/
#   vitis_hls -f export_ip.tcl

set ROOT   [pwd]
set IP_DIR "$ROOT/ip_repo"
file mkdir $IP_DIR

set BLOCKS {
    ofdm_tx_proj
    ofdm_rx_proj
    sync_detect_proj
    cfo_correct_proj
    scrambler_proj
    interleaver_proj
    conv_enc_proj
    viterbi_proj
}

foreach proj $BLOCKS {
    puts "=============================="
    puts " Exporting: $proj"
    puts "=============================="

    open_project $proj
    open_solution sol1

    export_design \
        -format ip_catalog \
        -output "$IP_DIR/[string map {_proj ""} $proj]" \
        -vendor  "hallycon.in" \
        -library "ofdm" \
        -version "1.0"

    close_project
    puts "  → Done: $IP_DIR/[string map {_proj \"\" } $proj]"
    puts ""
}

puts "============================================"
puts " All IPs exported to: $IP_DIR/"
puts " Next: Vivado project + xfft swap"
puts "============================================"
