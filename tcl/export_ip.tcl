# export_ip.tcl — Export the 5 merged OFDM HLS blocks as Vivado IP
# Opens EXISTING synthesized projects — no re-synthesis.
# Run via: ./setup_vitis.sh export_ip
#
# Output: ip_repo/<top>/ — Vivado IP catalog packages
#
# Post-merge topology (5 IPs total):
#   TX:  tx_chain  →  ofdm_tx
#   RX:  sync_cfo  →  ofdm_rx  →  fec_rx   (fec_rx on 200 MHz clock)

set ROOT [file normalize [file dirname [file dirname [info script]]]]
set IP_DIR "$ROOT/ip_repo"
file mkdir $IP_DIR

# {project_dir  top_function}
set BLOCKS {
    {tx_chain_proj     tx_chain}
    {ofdm_tx_proj      ofdm_tx}
    {sync_detect_proj  sync_detect}
    {ofdm_rx_proj      ofdm_rx}
    {fec_rx_proj       fec_rx}
    {ofdm_mac_proj     ofdm_mac}
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
puts " All 5 IPs exported to: $IP_DIR/"
puts "============================================"
