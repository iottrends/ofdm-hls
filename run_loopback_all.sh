#!/bin/bash
# ============================================================
# run_loopback_all.sh  —  Sweep all 4 modcods through run_loopback.sh
#
# Modcods exercised:
#   QPSK   rate-1/2  (--mod 0 --rate 0)
#   QPSK   rate-2/3  (--mod 0 --rate 1)
#   16-QAM rate-1/2  (--mod 1 --rate 0)
#   16-QAM rate-2/3  (--mod 1 --rate 1)
#
# Each combo runs the full 5-step loopback (Python ref gen, HLS TX
# csim, EVM compare, HLS RX csim, Python ref decode).  Per-modcod
# RESULT line is captured and an aggregate summary is printed.
#
# Usage:
#   ./run_loopback_all.sh
# ============================================================

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

LOG_DIR="/tmp/loopback_all_$$"
mkdir -p "$LOG_DIR"

declare -a COMBOS=(
    "0 0 QPSK   rate-1/2"
    "0 1 QPSK   rate-2/3"
    "1 0 16-QAM rate-1/2"
    "1 1 16-QAM rate-2/3"
)

echo ""
echo "======================================================"
echo "  Modcod Sweep — 4 combinations"
echo "======================================================"

PASS_TOTAL=0
FAIL_TOTAL=0
declare -a SUMMARY=()

for combo in "${COMBOS[@]}"; do
    read -r m r mod_name rate_name <<< "$combo"
    label="${mod_name}/${rate_name}"
    log_file="$LOG_DIR/loopback_m${m}_r${r}.log"

    echo ""
    echo "──────────────────────────────────────────────────────"
    echo "  RUN: ${label}  (--mod $m --rate $r)"
    echo "  Log: ${log_file}"
    echo "──────────────────────────────────────────────────────"

    # Run loopback; capture full log, stream a tail so user sees progress
    if ./run_loopback.sh --mod "$m" --rate "$r" > "$log_file" 2>&1; then
        result="PASS"
        PASS_TOTAL=$((PASS_TOTAL + 1))
    else
        result="FAIL"
        FAIL_TOTAL=$((FAIL_TOTAL + 1))
    fi

    # Pull headline stats from the per-run log
    evm=$(grep -oP "TX EVM check passed.*EVM = \K[0-9.]+%" "$log_file" | head -1)
    ber_line=$(grep -E "BER\s*:" "$log_file" | tail -1)

    SUMMARY+=("${label}  ${result}  EVM=${evm:-?}  ${ber_line:-no BER line}")
    echo "  → ${result}  EVM=${evm:-?}"
done

# ── Aggregate summary ─────────────────────────────────────────
echo ""
echo "======================================================"
echo "  MODCOD SWEEP SUMMARY"
echo "======================================================"
printf "  %-20s %-6s %-12s %s\n" "MODCOD" "RESULT" "TX-EVM" "RX-BER"
echo "  ----------------------------------------------------"
for line in "${SUMMARY[@]}"; do
    printf "  %s\n" "$line"
done
echo ""
echo "  Combinations passed : ${PASS_TOTAL} / ${#COMBOS[@]}"
echo "  Combinations failed : ${FAIL_TOTAL} / ${#COMBOS[@]}"
echo ""
if [ $FAIL_TOTAL -eq 0 ]; then
    echo "  RESULT: ALL MODCODS PASS"
else
    echo "  RESULT: ${FAIL_TOTAL} MODCOD(S) FAILED — inspect logs in ${LOG_DIR}"
fi
echo "======================================================"
echo ""

[ $FAIL_TOTAL -eq 0 ]
