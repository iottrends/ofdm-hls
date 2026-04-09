#!/bin/bash
# ============================================================
# setup_vitis.sh  —  Vitis HLS environment setup for WSL
#
# Usage:
#   source setup_vitis.sh          # set up env in current shell
#   ./setup_vitis.sh csim          # run C simulation
#   ./setup_vitis.sh synth         # run C synthesis
#   ./setup_vitis.sh all           # csim + synth + report
# ============================================================

XILINX_ROOT="$HOME/Xilinx/2025.2"
SCRIPT_DIR_SETUP="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# ── RDI vars (mirrors what vivado/vitis wrapper scripts derive) ──
# RDI_BINROOT = the tool's bin/ dir
# RDI_APPROOT = one level above bin/ (the tool root)
export RDI_BINROOT="$XILINX_ROOT/Vitis/bin"
export RDI_APPROOT="$XILINX_ROOT/Vitis"
export TCL_LIBRARY="$XILINX_ROOT/tps/tcl/tcl8.6"

# ── Tool paths ───────────────────────────────────────────────
export XILINX_VIVADO="$XILINX_ROOT/Vivado"
export XILINX_VITIS="$XILINX_ROOT/Vitis"
export XILINX_HLS="$XILINX_ROOT/Vitis"

# ── Binary PATH ───────────────────────────────────────────────
export PATH="$XILINX_ROOT/Vivado/bin:$XILINX_ROOT/Vitis/bin:$PATH"

# ── Shared libraries ─────────────────────────────────────────
# lib_compat/ has libncurses.so.5/libtinfo.so.5 → .so.6 symlinks.
# Ubuntu 22+ dropped these; Vitis still requires .so.5.
export LD_LIBRARY_PATH="\
$SCRIPT_DIR_SETUP/lib_compat:\
$XILINX_ROOT/Vitis/lib/lnx64.o:\
$XILINX_ROOT/Vivado/lib/lnx64.o:\
$LD_LIBRARY_PATH"

# ── Verify the binary loads ───────────────────────────────────
# Invoke via loader with -exec flag (required by loader)
VITIS_HLS_BIN="$XILINX_ROOT/Vitis/bin/loader"
VITIS_HLS_EXEC="-exec vitis_hls"

_check_env() {
    # vitis_hls prints version banner to stderr and exits non-zero for -help
    # so check for the version string instead of exit code
    local out
    out=$("$VITIS_HLS_BIN" $VITIS_HLS_EXEC -help 2>&1 || true)
    if echo "$out" | grep -q "Vitis HLS"; then
        echo "[setup] vitis_hls OK  ($VITIS_HLS_BIN)"
        return 0
    else
        echo "[setup] ERROR: vitis_hls failed to start"
        echo "$out" | head -5
        return 1
    fi
}

# ── TCL scripts for each flow ─────────────────────────────────
_write_csim_tcl() {
    local mod_arg="${1:-1}"   # 0=QPSK, 1=16QAM
    local proj_dir="$SCRIPT_DIR/ofdm_tx_proj"
    local bits_src="$SCRIPT_DIR/tb_input_to_tx.bin"
    local hls_out="$SCRIPT_DIR/tb_tx_output_hls.txt"
    local build_dir="${proj_dir}/sol1/csim/build"
    local ld_path="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"
    cat > /tmp/ofdm_csim.tcl << EOF
open_project ofdm_tx_proj
set_top ofdm_tx
add_files ofdm_tx.cpp
add_files ofdm_tx.h
add_files scrambler.cpp
add_files scrambler.h
add_files interleaver.cpp
add_files interleaver.h
add_files conv_enc.cpp
add_files viterbi_dec.cpp
add_files conv_fec.h
add_files -tb ofdm_tx_tb.cpp
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
# Delete old binary so we always get a fresh compile
if {[file exists ${build_dir}/csim.exe]} {
    file delete ${build_dir}/csim.exe
}
# Compile only
csim_design -setup
# Copy test vector into build dir
file copy -force ${bits_src} ${build_dir}/tb_input_to_tx.bin
# Run simulation directly
set ::env(LD_LIBRARY_PATH) "${ld_path}:\$::env(LD_LIBRARY_PATH)"
set ret [catch {exec sh -c "cd ${build_dir} && ./csim.exe --mod ${mod_arg}"} out]
puts \$out
if {\$ret != 0} { puts "@E csim.exe failed"; return -code error }
# Copy output back to project root
file copy -force ${build_dir}/tb_tx_output_hls.txt ${hls_out}
puts "@I Output copied to ${hls_out}"
close_project
exit
EOF
}

