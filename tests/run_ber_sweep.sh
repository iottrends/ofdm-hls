#!/bin/bash
# ============================================================
# run_ber_sweep.sh  —  BER sweep over modcods × SNR
#
# Same flow as run_loopback_noisy.sh (gen → TX csim → channel sim →
# ./setup_vitis.sh rx_noisy_csim → grep vitis_rx_noisy_csim.log) but
# wrapped in an outer loop over (mod, rate, SNR).  TX C-sim runs once
# per modcod; channel sim + RX C-sim run per SNR point.
#
# Channel: 'combined' = CFO + multipath + AWGN + phase noise
# (see sim/ofdm_channel_sim.py:apply_channel for the impairment chain).
#
# Defaults:
#   Modcods : QPSK 1/2, QPSK 2/3, 16QAM 1/2, 16QAM 2/3
#   SNR     : 5..22 dB step 1
#
# Usage:
#   ./run_ber_sweep.sh                          # full sweep
#   ./run_ber_sweep.sh --snr-step 2             # coarser sweep (~25 min)
#   ./run_ber_sweep.sh --snr "10 15 20"         # custom SNR list
#   ./run_ber_sweep.sh --channel awgn           # AWGN only (no impairments)
#   ./run_ber_sweep.sh --modcods "0:0 1:1"      # subset of modcods
# ============================================================

set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."   # operate from repo root

# ── Defaults ─────────────────────────────────────────────────
SNR_LIST=""
SNR_MIN=5
SNR_MAX=22
SNR_STEP=1
CHANNEL="combined"
MODCODS="0:0 0:1 1:0 1:1"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --snr)       SNR_LIST="$2";  shift 2 ;;
        --snr-min)   SNR_MIN="$2";   shift 2 ;;
        --snr-max)   SNR_MAX="$2";   shift 2 ;;
        --snr-step)  SNR_STEP="$2";  shift 2 ;;
        --channel)   CHANNEL="$2";   shift 2 ;;
        --modcods)   MODCODS="$2";   shift 2 ;;
        -h|--help)
            grep -E "^# " "$0" | sed 's/^# //'; exit 0 ;;
        *) echo "[ERR] unknown arg: $1"; exit 1 ;;
    esac
done

# Build SNR list if not explicitly given
if [[ -z "$SNR_LIST" ]]; then
    SNR_LIST=$(seq "$SNR_MIN" "$SNR_STEP" "$SNR_MAX" | tr '\n' ' ')
fi

# ── Helpers ──────────────────────────────────────────────────
sep() { echo ""; echo "──────────────────────────────────────────────────────"; }
hdr() { sep; echo "  $*"; sep; }
mod_name()  { [ "$1" -eq 0 ] && echo "QPSK"  || echo "16QAM"; }
rate_name() { [ "$1" -eq 0 ] && echo "1/2"   || echo "2/3";   }

echo ""
echo "======================================================"
echo "  OFDM BER Sweep — HLS RX (rx_noisy_csim flow)"
echo "  Modcods : $MODCODS  (mod:rate)"
echo "  Channel : $CHANNEL  (combined = CFO+multipath+AWGN+phase noise)"
echo "  SNR     : $SNR_LIST dB"
echo "======================================================"

SUMMARY_CSV="ber_sweep_summary.csv"
echo "modcod,snr_db,bit_errors,total_bits,ber,status" > "$SUMMARY_CSV"

# Stash the per-frame Vitis log path so we can grep after each rx_noisy_csim
RX_LOG="vitis_rx_noisy_csim.log"

