#!/bin/bash
# ============================================================
# test_fec_gain.sh  —  Fine-grained SNR sweep to measure FEC coding gain
#
# Sweeps SNR in 1 dB steps for QPSK and 16QAM.
# Compares rate-1/2 vs rate-2/3 BER=0 threshold to quantify coding gain.
# ============================================================
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_common.sh"

echo ""
echo "======================================================"
echo "  FEC Coding Gain — Fine SNR Sweep (1 dB steps)"
echo "======================================================"

ensure_binaries

SEED_BASE=42
FRAMES=3

sweep_rate() {
    local mod=$1 rate=$2 nsyms=$3 snr_min=$4 snr_max=$5
    local mod_name rate_name
    [ "$mod" -eq 0 ] && mod_name="QPSK" || mod_name="16QAM"
    [ "$rate" -eq 0 ] && rate_name="r1/2" || rate_name="r2/3"

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
            add_noise "$SNR" awgn "$seed"
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
            be_str="no_decode"; ber_str="---"
        elif [ "$total_be" -eq 0 ] && [ "$sync_fails" -eq 0 ] && [ "$no_outs" -eq 0 ]; then
            be_str="0"; ber_str="0"
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
        echo "${SNR} ${be_str} ${ber_str}"
    done

    rm -f "$clean_tx" "$clean_ref"
}

print_comparison() {
    local mod=$1 mod_name=$2 nsyms_r12=$3 nsyms_r23=$4 snr_min=$5 snr_max=$6

    echo ""
    echo "  $mod_name, AWGN, n_syms=$nsyms_r12 (r1/2) / $nsyms_r23 (r2/3), $FRAMES frames/pt"
    echo "  --------------------------------------------------------"
    printf "  %-6s | %-10s %-12s | %-10s %-12s\n" "SNR" "r1/2 BE" "r1/2 BER" "r2/3 BE" "r2/3 BER"
    echo "  ------ | ---------- ------------ | ---------- ------------"

    # Collect r1/2 results
    local r12_data
    r12_data=$(sweep_rate "$mod" 0 "$nsyms_r12" "$snr_min" "$snr_max")

    # Collect r2/3 results
    local r23_data
    r23_data=$(sweep_rate "$mod" 1 "$nsyms_r23" "$snr_min" "$snr_max")

    # Print side by side
    for SNR in $(seq "$snr_min" 1 "$snr_max"); do
        local line12 line23
        line12=$(echo "$r12_data" | grep "^${SNR} ")
        line23=$(echo "$r23_data" | grep "^${SNR} ")

        local be12 ber12 be23 ber23
        be12=$(echo "$line12" | awk '{print $2}')
        ber12=$(echo "$line12" | awk '{print $3}')
        be23=$(echo "$line23" | awk '{print $2}')
        ber23=$(echo "$line23" | awk '{print $3}')

        printf "  %-6s | %-10s %-12s | %-10s %-12s\n" \
            "${SNR}dB" "$be12" "$ber12" "$be23" "$ber23"
    done
}

# QPSK: sweep 7-16 dB
print_comparison 0 "QPSK" 64 63 7 16

# 16QAM: sweep 12-22 dB
print_comparison 1 "16QAM" 64 63 12 22

echo ""
echo "  Coding gain = SNR where r2/3 reaches BER=0 minus SNR where r1/2 reaches BER=0"
echo "  Expected: ~2-3 dB for hard-decision Viterbi K=7"
echo ""