_write_rx_csim_tcl() {
    local proj_dir="$SCRIPT_DIR/ofdm_rx_proj"
    local tx_out="$SCRIPT_DIR/tb_tx_output_hls.txt"
    local in_bits="$SCRIPT_DIR/tb_input_to_tx.bin"
    local decoded_out="$SCRIPT_DIR/tb_rx_decoded_hls.bin"
    local build_dir="${proj_dir}/sol1/csim/build"
    local ld_path="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"
    cat > /tmp/ofdm_rx_csim.tcl << EOF
open_project ofdm_rx_proj
set_top ofdm_rx
add_files ofdm_rx.cpp
add_files ofdm_tx.h
add_files ofdm_rx.h
add_files sync_detect.cpp
add_files sync_detect.h
add_files cfo_correct.cpp
add_files cfo_correct.h
add_files scrambler.cpp
add_files scrambler.h
add_files interleaver.cpp
add_files interleaver.h
add_files conv_enc.cpp
add_files viterbi_dec.cpp
add_files conv_fec.h
add_files -tb ofdm_rx_tb.cpp
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
# Delete old binary so we always get a fresh compile
if {[file exists ${build_dir}/csim.exe]} {
    file delete ${build_dir}/csim.exe
}
# Compile only
csim_design -setup
# Copy test vectors into build dir
file copy -force ${tx_out}   ${build_dir}/tb_tx_output_hls.txt
file copy -force ${in_bits}  ${build_dir}/tb_input_to_tx.bin
# Run simulation directly
set ::env(LD_LIBRARY_PATH) "${ld_path}:\$::env(LD_LIBRARY_PATH)"
set ret [catch {exec sh -c "cd ${build_dir} && ./csim.exe"} out]
puts \$out
if {\$ret != 0} { puts "@E csim.exe failed"; return -code error }
# Copy output back to project root
if {[file exists ${build_dir}/tb_rx_decoded_hls.bin]} {
    file copy -force ${build_dir}/tb_rx_decoded_hls.bin ${decoded_out}
    puts "@I Decoded bytes copied to ${decoded_out}"
}
close_project
exit
EOF
}

_write_rx_noisy_csim_tcl() {
    local proj_dir="$SCRIPT_DIR/ofdm_rx_proj"
    local tx_out="$SCRIPT_DIR/tb_tx_output_hls_noise.txt"   # noisy signal from ofdm_channel_sim.py
    local in_bits="$SCRIPT_DIR/tb_input_to_tx.bin"
    local decoded_out="$SCRIPT_DIR/tb_decoded_noisy.bin"
    local build_dir="${proj_dir}/sol1/csim/build"
    local ld_path="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"
    cat > /tmp/ofdm_rx_noisy_csim.tcl << EOF
open_project ofdm_rx_proj
set_top ofdm_rx
add_files ofdm_rx.cpp
add_files ofdm_tx.h
add_files ofdm_rx.h
add_files sync_detect.cpp
add_files sync_detect.h
add_files cfo_correct.cpp
add_files cfo_correct.h
add_files scrambler.cpp
add_files scrambler.h
add_files interleaver.cpp
add_files interleaver.h
add_files conv_enc.cpp
add_files viterbi_dec.cpp
add_files conv_fec.h
add_files -tb ofdm_rx_tb.cpp
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
if {[file exists ${build_dir}/csim.exe]} {
    file delete ${build_dir}/csim.exe
}
csim_design -setup
file copy -force ${tx_out}   ${build_dir}/tb_tx_output_hls.txt
file copy -force ${in_bits}  ${build_dir}/tb_input_to_tx.bin
set ::env(LD_LIBRARY_PATH) "${ld_path}:\$::env(LD_LIBRARY_PATH)"
set ret [catch {exec sh -c "cd ${build_dir} && ./csim.exe"} out]
puts \$out
if {\$ret != 0} { puts "@E csim.exe failed"; return -code error }
if {[file exists ${build_dir}/tb_rx_decoded_hls.bin]} {
    file copy -force ${build_dir}/tb_rx_decoded_hls.bin ${decoded_out}
    puts "@I Decoded bytes copied to ${decoded_out}"
}
close_project
exit
EOF
}

_write_rx_noisy_build_tcl() {
    # Compile the RX noisy C-sim binary without running it.
    # The sweep script then runs the binary directly for each test point,
    # avoiding per-point Vitis HLS startup overhead (~30-60 s/compile saved).
    local proj_dir="$SCRIPT_DIR/ofdm_rx_proj"
    local build_dir="${proj_dir}/sol1/csim/build"
    local ld_path="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"
    cat > /tmp/ofdm_rx_noisy_build.tcl << EOF
open_project ofdm_rx_proj
set_top ofdm_rx
add_files ofdm_rx.cpp
add_files ofdm_tx.h
add_files ofdm_rx.h
add_files sync_detect.cpp
add_files sync_detect.h
add_files cfo_correct.cpp
add_files cfo_correct.h
add_files scrambler.cpp
add_files scrambler.h
add_files interleaver.cpp
add_files interleaver.h
add_files conv_enc.cpp
add_files viterbi_dec.cpp
add_files conv_fec.h
add_files -tb ofdm_rx_tb.cpp
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
csim_design -setup
puts "@I RX C-sim binary built at ${build_dir}/csim.exe"
close_project
exit
EOF
}

