#!/bin/bash
set -e
cd "$(dirname "$0")"

STEPS=(
    synth
    rx_synth
    sync_detect_synth
    cfo_correct_synth
    conv_enc_synth
    viterbi_synth
    scrambler_synth
    interleaver_synth
    export_ip
)

TOTAL=${#STEPS[@]}
PASS=0

for i in "${!STEPS[@]}"; do
    STEP="${STEPS[$i]}"
    echo ""
    echo "[$((i+1))/$TOTAL] ===== $STEP ====="
    if ./setup_vitis.sh "$STEP"; then
        echo "[$((i+1))/$TOTAL] DONE: $STEP"
        PASS=$((PASS+1))
    else
        echo ""
        echo "ERROR: $STEP failed — stopping."
        exit 1
    fi
done

echo ""
echo "====================================="
echo " All $PASS/$TOTAL steps completed OK"
echo "====================================="

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IP_DIR="$SCRIPT_DIR/ip_repo"

echo ""
echo "===== Extracting export.zip for each IP in $IP_DIR ====="
EXTRACT_FAIL=0
for d in "$IP_DIR"/*/; do
    if [ -f "$d/export.zip" ]; then
        echo "  Extracting $d/export.zip ..."
        if (cd "$d" && unzip -o export.zip > /dev/null 2>&1); then
            echo "  OK: $(basename "$d")"
        else
            echo "  FAILED: $(basename "$d")"
            EXTRACT_FAIL=$((EXTRACT_FAIL+1))
        fi
    fi
done

if [ "$EXTRACT_FAIL" -ne 0 ]; then
    echo "ERROR: $EXTRACT_FAIL zip extractions failed"
    exit 1
fi
echo "All export.zip files extracted — ip_repo hdl/ and component.xml are up to date."