# ── Outer loop: modcods ──────────────────────────────────────
for MC in $MODCODS; do
    MOD="${MC%:*}"
    RATE="${MC#*:}"
    MOD_N=$(mod_name "$MOD")
    RATE_N=$(rate_name "$RATE")
    LABEL="${MOD_N}-${RATE_N}"

    hdr "MODCOD  $LABEL   (mod=$MOD rate=$RATE)"

    # Step 1+2: gen bits + TX C-sim — once per modcod
    echo "  [INFO]  Generating input bits + Python TX reference"
    python3 sim/ofdm_reference.py --gen --mod "$MOD" --rate "$RATE" > /dev/null
    echo "  [INFO]  Running HLS TX C-sim"
    ./setup_vitis.sh csim "$MOD" "$RATE" > /tmp/ber_sweep_tx_csim.log 2>&1 || true
    if [ ! -f tb_tx_output_hls.txt ]; then
        echo "  [ERR]   HLS TX C-sim failed for $LABEL — see /tmp/ber_sweep_tx_csim.log"
        continue
    fi
    echo "  [OK]    Clean TX IQ ready ($(wc -l < tb_tx_output_hls.txt) samples)"

    CSV_FILE="ber_sweep_mod${MOD}_rate${RATE}.csv"
    echo "snr_db,bit_errors,total_bits,ber,status" > "$CSV_FILE"

    echo ""
    printf "  %8s  %10s  %10s  %12s  %s\n" "SNR(dB)" "BitErr" "TotBits" "BER" "Status"
    echo "  -------------------------------------------------------------"

    for SNR in $SNR_LIST; do
        # Channel sim → tb_tx_output_hls_noise.txt
        # Use a deterministic seed per (modcod, snr) so re-runs reproduce.
        SEED=$(( SNR * 1000 + MOD * 100 + RATE * 10 + 42 ))
        python3 sim/ofdm_channel_sim.py \
            --channel "$CHANNEL" --snr "$SNR" --seed "$SEED" \
            --mod "$MOD" --write-noisy \
            --input tb_tx_output_hls.txt > /dev/null 2>&1

        if [ ! -f tb_tx_output_hls_noise.txt ]; then
            printf "  %8s  %10s  %10s  %12s  %s\n" "$SNR" "-" "-" "-" "chan-sim-fail"
            echo "$LABEL,$SNR,,,,chan-sim-fail" >> "$SUMMARY_CSV"
            echo "$SNR,,,,chan-sim-fail" >> "$CSV_FILE"
            continue
        fi

        # RX C-sim — same proven flow as run_loopback_noisy.sh.
        # rx_noisy_csim writes vitis_rx_noisy_csim.log; binary may exit
        # non-zero on TB FAIL, so swallow with || true.
        ./setup_vitis.sh rx_noisy_csim "$MOD" "$RATE" > /dev/null 2>&1 || true

        # Parse — same approach as run_loopback_noisy.sh:106-108
        BE=$(grep '\[TB\] Bit  errors' "$RX_LOG" | head -1 | awk '{print $5}')
        TB=$(grep '\[TB\] Bit  errors' "$RX_LOG" | head -1 | awk '{print $7}')

        if [[ -n "$BE" && -n "$TB" && "$TB" -gt 0 ]]; then
            BER=$(python3 -c "print(f'{$BE/$TB:.3e}')")
            if [ "$BE" -eq 0 ]; then
                STATUS="clean"
            else
                STATUS="errors"
            fi
            printf "  %8s  %10d  %10d  %12s  %s\n" "$SNR" "$BE" "$TB" "$BER" "$STATUS"
            echo "$LABEL,$SNR,$BE,$TB,$BER,$STATUS" >> "$SUMMARY_CSV"
            echo "$SNR,$BE,$TB,$BER,$STATUS" >> "$CSV_FILE"
        elif grep -q "header CRC error" "$RX_LOG"; then
            # Decoder couldn't lock onto the header → treat as catastrophic
            printf "  %8s  %10s  %10s  %12s  %s\n" "$SNR" "-" "-" "-" "hdr-crc-fail"
            echo "$LABEL,$SNR,,,,hdr-crc-fail" >> "$SUMMARY_CSV"
            echo "$SNR,,,,hdr-crc-fail" >> "$CSV_FILE"
        else
            printf "  %8s  %10s  %10s  %12s  %s\n" "$SNR" "-" "-" "-" "rx-no-output"
            echo "$LABEL,$SNR,,,,rx-no-output" >> "$SUMMARY_CSV"
            echo "$SNR,,,,rx-no-output" >> "$CSV_FILE"
        fi
    done

    echo "  [OK]    Wrote: $CSV_FILE"
done

echo ""
echo "======================================================"
echo "  BER SWEEP COMPLETE"
echo "======================================================"
echo "  Per-modcod CSVs : ber_sweep_mod{M}_rate{R}.csv"
echo "  Summary CSV     : $SUMMARY_CSV"
echo "======================================================"
echo ""
