# export_ip.tcl — Export all OFDM HLS blocks as Vivado IP
# Opens EXISTING synthesized projects — no re-synthesis.
# Run via: ./setup_vitis.sh export_ip
#
# Output: ip_repo/<block>/ — Vivado IP catalog packages

set ROOT [file normalize [file dirname [file dirname [info script]]]]
set IP_DIR "$ROOT/ip_repo"
file mkdir $IP_DIR

# {project_dir  top_function}
set BLOCKS {
    {ofdm_tx_proj    ofdm_tx}
    {ofdm_rx_proj    ofdm_rx}
    {sync_detect_proj  sync_detect}
    {cfo_correct_proj  cfo_correct}
    {scrambler_proj    scrambler}
    {interleaver_proj  interleaver}
    {conv_enc_proj     conv_enc}
    {viterbi_proj      viterbi_dec}
}

foreach item $BLOCKS {
    set proj  [lindex $item 0]
    set top   [lindex $item 1]
    set proj_path "$ROOT/$proj"

    if {![file exists $proj_path]} {
        puts "ERROR: project not found: $proj_path"
        continue
    }

    puts "=============================="
    puts " Exporting: $top  ($proj)"
    puts "=============================="

    open_project $proj_path
    open_solution sol1

    set out_dir "$IP_DIR/$top"
    file mkdir $out_dir

    export_design \
        -format    ip_catalog \
        -output    $out_dir  \
        -vendor    "hallycon.in" \
        -library   "ofdm" \
        -version   "1.0" \
        -display_name "Hallycon OFDM $top"

    close_project
    puts "  Done: $out_dir"
    puts ""
}

puts "============================================"
puts " All IPs exported to: $IP_DIR/"
puts "============================================"
