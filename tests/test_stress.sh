#!/bin/bash
# ============================================================
# test_stress.sh  —  L5: Boundary and stress tests
#
# Tests:
#   1. CFO sweep: -0.45 to +0.45 in 0.05 steps (SNR=20, AWGN)
#   2. Phase noise sigma sweep: 0.001 to 0.05
#   3. Minimal frame (n_syms=1, all valid mod/rate combos)
#   4. Max frame (n_syms=255, all valid mod/rate combos)
#   5. Near-boundary frames (n_syms=2,3,253,254,255)
#
# Usage:
#   ./tests/test_stress.sh          # full (~10-15 min)
#   ./tests/test_stress.sh --quick  # reduced (~3-5 min)
# ============================================================
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_common.sh"

QUICK=0
[[ "${1:-}" == "--quick" ]] && QUICK=1

echo ""
echo "======================================================"
echo "  L5 — Stress & Boundary Tests"
echo "======================================================"

ensure_binaries

# ── Test 1: CFO sweep ────────────────────────────────────────
# Verify cfo_correct compensates CFO across the estimator's range.
# At SNR=20 dB on AWGN, BER should be 0 for |CFO| <= 0.45 SC.
echo ""
echo "  Test 1: CFO sweep (SNR=20, AWGN, 16QAM rate-1/2, n_syms=16)"
echo "  --------------------------------------------------------"

CFO_MOD=1; CFO_RATE=0; CFO_NSYMS=16; CFO_SNR=20
run_tx $CFO_MOD $CFO_RATE $CFO_NSYMS
CLEAN_TX=$(mktemp)
cp "$ROOT_DIR/tb_tx_output_hls.txt" "$CLEAN_TX"

if [ "$QUICK" -eq 1 ]; then
    CFO_LIST="-0.4 -0.2 0.0 0.2 0.4"
else
    CFO_LIST="-0.45 -0.4 -0.35 -0.3 -0.25 -0.2 -0.15 -0.1 -0.05 0.0 0.05 0.1 0.15 0.2 0.25 0.3 0.35 0.4 0.45"
fi

for CFO in $CFO_LIST; do
    cp "$CLEAN_TX" "$ROOT_DIR/tb_tx_output_hls.txt"
    # Use channel sim with CFO injection
    (cd "$ROOT_DIR" && python3 "$SIM_DIR/ofdm_channel_sim.py" \
        --channel awgn --snr "$CFO_SNR" --seed 42 \
        --cfo-sc "$CFO" --write-noisy \
        --input tb_tx_output_hls.txt > /dev/null 2>&1)
    cp "$ROOT_DIR/tb_tx_output_hls_noise.txt" "$ROOT_DIR/tb_tx_output_hls.txt"
    run_rx $CFO_MOD $CFO_RATE $CFO_NSYMS

    if [ "$RX_HEADER_ERR" -gt 0 ]; then
        fail "CFO=${CFO} sc  header CRC error"
    elif [ "$RX_BE" != "NA" ] && [ "$RX_BE" -eq 0 ]; then
        pass "CFO=${CFO} sc  BER=0"
    elif [ "$RX_BE" != "NA" ]; then
        fail "CFO=${CFO} sc  BER>0 ($RX_BE / $RX_TOTAL_BITS)"
    else
        fail "CFO=${CFO} sc  no output"
    fi
done
rm -f "$CLEAN_TX"

# ── Test 2: Phase noise sigma sweep ──────────────────────────
echo ""
echo "  Test 2: Phase noise sigma sweep (SNR=20, 16QAM rate-1/2, n_syms=16)"
echo "  --------------------------------------------------------"

PN_MOD=1; PN_RATE=0; PN_NSYMS=16; PN_SNR=20
run_tx $PN_MOD $PN_RATE $PN_NSYMS
CLEAN_TX=$(mktemp)
cp "$ROOT_DIR/tb_tx_output_hls.txt" "$CLEAN_TX"

if [ "$QUICK" -eq 1 ]; then
    SIGMA_LIST="0.001 0.005 0.02 0.05"
else
    SIGMA_LIST="0.001 0.002 0.005 0.01 0.02 0.03 0.04 0.05"
