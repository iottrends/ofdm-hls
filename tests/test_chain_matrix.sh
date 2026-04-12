#!/bin/bash
# ============================================================
# test_chain_matrix.sh  —  L2+L3: TX conformance + RX clean-channel
#
# Tests every (mod, rate, n_syms) combination on a clean channel.
#   - TX: sample count must match (n_syms+3)*288
#   - RX: BER must be 0 (no channel impairment)
#
# Usage:
#   ./tests/test_chain_matrix.sh          # full (~5 min)
#   ./tests/test_chain_matrix.sh --quick  # fewer n_syms values (~2 min)
# ============================================================
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_common.sh"

QUICK=0
[[ "${1:-}" == "--quick" ]] && QUICK=1

echo ""
echo "======================================================"
echo "  L2+L3 — TX/RX Chain Matrix (clean channel)"
echo "======================================================"

ensure_binaries

# Parameter space
MODS="0 1"
RATES="0 1"
if [ "$QUICK" -eq 1 ]; then
    NSYMS_LIST="1 3 16 255"
else
    # Include multiples of 3 for rate-2/3 coverage, plus boundary values
    NSYMS_LIST="1 3 6 16 18 64 128 129 200 252 254 255"
fi

for MOD in $MODS; do
    MOD_NAME=$([ "$MOD" -eq 0 ] && echo "QPSK" || echo "16QAM")
    for RATE in $RATES; do
        RATE_NAME=$([ "$RATE" -eq 0 ] && echo "1/2" || echo "2/3")
        for NSYMS in $NSYMS_LIST; do
            # Rate-2/3 requires total_coded divisible by 3.
            # coded_per_sym is 50 (QPSK) or 100 (16QAM), neither div by 3,
            # so n_syms itself must be divisible by 3.
            if [ "$RATE" -eq 1 ] && [ $((NSYMS % 3)) -ne 0 ]; then
                skip "${MOD_NAME} rate-${RATE_NAME} n_syms=${NSYMS} (need n_syms%3==0)"
                continue
            fi

            LABEL="${MOD_NAME} rate-${RATE_NAME} n_syms=${NSYMS}"
            info "$LABEL"

            # TX: generate + run
            if ! run_tx "$MOD" "$RATE" "$NSYMS"; then
                fail "$LABEL — TX C-sim error"
                continue
            fi

            # TX: verify sample count
            TX_SAMPLES=$(wc -l < "$ROOT_DIR/tb_tx_output_hls.txt" | tr -d ' ')
            EXPECTED=$(( (NSYMS + 3) * 288 ))
            if [ "$TX_SAMPLES" -ne "$EXPECTED" ]; then
                fail "$LABEL — TX sample count: got $TX_SAMPLES, expected $EXPECTED"
                continue
            fi

            # RX: decode on clean channel
            run_rx "$MOD" "$RATE" "$NSYMS"

            if [ "$RX_HEADER_ERR" -gt 0 ]; then
                fail "$LABEL — RX header CRC error"
            elif [ "$RX_BE" = "NA" ]; then
                fail "$LABEL — RX no output"
            elif [ "$RX_BE" -eq 0 ]; then
                pass "$LABEL — BER=0"
            else
                fail "$LABEL — BER>0 ($RX_BE / $RX_TOTAL_BITS bit errors)"
            fi
        done
    done
done

summary