_write_fec_csim_tcl() {
    local proj_dir="$SCRIPT_DIR/conv_fec_proj"
    local build_dir="${proj_dir}/sol1/csim/build"
    local ld_path="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"
    cat > /tmp/fec_csim.tcl << EOF
open_project conv_fec_proj
set_top conv_enc
add_files conv_enc.cpp
add_files viterbi_dec.cpp
add_files conv_fec.h
add_files -tb conv_fec_tb.cpp
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
if {[file exists ${build_dir}/csim.exe]} {
    file delete ${build_dir}/csim.exe
}
csim_design -setup
set ::env(LD_LIBRARY_PATH) "${ld_path}:\$::env(LD_LIBRARY_PATH)"
set ret [catch {exec sh -c "cd ${build_dir} && ./csim.exe"} out]
puts \$out
if {\$ret != 0} { puts "@E csim.exe failed"; return -code error }
close_project
exit
EOF
}

_write_synth_tcl() {
    cat > /tmp/ofdm_synth.tcl << 'EOF'
open_project ofdm_tx_proj
set_top ofdm_tx
add_files ofdm_tx.cpp
add_files ofdm_tx.h
open_solution sol1
set_part xc7a50tcsg325-1
create_clock -period 10
csynth_design
close_project
exit
EOF
}

