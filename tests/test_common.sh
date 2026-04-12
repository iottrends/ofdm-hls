#!/bin/bash
# ============================================================
# test_common.sh  —  Shared helpers for OFDM HLS test suite
#
# Source this file from any test script:
#   SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
#   source "$SCRIPT_DIR/test_common.sh"
# ============================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM_DIR="$ROOT_DIR/sim"
TX_BUILD="$ROOT_DIR/ofdm_tx_proj/sol1/csim/build"
RX_BUILD="$ROOT_DIR/ofdm_rx_proj/sol1/csim/build"
LD_PATH="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:\
/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:\
/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"

# Counters
_TEST_PASS=0
_TEST_FAIL=0
_TEST_SKIP=0

# Colour codes (auto-detect terminal)
if [ -t 1 ]; then
    _GREEN="\033[32m"; _RED="\033[31m"; _YELLOW="\033[33m"; _RESET="\033[0m"
else
    _GREEN=""; _RED=""; _YELLOW=""; _RESET=""
fi

pass() { _TEST_PASS=$((_TEST_PASS+1)); printf "  ${_GREEN}PASS${_RESET}  %s\n" "$*"; }
fail() { _TEST_FAIL=$((_TEST_FAIL+1)); printf "  ${_RED}FAIL${_RESET}  %s\n" "$*"; }
skip() { _TEST_SKIP=$((_TEST_SKIP+1)); printf "  ${_YELLOW}SKIP${_RESET}  %s\n" "$*"; }
info() { printf "  [..] %s\n" "$*"; }

summary() {
    local total=$((_TEST_PASS + _TEST_FAIL + _TEST_SKIP))
    echo ""
    echo "======================================================"
    printf "  Results: ${_GREEN}%d PASS${_RESET}  ${_RED}%d FAIL${_RESET}  ${_YELLOW}%d SKIP${_RESET}  / %d total\n" \
        $_TEST_PASS $_TEST_FAIL $_TEST_SKIP $total
    echo "======================================================"
    [ $_TEST_FAIL -eq 0 ] && return 0 || return 1
}

# Build TX and RX csim binaries if they don't already exist.
# Call once at the top of any test script that uses run_tx / run_rx.
ensure_binaries() {
    if [ ! -f "$TX_BUILD/csim.exe" ]; then
        info "Building TX C-sim binary..."
        (cd "$ROOT_DIR" && ./setup_vitis.sh csim 1 0 >/dev/null 2>&1)
    fi
    if [ ! -f "$RX_BUILD/csim.exe" ]; then
        info "Building RX C-sim binary..."
        (cd "$ROOT_DIR" && ./setup_vitis.sh rx_noisy_build >/dev/null 2>&1)
    fi
}

# run_tx MOD RATE N_SYMS
#   Generates input bits, runs HLS TX, leaves output in TX_BUILD.
#   Writes tb_input_to_tx.bin and tb_tx_output_hls.txt to ROOT_DIR.
run_tx() {
    local mod=${1:-1} rate=${2:-0} nsyms=${3:-255}
    python3 "$SIM_DIR/ofdm_reference.py" --gen --mod "$mod" --rate "$rate" --n-syms "$nsyms" \
        > /dev/null 2>&1
    cp "$ROOT_DIR/tb_input_to_tx.bin" "$TX_BUILD/tb_input_to_tx.bin"
    (cd "$TX_BUILD" && \
     LD_LIBRARY_PATH="${LD_PATH}:${LD_LIBRARY_PATH:-}" \
     ./csim.exe --mod "$mod" --rate "$rate" --n-syms "$nsyms" > /dev/null 2>&1) || return 1
    cp "$TX_BUILD/tb_tx_output_hls.txt" "$ROOT_DIR/tb_tx_output_hls.txt"
}

# run_rx MOD RATE N_SYMS  →  sets RX_BER, RX_BE, RX_TOTAL_BITS, RX_EXIT
#   Expects tb_tx_output_hls.txt and tb_input_to_tx.bin in ROOT_DIR.
run_rx() {
    local mod=${1:-1} rate=${2:-0} nsyms=${3:-255}
    cp "$ROOT_DIR/tb_tx_output_hls.txt" "$RX_BUILD/tb_tx_output_hls.txt"
    cp "$ROOT_DIR/tb_input_to_tx.bin"   "$RX_BUILD/tb_input_to_tx.bin"
    local out
    out=$(cd "$RX_BUILD" && \
          LD_LIBRARY_PATH="${LD_PATH}:${LD_LIBRARY_PATH:-}" \
          ./csim.exe --mod "$mod" --rate "$rate" --n-syms "$nsyms" 2>&1) || true
    RX_EXIT=$?
    RX_BE=$(echo "$out" | grep -i "Bit.*errors" | grep -oP '\d+\s*/\s*\d+' | head -1 | awk -F/ '{print $1}' | tr -d ' ')
    RX_TOTAL_BITS=$(echo "$out" | grep -i "Bit.*errors" | grep -oP '\d+\s*/\s*\d+' | head -1 | awk -F/ '{print $2}' | tr -d ' ')
    RX_BE=${RX_BE:-NA}
    RX_TOTAL_BITS=${RX_TOTAL_BITS:-0}
    RX_HEADER_ERR=$(echo "$out" | grep -c "header CRC error")
    RX_RAW="$out"
}

# add_noise SNR [CHANNEL] [SEED]
#   Applies channel sim to tb_tx_output_hls.txt → tb_tx_output_hls_noise.txt → tb_tx_output_hls.txt
add_noise() {
    local snr=${1:-20} channel=${2:-awgn} seed=${3:-42}
    (cd "$ROOT_DIR" && python3 "$SIM_DIR/ofdm_channel_sim.py" \
        --channel "$channel" --snr "$snr" --seed "$seed" \
        --write-noisy --input tb_tx_output_hls.txt > /dev/null 2>&1)
    cp "$ROOT_DIR/tb_tx_output_hls_noise.txt" "$ROOT_DIR/tb_tx_output_hls.txt"
}
