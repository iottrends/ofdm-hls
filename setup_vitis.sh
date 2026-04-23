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

XILINX_ROOT="/mnt/d/work/vivado/2025.2"
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
    local mod_arg="${1:-1}"    # 0=QPSK, 1=16QAM
    local rate_arg="${2:-0}"   # 0=rate-1/2, 1=rate-2/3
    local proj_dir="$SCRIPT_DIR/ofdm_tx_proj"
    local bits_src="$SCRIPT_DIR/tb_input_to_tx.bin"
    local hls_out="$SCRIPT_DIR/tb_tx_output_hls.txt"
    local build_dir="${proj_dir}/sol1/csim/build"
    local ld_path="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"
    cat > /tmp/ofdm_csim.tcl << EOF
open_project -reset ofdm_tx_proj
set_top ofdm_tx
add_files src/ofdm_tx.cpp
add_files src/ofdm_tx.h
add_files src/scrambler.cpp
add_files src/scrambler.h
add_files src/interleaver.cpp
add_files src/interleaver.h
add_files src/conv_enc.cpp
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
add_files -tb tb/ofdm_tx_tb.cpp -cflags "-I./src"
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
set ret [catch {exec sh -c "cd ${build_dir} && ./csim.exe --mod ${mod_arg} --rate ${rate_arg}"} out]
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
    local mod_arg="${1:-1}"    # 0=QPSK, 1=16QAM
    local rate_arg="${2:-0}"   # 0=rate-1/2, 1=rate-2/3
    local proj_dir="$SCRIPT_DIR/ofdm_rx_proj"
    local tx_out="$SCRIPT_DIR/tb_tx_output_hls.txt"
    local in_bits="$SCRIPT_DIR/tb_input_to_tx.bin"
    local decoded_out="$SCRIPT_DIR/tb_rx_decoded_hls.bin"
    local build_dir="${proj_dir}/sol1/csim/build"
    local ld_path="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"
    cat > /tmp/ofdm_rx_csim.tcl << EOF
open_project -reset ofdm_rx_proj
set_top ofdm_rx
add_files src/ofdm_rx.cpp
add_files src/ofdm_tx.h
add_files src/ofdm_rx.h
add_files src/sync_detect.cpp
add_files src/sync_detect.h
add_files src/cfo_correct.cpp
add_files src/cfo_correct.h
add_files src/scrambler.cpp
add_files src/scrambler.h
add_files src/interleaver.cpp
add_files src/interleaver.h
add_files src/conv_enc.cpp
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
add_files -tb tb/ofdm_rx_tb.cpp -cflags "-I./src"
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
set ret [catch {exec sh -c "cd ${build_dir} && ./csim.exe --mod ${mod_arg} --rate ${rate_arg}"} out]
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
    local mod_arg="${1:-1}"    # 0=QPSK, 1=16QAM
    local rate_arg="${2:-0}"   # 0=rate-1/2, 1=rate-2/3
    local proj_dir="$SCRIPT_DIR/ofdm_rx_proj"
    local tx_out="$SCRIPT_DIR/tb_tx_output_hls_noise.txt"   # noisy signal from ofdm_channel_sim.py
    local in_bits="$SCRIPT_DIR/tb_input_to_tx.bin"
    local decoded_out="$SCRIPT_DIR/tb_decoded_noisy.bin"
    local build_dir="${proj_dir}/sol1/csim/build"
    local ld_path="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"
    cat > /tmp/ofdm_rx_noisy_csim.tcl << EOF