_write_rx_synth_tcl() {
    cat > /tmp/ofdm_rx_synth.tcl << 'EOF'
open_project ofdm_rx_proj
set_top ofdm_rx
add_files ofdm_rx.cpp
add_files ofdm_rx.h
add_files ofdm_tx.h
open_solution sol1
set_part xc7a50tcsg325-1
create_clock -period 10
csynth_design
puts "\n=== RX Synthesis Report ==="
set rpt [open ofdm_rx_proj/sol1/syn/report/ofdm_rx_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
EOF
}

_write_conv_enc_synth_tcl() {
    cat > /tmp/conv_enc_synth.tcl << 'EOF'
open_project conv_enc_proj
set_top conv_enc
add_files conv_enc.cpp
add_files conv_fec.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
csynth_design
puts "\n=== conv_enc Synthesis Report ==="
set rpt [open conv_enc_proj/sol1/syn/report/conv_enc_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
EOF
}

_write_viterbi_synth_tcl() {
    cat > /tmp/viterbi_synth.tcl << 'EOF'
open_project viterbi_proj
set_top viterbi_dec
add_files viterbi_dec.cpp
add_files conv_fec.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
csynth_design
puts "\n=== viterbi_dec Synthesis Report ==="
set rpt [open viterbi_proj/sol1/syn/report/viterbi_dec_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
EOF
}

_write_sync_detect_synth_tcl() {
    cat > /tmp/sync_detect_synth.tcl << 'EOF'
open_project sync_detect_proj
set_top sync_detect
add_files sync_detect.cpp
add_files sync_detect.h
add_files ofdm_rx.h
add_files ofdm_tx.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
csynth_design
puts "\n=== sync_detect Synthesis Report ==="
set rpt [open sync_detect_proj/sol1/syn/report/sync_detect_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
EOF
}

_write_cfo_correct_synth_tcl() {
    cat > /tmp/cfo_correct_synth.tcl << 'EOF'
open_project cfo_correct_proj
set_top cfo_correct
add_files cfo_correct.cpp
add_files cfo_correct.h
add_files sync_detect.h
add_files ofdm_rx.h
add_files ofdm_tx.h
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
csynth_design
puts "\n=== cfo_correct Synthesis Report ==="
set rpt [open cfo_correct_proj/sol1/syn/report/cfo_correct_csynth.rpt r]
puts [read $rpt]
close $rpt
close_project
exit
EOF
}

_write_all_tcl() {
    cat > /tmp/ofdm_all.tcl << 'EOF'
open_project ofdm_tx_proj
set_top ofdm_tx
add_files ofdm_tx.cpp
add_files ofdm_tx.h
add_files -tb ofdm_tx_tb.cpp
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
# C simulation
csim_design
# C synthesis
csynth_design
# Export report
puts "=== Synthesis Report ==="
puts [read [open ofdm_tx_proj/sol1/syn/report/ofdm_tx_csynth.rpt]]
close_project
exit
EOF
}

# ── Main: only runs when script is executed (not sourced) ─────
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

    _check_env || exit 1

    case "${1:-help}" in
        csim)
            echo "[run] Running TX C simulation (mod=${2:-1})..."
            _write_csim_tcl "${2:-1}"
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_csim.tcl 2>&1 | tee vitis_csim.log
            echo "[run] Log saved to vitis_csim.log"
            ;;
        rx_csim)
            echo "[run] Running RX C simulation..."
            _write_rx_csim_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_rx_csim.tcl 2>&1 | tee vitis_rx_csim.log
            echo "[run] Log saved to vitis_rx_csim.log"
            ;;
        rx_noisy_csim)
            echo "[run] Running RX C simulation on noisy signal (tb_tx_output_hls_noise.txt)..."
            _write_rx_noisy_csim_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_rx_noisy_csim.tcl 2>&1 | tee vitis_rx_noisy_csim.log
            echo "[run] Log saved to vitis_rx_noisy_csim.log"
            ;;
        rx_noisy_build)
            echo "[run] Building RX C-sim binary (compile only, no run)..."
            _write_rx_noisy_build_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_rx_noisy_build.tcl 2>&1 | tee vitis_rx_noisy_build.log
            echo "[run] Binary ready. Log saved to vitis_rx_noisy_build.log"
            ;;
        fec_csim)
            echo "[run] Running FEC codec C simulation..."
            _write_fec_csim_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/fec_csim.tcl 2>&1 | tee vitis_fec_csim.log
            echo "[run] Log saved to vitis_fec_csim.log"
            ;;
        rx_synth)
            echo "[run] Running RX C synthesis..."
            _write_rx_synth_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_rx_synth.tcl 2>&1 | tee vitis_rx_synth.log
            echo "[run] Log saved to vitis_rx_synth.log"
            ;;
        conv_enc_synth)
            echo "[run] Running conv_enc C synthesis..."
            _write_conv_enc_synth_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/conv_enc_synth.tcl 2>&1 | tee vitis_conv_enc_synth.log
            echo "[run] Log saved to vitis_conv_enc_synth.log"
            ;;
        viterbi_synth)
            echo "[run] Running viterbi_dec C synthesis..."
            _write_viterbi_synth_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/viterbi_synth.tcl 2>&1 | tee vitis_viterbi_synth.log
            echo "[run] Log saved to vitis_viterbi_synth.log"
            ;;
        sync_detect_synth)
            echo "[run] Running sync_detect C synthesis..."
            _write_sync_detect_synth_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/sync_detect_synth.tcl 2>&1 | tee vitis_sync_detect_synth.log
            echo "[run] Log saved to vitis_sync_detect_synth.log"
            ;;
        cfo_correct_synth)
            echo "[run] Running cfo_correct C synthesis..."
            _write_cfo_correct_synth_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/cfo_correct_synth.tcl 2>&1 | tee vitis_cfo_correct_synth.log
            echo "[run] Log saved to vitis_cfo_correct_synth.log"
            ;;
        synth)
            echo "[run] Running C synthesis..."
            _write_synth_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_synth.tcl 2>&1 | tee vitis_synth.log
            echo "[run] Log saved to vitis_synth.log"
            ;;
        all)
            echo "[run] Running csim + synthesis..."
            _write_all_tcl
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_all.tcl 2>&1 | tee vitis_all.log
            echo "[run] Log saved to vitis_all.log"
            ;;
        check)
            _check_env
            ;;
        help|*)
            echo "Usage:"
            echo "  source setup_vitis.sh          # set env in current shell"
            echo "  ./setup_vitis.sh check         # verify vitis_hls launches"
            echo "  ./setup_vitis.sh csim          # TX C simulation"
            echo "  ./setup_vitis.sh rx_csim          # RX C simulation (requires tx csim output)
  ./setup_vitis.sh rx_noisy_csim    # RX C simulation on noisy signal (requires ofdm_channel_sim.py output)"
            echo "  ./setup_vitis.sh rx_synth          # RX (ofdm_rx) C synthesis"
            echo "  ./setup_vitis.sh conv_enc_synth    # conv_enc FEC encoder C synthesis"
            echo "  ./setup_vitis.sh viterbi_synth     # viterbi_dec FEC decoder C synthesis"
            echo "  ./setup_vitis.sh sync_detect_synth # sync_detect timing+CFO C synthesis"
            echo "  ./setup_vitis.sh cfo_correct_synth # cfo_correct phase rotation C synthesis"
            echo "  ./setup_vitis.sh synth             # TX C synthesis"
            echo "  ./setup_vitis.sh all               # TX csim + synth + report"
            ;;
    esac
else
    # Being sourced — just report status
    _check_env
fi
