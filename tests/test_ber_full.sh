#!/bin/bash
# ============================================================
# test_ber_full.sh  —  L4: Full BER matrix sweep
#
# Loops over mod × rate × channel × SNR × frames using the
# pre-compiled RX binary (same strategy as run_ber_sweep.sh).
#
# Pass criteria:
#   - BER = 0 at SNR >= 20 dB for all mod/rate/channel combos
#   - No header CRC errors at SNR >= 15 dB
#   - 1 dB fine-grain sweep on AWGN confirms no sync_detect
#     energy-floor regression in the 23-29 dB band
#
# Usage:
#   ./tests/test_ber_full.sh          # full matrix (~30-60 min)
#   ./tests/test_ber_full.sh --quick  # reduced (~5-10 min)
# ============================================================
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_common.sh"

QUICK=0
[[ "${1:-}" == "--quick" ]] && QUICK=1

echo ""
echo "======================================================"
echo "  L4 — Full BER Matrix Sweep"
echo "======================================================"

ensure_binaries

# ── Configuration ─────────────────────────────────────────────
MODS="0 1"
RATES="0 1"
NSYMS=255

if [ "$QUICK" -eq 1 ]; then
    CHANNELS="awgn combined"
    SNR_POINTS="5 10 15 20 25"
    FRAMES=3
    FINE_SNR_LIST=$(seq 15 2 35)
else
    CHANNELS="awgn phase multipath combined"
    SNR_POINTS="0 5 10 15 20 25 30"
    FRAMES=5
    FINE_SNR_LIST=$(seq 0 1 35)
fi

CSV_DIR="$ROOT_DIR/tests/ber_results"
mkdir -p "$CSV_DIR"

# ── Part A: Full mod × rate × channel sweep ───────────────────
echo ""
echo "  Part A: mod × rate × channel BER sweep"
echo "  --------------------------------------------------------"

for MOD in $MODS; do
    MOD_NAME=$([ "$MOD" -eq 0 ] && echo "QPSK" || echo "16QAM")
    for RATE in $RATES; do
        RATE_NAME=$([ "$RATE" -eq 0 ] && echo "r12" || echo "r23")
        LABEL="${MOD_NAME}_${RATE_NAME}"
        CSV="$CSV_DIR/ber_${LABEL}.csv"
        echo "channel,snr_db,frame,bit_errors,total_bits" > "$CSV"

        # Generate TX IQ once per (mod, rate)
        run_tx "$MOD" "$RATE" "$NSYMS"
        CLEAN_TX=$(mktemp)
        cp "$ROOT_DIR/tb_tx_output_hls.txt" "$CLEAN_TX"

        for CHANNEL in $CHANNELS; do
            for SNR in $SNR_POINTS; do
                POINT_ERRORS=0; POINT_BITS=0; POINT_CRC_FAIL=0

                for FRAME in $(seq 1 $FRAMES); do
                    SEED=$(( FRAME * 9973 + ${SNR%%.*} * 997 + $(echo "$CHANNEL" | cksum | awk '{print $1}') % 997 ))

                    # Restore clean TX before applying noise
                    cp "$CLEAN_TX" "$ROOT_DIR/tb_tx_output_hls.txt"
                    add_noise "$SNR" "$CHANNEL" "$SEED"
                    run_rx "$MOD" "$RATE" "$NSYMS"

                    local_be=0; local_bits=0
                    if [ "$RX_BE" != "NA" ]; then
                        local_be=$RX_BE
                        local_bits=$RX_TOTAL_BITS
                    fi
                    POINT_ERRORS=$((POINT_ERRORS + local_be))
                    POINT_BITS=$((POINT_BITS + local_bits))
                    if [ "$RX_HEADER_ERR" -gt 0 ]; then
                        POINT_CRC_FAIL=$((POINT_CRC_FAIL + 1))
                    fi
                    echo "$CHANNEL,$SNR,$FRAME,$local_be,$local_bits" >> "$CSV"
                done

                # Per-point verdict
                SNR_INT=${SNR%%.*}
                if [ "$SNR_INT" -ge 20 ]; then
                    if [ "$POINT_ERRORS" -eq 0 ] && [ "$POINT_BITS" -gt 0 ]; then
                        pass "$LABEL  $CHANNEL  SNR=${SNR}dB  BER=0  ($POINT_BITS bits)"
                    elif [ "$POINT_CRC_FAIL" -gt 0 ]; then
                        fail "$LABEL  $CHANNEL  SNR=${SNR}dB  $POINT_CRC_FAIL/$FRAMES CRC errors"
                    elif [ "$POINT_BITS" -eq 0 ]; then
                        fail "$LABEL  $CHANNEL  SNR=${SNR}dB  no output"
                    else
                        BER=$(python3 -c "print(f'{$POINT_ERRORS/$POINT_BITS:.2e}')")
                        fail "$LABEL  $CHANNEL  SNR=${SNR}dB  BER=$BER ($POINT_ERRORS errors)"
                    fi
                elif [ "$SNR_INT" -ge 15 ]; then
                    # Between 15-19 dB: CRC should not fail, but some bit errors OK
                    if [ "$POINT_CRC_FAIL" -gt 0 ]; then
                        fail "$LABEL  $CHANNEL  SNR=${SNR}dB  $POINT_CRC_FAIL CRC errors (unexpected >=15dB)"
                    fi
                fi
            done
        done
        rm -f "$CLEAN_TX"

        # Restore clean TX for next mod/rate combo
        run_tx "$MOD" "$RATE" "$NSYMS" 2>/dev/null || true
    done
done

# ── Part B: Fine SNR regression (sync_detect energy floor) ────
echo ""
echo "  Part B: sync_detect energy-floor regression (fine SNR sweep)"
echo "  --------------------------------------------------------"

# Use short frame for speed, AWGN only
FINE_MOD=1; FINE_RATE=0; FINE_NSYMS=16
run_tx $FINE_MOD $FINE_RATE $FINE_NSYMS
CLEAN_TX=$(mktemp)
cp "$ROOT_DIR/tb_tx_output_hls.txt" "$CLEAN_TX"

for SNR in $FINE_SNR_LIST; do
    cp "$CLEAN_TX" "$ROOT_DIR/tb_tx_output_hls.txt"
    add_noise "$SNR" awgn $((SNR * 997 + 42))
    run_rx $FINE_MOD $FINE_RATE $FINE_NSYMS

    if [ "$SNR" -ge 15 ]; then
        if [ "$RX_HEADER_ERR" -gt 0 ]; then
            fail "fine-SNR  AWGN  SNR=${SNR}dB  header CRC error (ENERGY_FLOOR regression!)"
        elif [ "$RX_BE" != "NA" ] && [ "$RX_BE" -eq 0 ]; then
            pass "fine-SNR  AWGN  SNR=${SNR}dB  BER=0"
        fi
    fi
done
rm -f "$CLEAN_TX"

# ── Summary ───────────────────────────────────────────────────
echo ""
echo "  BER CSVs in: $CSV_DIR/"
summary