open_project -reset ofdm_rx_proj
set_top ofdm_rx
add_files src/ofdm_rx.cpp
add_files src/ofdm_tx.h
add_files src/ofdm_rx.h
add_files src/sync_detect.cpp
add_files src/sync_detect.h
add_files src/cfo_correct.cpp
add_files src/cfo_correct.h
add_files src/scrambler.cpp
add_files src/scrambler.h
add_files src/interleaver.cpp
add_files src/interleaver.h
add_files src/conv_enc.cpp
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
add_files -tb tb/ofdm_rx_tb.cpp -cflags "-I./src"
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
set ret [catch {exec sh -c "cd ${build_dir} && ./csim.exe --mod ${mod_arg} --rate ${rate_arg}"} out]
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
open_project -reset ofdm_rx_proj
set_top ofdm_rx
add_files src/ofdm_rx.cpp
add_files src/ofdm_tx.h
add_files src/ofdm_rx.h
add_files src/sync_detect.cpp
add_files src/sync_detect.h
add_files src/cfo_correct.cpp
add_files src/cfo_correct.h
add_files src/scrambler.cpp
add_files src/scrambler.h
add_files src/interleaver.cpp
add_files src/interleaver.h
add_files src/conv_enc.cpp
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
add_files -tb tb/ofdm_rx_tb.cpp -cflags "-I./src"
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
open_project -reset conv_fec_proj
set_top conv_enc
add_files src/conv_enc.cpp
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
add_files -tb tb/conv_fec_tb.cpp -cflags "-I./src"
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

# Synth TCL scripts live as static files in tcl/ — no generation needed.

_write_tx_cosim_tcl() {
    local mod_arg="${1:-1}"
    local proj_dir="$SCRIPT_DIR/ofdm_tx_proj"
    local bits_src="$SCRIPT_DIR/tb_input_to_tx.bin"
    cat > /tmp/ofdm_tx_cosim.tcl << EOF
open_project -reset ofdm_tx_proj
set_top ofdm_tx
add_files src/ofdm_tx.cpp
add_files src/ofdm_tx.h
add_files src/scrambler.cpp
add_files src/scrambler.h
add_files src/interleaver.cpp
add_files src/interleaver.h
add_files src/conv_enc.cpp
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
add_files -tb tb/ofdm_tx_tb.cpp -cflags "-I./src"
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
# Step 1: C synthesis → generate RTL
csynth_design
# Step 2: Set HLS_TBDATA_DIR so cosim.tv.exe finds test vectors regardless of CWD.
# The TB uses tb_path() which prepends this dir when the env var is set.
set ::env(HLS_TBDATA_DIR) "$SCRIPT_DIR"
# Step 3: RTL co-simulation
# -rtl verilog      : Verilog RTL (faster than VHDL)
# -trace_level none : no waveform dump (faster)
# -O                : enable simulation optimisation
cosim_design -rtl verilog -trace_level none -O -argv "--mod ${mod_arg}"
puts "@I TX cosim complete"
close_project
exit
EOF
}

