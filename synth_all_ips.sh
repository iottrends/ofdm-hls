#!/bin/bash
cd "$(dirname "$0")"

SCRIPT_DIR="$(pwd)"
IP_DIR="$SCRIPT_DIR/ip_repo"

# ── 1. Clean ip_repo ─────────────────────────────────────
echo "===== Cleaning $IP_DIR ====="
rm -rf "$IP_DIR"
mkdir -p "$IP_DIR"
echo "  Done — old IPs removed."

# ── 2. Synth + export ────────────────────────────────────
STEPS=(
    tx_chain_synth
    ofdm_tx_synth
    sync_detect_synth
    rx_synth
    fec_rx_synth
    ofdm_mac_synth
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

# ── 3. Extract export.zip ────────────────────────────────
echo ""
echo "===== Extracting export.zip for each IP in $IP_DIR ====="
EXTRACT_FAIL=0
for d in "$IP_DIR"/*/; do
    if [ -f "$d/export.zip" ]; then
        name="$(basename "$d")"
        echo "  Extracting $name ..."
        if (cd "$d" && unzip -o export.zip > /dev/null 2>&1); then
            echo "  OK: $name"
        else
            echo "  FAILED: $name"
            EXTRACT_FAIL=$((EXTRACT_FAIL+1))
        fi
    fi
done

if [ "$EXTRACT_FAIL" -ne 0 ]; then
    echo "ERROR: $EXTRACT_FAIL zip extractions failed"
    exit 1
fi

# ── 4. Verify ────────────────────────────────────────────
echo ""
echo "===== Verifying component.xml ====="
MISSING=0
for d in tx_chain ofdm_tx sync_detect ofdm_rx fec_rx ofdm_mac; do
    if [ -f "$IP_DIR/$d/component.xml" ]; then
        echo "  OK: $d"
    else
        echo "  MISSING: $d"
        MISSING=$((MISSING+1))
    fi
done

if [ "$MISSING" -ne 0 ]; then
    echo "ERROR: $MISSING IPs missing component.xml"
    exit 1
fi

echo ""
echo "====================================="
echo " All IPs built and verified in $IP_DIR"
echo "====================================="
