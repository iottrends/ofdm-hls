#!/bin/bash
# ============================================================
# test_combined_sweep.sh — BER sweep with combined channel
#   (AWGN + phase noise + multipath + CFO)
#
# Sweeps all 4 mode combinations at 1 dB steps, 3 frames/pt.
# ============================================================
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_common.sh"

echo ""
echo "======================================================"
echo "  Combined Channel BER Sweep (AWGN+phase+multipath+CFO)"
echo "  3 frames per SNR point, 1 dB steps"
echo "======================================================"

ensure_binaries

FRAMES=3
SEED_BASE=42
CHANNEL=combined

sweep_mode() {
    local mod=$1 rate=$2 nsyms=$3 snr_min=$4 snr_max=$5
    local mod_name rate_name
    [ "$mod" -eq 0 ] && mod_name="QPSK" || mod_name="16QAM"
    [ "$rate" -eq 0 ] && rate_name="r1/2" || rate_name="r2/3"

    echo ""
    echo "  $mod_name $rate_name, n_syms=$nsyms, $FRAMES frames/pt"
    echo "  --------------------------------------------------------"
    printf "  %-6s  %-14s  %-12s\n" "SNR" "Bit Errors" "BER"
    echo "  ------  --------------  ------------"

    run_tx "$mod" "$rate" "$nsyms"
    local clean_tx=$(mktemp)
    local clean_ref=$(mktemp)
    cp "$ROOT_DIR/tb_tx_output_hls.txt" "$clean_tx"
    cp "$ROOT_DIR/tb_input_to_tx.bin"   "$clean_ref"

    for SNR in $(seq "$snr_min" 1 "$snr_max"); do
        local total_be=0 total_bits=0 sync_fails=0 no_outs=0

        for frame in $(seq 1 $FRAMES); do
            local seed=$((SEED_BASE + SNR * 997 + frame * 9973))
            cp "$clean_tx"  "$ROOT_DIR/tb_tx_output_hls.txt"
            cp "$clean_ref" "$ROOT_DIR/tb_input_to_tx.bin"
            add_noise "$SNR" "$CHANNEL" "$seed"
            run_rx "$mod" "$rate" "$nsyms"

            if [ "$RX_HEADER_ERR" -gt 0 ]; then
                sync_fails=$((sync_fails + 1))
            elif [ "$RX_BE" = "NA" ]; then
                no_outs=$((no_outs + 1))
            else
                total_be=$((total_be + RX_BE))
                total_bits=$((total_bits + RX_TOTAL_BITS))
            fi
        done

        local be_str ber_str
        if [ "$total_bits" -eq 0 ]; then
            be_str="no_decode"
            ber_str="---"
        elif [ "$total_be" -eq 0 ] && [ "$sync_fails" -eq 0 ] && [ "$no_outs" -eq 0 ]; then
            be_str="0"
            ber_str="0"
        else
            be_str="${total_be}"
            [ "$sync_fails" -gt 0 ] && be_str="${be_str}+${sync_fails}sync"
            [ "$no_outs" -gt 0 ] && be_str="${be_str}+${no_outs}no"
            if [ "$total_bits" -gt 0 ]; then
                ber_str=$(python3 -c "print(f'{$total_be/$total_bits:.2e}')")
            else
                ber_str="---"
            fi
        fi
        printf "  %-6s  %-14s  %-12s\n" "${SNR}dB" "$be_str" "$ber_str"
    done

    rm -f "$clean_tx" "$clean_ref"
}

# QPSK r1/2:  5–14 dB
sweep_mode 0 0 64 5 14

# QPSK r2/3:  7–16 dB
sweep_mode 0 1 63 7 16

# 16QAM r1/2: 10–20 dB
sweep_mode 1 0 64 10 20

# 16QAM r2/3: 12–22 dB
sweep_mode 1 1 63 12 22

echo ""
echo "  Channel: combined (CFO + multipath + AWGN + phase noise)"
echo "  CFO=0.3 subcarrier spacings, phase sigma=0.005 rad/sample"
echo "  Multipath: [0, 1.0], [3, 0.3∠-60°], [7, 0.1∠120°]"
echo ""