fi

for SIGMA in $SIGMA_LIST; do
    cp "$CLEAN_TX" "$ROOT_DIR/tb_tx_output_hls.txt"
    (cd "$ROOT_DIR" && python3 "$SIM_DIR/ofdm_channel_sim.py" \
        --channel phase --snr "$PN_SNR" --seed 42 \
        --phase-sigma "$SIGMA" --write-noisy \
        --input tb_tx_output_hls.txt > /dev/null 2>&1)
    cp "$ROOT_DIR/tb_tx_output_hls_noise.txt" "$ROOT_DIR/tb_tx_output_hls.txt"
    run_rx $PN_MOD $PN_RATE $PN_NSYMS

    if [ "$RX_HEADER_ERR" -gt 0 ]; then
        fail "phase-sigma=${SIGMA}  header CRC error"
    elif [ "$RX_BE" != "NA" ] && [ "$RX_BE" -eq 0 ]; then
        pass "phase-sigma=${SIGMA}  BER=0"
    elif [ "$RX_BE" != "NA" ]; then
        # Phase noise can cause errors; just report, only fail for CRC
        BER=$(python3 -c "print(f'{$RX_BE/$RX_TOTAL_BITS:.2e}')")
        info "phase-sigma=${SIGMA}  BER=$BER ($RX_BE errors — phase noise)"
    else
        fail "phase-sigma=${SIGMA}  no output"
    fi
done
rm -f "$CLEAN_TX"

# ── Test 3: Minimal frame ────────────────────────────────────
echo ""
echo "  Test 3: Minimal frame (n_syms=1, clean channel)"
echo "  --------------------------------------------------------"

for MOD in 0 1; do
    MOD_NAME=$([ "$MOD" -eq 0 ] && echo "QPSK" || echo "16QAM")
    # Rate-1/2 only (n_syms=1 not divisible by 3 for rate-2/3)
    run_tx "$MOD" 0 1
    run_rx "$MOD" 0 1
    if [ "$RX_BE" != "NA" ] && [ "$RX_BE" -eq 0 ]; then
        pass "n_syms=1  ${MOD_NAME} rate-1/2  BER=0"
    else
        fail "n_syms=1  ${MOD_NAME} rate-1/2  BER=$RX_BE"
    fi
done

# ── Test 4: Max frame ────────────────────────────────────────
echo ""
echo "  Test 4: Max frame (n_syms=255, clean channel)"
echo "  --------------------------------------------------------"

for MOD in 0 1; do
    MOD_NAME=$([ "$MOD" -eq 0 ] && echo "QPSK" || echo "16QAM")
    for RATE in 0 1; do
        RATE_NAME=$([ "$RATE" -eq 0 ] && echo "1/2" || echo "2/3")
        run_tx "$MOD" "$RATE" 255
        run_rx "$MOD" "$RATE" 255
        if [ "$RX_BE" != "NA" ] && [ "$RX_BE" -eq 0 ]; then
            pass "n_syms=255  ${MOD_NAME} rate-${RATE_NAME}  BER=0"
        else
            fail "n_syms=255  ${MOD_NAME} rate-${RATE_NAME}  BER=$RX_BE"
        fi
    done
done

# ── Test 5: Near-boundary frames ─────────────────────────────
echo ""
echo "  Test 5: Near-boundary frames (clean channel, 16QAM rate-1/2)"
echo "  --------------------------------------------------------"

if [ "$QUICK" -eq 1 ]; then
    BOUNDARY_LIST="2 3 253 254"
else
    BOUNDARY_LIST="2 3 4 5 6 126 127 128 252 253 254"
fi

for NSYMS in $BOUNDARY_LIST; do
    run_tx 1 0 "$NSYMS"
    run_rx 1 0 "$NSYMS"
    if [ "$RX_BE" != "NA" ] && [ "$RX_BE" -eq 0 ]; then
        pass "n_syms=${NSYMS}  BER=0"
    else
        fail "n_syms=${NSYMS}  BER=$RX_BE"
    fi
done

# ── Summary ───────────────────────────────────────────────────
summary