_write_rx_cosim_tcl() {
    local mod_arg="${1:-1}"
    local proj_dir="$SCRIPT_DIR/ofdm_rx_proj"
    local tx_out="$SCRIPT_DIR/tb_tx_output_hls.txt"
    local in_bits="$SCRIPT_DIR/tb_input_to_tx.bin"
    cat > /tmp/ofdm_rx_cosim.tcl << EOF
open_project -reset ofdm_rx_proj
set_top ofdm_rx
add_files src/ofdm_rx.cpp
add_files src/ofdm_rx.h
add_files src/ofdm_tx.h
add_files src/sync_detect.cpp
add_files src/sync_detect.h
add_files src/cfo_correct.cpp
add_files src/cfo_correct.h
add_files src/scrambler.cpp
add_files src/scrambler.h
add_files src/interleaver.cpp
add_files src/interleaver.h
add_files src/conv_enc.cpp
add_files src/viterbi_dec.cpp
add_files src/conv_fec.h
add_files -tb tb/ofdm_rx_tb.cpp -cflags "-I./src"
open_solution sol1 -reset
set_part xc7a50tcsg325-1
create_clock -period 10
# Step 1: C synthesis → generate RTL
csynth_design
# Step 2: Set HLS_TBDATA_DIR so cosim.tv.exe finds test vectors regardless of CWD.
set ::env(HLS_TBDATA_DIR) "$SCRIPT_DIR"
# Step 3: RTL co-simulation (only ofdm_rx becomes RTL; rest of TB runs as C)
cosim_design -rtl verilog -trace_level none -O -argv "--mod ${mod_arg}"
puts "@I RX cosim complete"
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
            echo "[run] Running TX C simulation (mod=${2:-1} rate=${3:-0})..."
            _write_csim_tcl "${2:-1}" "${3:-0}"
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_csim.tcl 2>&1 | tee vitis_csim.log
            echo "[run] Log saved to vitis_csim.log"
            ;;
        rx_csim)
            echo "[run] Running RX C simulation (mod=${2:-1} rate=${3:-0})..."
            _write_rx_csim_tcl "${2:-1}" "${3:-0}"
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_rx_csim.tcl 2>&1 | tee vitis_rx_csim.log
            echo "[run] Log saved to vitis_rx_csim.log"
            ;;
        rx_noisy_csim)
            echo "[run] Running RX C simulation on noisy signal (mod=${2:-1} rate=${3:-0})..."
            _write_rx_noisy_csim_tcl "${2:-1}" "${3:-0}"
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
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_rx.tcl 2>&1 | tee vitis_rx_synth.log
            echo "[run] Log saved to vitis_rx_synth.log"
            ;;
        tx_chain_synth)
            echo "[run] Running tx_chain C synthesis (scrambler+conv_enc+interleaver, 10 ns)..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_tx_chain.tcl 2>&1 | tee vitis_tx_chain_synth.log
            echo "[run] Log saved to vitis_tx_chain_synth.log"
            ;;
        sync_detect_synth)
            echo "[run] Running sync_detect C synthesis (free-running gate + inline CFO, 10 ns)..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_sync_detect.tcl 2>&1 | tee vitis_sync_detect_synth.log
            echo "[run] Log saved to vitis_sync_detect_synth.log"
            ;;
        fec_rx_synth)
            echo "[run] Running fec_rx C synthesis (interleaver+viterbi+scrambler, 5 ns / 200 MHz)..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_fec_rx.tcl 2>&1 | tee vitis_fec_rx_synth.log
            echo "[run] Log saved to vitis_fec_rx_synth.log"
            ;;
        ofdm_mac_synth)
            echo "[run] Running ofdm_mac C synthesis (MAC + PHY sequencer, 10 ns)..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_ofdm_mac.tcl 2>&1 | tee vitis_ofdm_mac_synth.log
            echo "[run] Log saved to vitis_ofdm_mac_synth.log"
            ;;
        conv_enc_synth)
            echo "[run] Running conv_enc C synthesis..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_conv_enc.tcl 2>&1 | tee vitis_conv_enc_synth.log
            echo "[run] Log saved to vitis_conv_enc_synth.log"
            ;;
        viterbi_synth)
            echo "[run] Running viterbi_dec C synthesis..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_viterbi.tcl 2>&1 | tee vitis_viterbi_synth.log
            echo "[run] Log saved to vitis_viterbi_synth.log"
            ;;
        sync_detect_synth)
            echo "[run] Running sync_detect C synthesis..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_sync_detect.tcl 2>&1 | tee vitis_sync_detect_synth.log
            echo "[run] Log saved to vitis_sync_detect_synth.log"
            ;;
        cfo_correct_synth)
            echo "[run] Running cfo_correct C synthesis..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_cfo_correct.tcl 2>&1 | tee vitis_cfo_correct_synth.log
            echo "[run] Log saved to vitis_cfo_correct_synth.log"
            ;;
        scrambler_synth)
            echo "[run] Running scrambler C synthesis..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_scrambler.tcl 2>&1 | tee vitis_scrambler_synth.log
            echo "[run] Log saved to vitis_scrambler_synth.log"
            ;;
        interleaver_synth)
            echo "[run] Running interleaver C synthesis..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_interleaver.tcl 2>&1 | tee vitis_interleaver_synth.log
            echo "[run] Log saved to vitis_interleaver_synth.log"
            ;;
        tx_cosim)
            echo "[run] TX RTL co-simulation (mod=${2:-1}) — csynth + cosim (slow, ~10-30 min)..."
            _write_tx_cosim_tcl "${2:-1}"
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_tx_cosim.tcl 2>&1 | tee vitis_tx_cosim.log
            echo "[run] Log saved to vitis_tx_cosim.log"
            grep -E "@I|@E|PASS|FAIL|cosim|error" vitis_tx_cosim.log | tail -20
            ;;
        rx_cosim)
            echo "[run] RX RTL co-simulation (mod=${2:-1}) — csynth + cosim (slow, ~20-60 min)..."
            _write_rx_cosim_tcl "${2:-1}"
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC /tmp/ofdm_rx_cosim.tcl 2>&1 | tee vitis_rx_cosim.log
            echo "[run] Log saved to vitis_rx_cosim.log"
            grep -E "@I|@E|PASS|FAIL|cosim|error" vitis_rx_cosim.log | tail -20
            ;;
        export_ip)
            echo "[run] Exporting all HLS IPs to ip_repo/..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/export_ip.tcl 2>&1 | tee vitis_export.log
            echo "[run] Log saved to vitis_export.log"
            ;;
        synth|ofdm_tx_synth)
            echo "[run] Running ofdm_tx C synthesis (TX modulator, 10 ns)..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_tx.tcl 2>&1 | tee vitis_ofdm_tx_synth.log
            echo "[run] Log saved to vitis_ofdm_tx_synth.log"
            ;;
        all)
            echo "[run] Running csim + synthesis..."
            cd "$SCRIPT_DIR"
            "$VITIS_HLS_BIN" $VITIS_HLS_EXEC tcl/synth_all.tcl 2>&1 | tee vitis_all.log
            echo "[run] Log saved to vitis_all.log"
            ;;
        check)
            _check_env
            ;;
        help|*)
            echo "Usage:"
            echo "  source setup_vitis.sh              # set env in current shell"
            echo "  ./setup_vitis.sh check             # verify vitis_hls launches"
            echo ""
            echo "  C Simulation:"
            echo "  ./setup_vitis.sh csim [mod]        # TX C-sim  (mod: 0=QPSK 1=16QAM, default 1)"
            echo "  ./setup_vitis.sh rx_csim           # RX C-sim  (requires tx csim output)"
            echo "  ./setup_vitis.sh rx_noisy_csim     # RX C-sim on noisy signal"
            echo "  ./setup_vitis.sh rx_noisy_build    # Build RX binary only (for BER sweep)"
            echo "  ./setup_vitis.sh fec_csim          # FEC encode+decode loopback test"
            echo ""
            echo "  RTL Co-simulation (csynth + cosim — slow, 10-60 min):"
            echo "  ./setup_vitis.sh tx_cosim [mod]    # TX RTL cosim: verifies ofdm_tx RTL vs C-sim"
            echo "  ./setup_vitis.sh rx_cosim [mod]    # RX RTL cosim: verifies ofdm_rx RTL vs C-sim"
            echo ""
            echo "  C Synthesis (resource + timing reports):"
            echo "  ./setup_vitis.sh synth             # ofdm_tx synthesis (10 ns)"
            echo "  ./setup_vitis.sh rx_synth          # ofdm_rx synthesis (10 ns)"
            echo "  ./setup_vitis.sh tx_chain_synth    # tx_chain (scram+enc+intlv, 10 ns)"
            echo "  ./setup_vitis.sh sync_cfo_synth    # sync_cfo (sync+cfo, 10 ns)"
            echo "  ./setup_vitis.sh fec_rx_synth      # fec_rx (intlv+vitv3+descram, 5 ns / 200 MHz)"
            echo "  ./setup_vitis.sh export_ip         # package 5 IPs → ip_repo/"
            echo "  ./setup_vitis.sh all               # TX csim + synth + report"
            ;;
    esac
else
    # Being sourced — just report status
    _check_env
fi
