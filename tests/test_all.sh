#!/bin/bash
# ============================================================
# test_all.sh  —  Master test runner for OFDM HLS test suite
#
# Runs L1 → L5 sequentially.  Stops on first layer failure
# unless --continue is passed.
#
# Usage:
#   ./tests/test_all.sh               # full suite (~45-90 min)
#   ./tests/test_all.sh --quick       # fast path  (~10-15 min)
#   ./tests/test_all.sh --quick --continue  # don't stop on failure
#
# Individual layers can also be run standalone:
#   ./tests/test_blocks.sh        [--quick]   # L1: per-block unit tests
#   ./tests/test_chain_matrix.sh  [--quick]   # L2+L3: TX/RX matrix
#   ./tests/test_ber_full.sh      [--quick]   # L4: BER sweep
#   ./tests/test_stress.sh        [--quick]   # L5: stress/boundary
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

QUICK_FLAG=""
CONTINUE=0
for arg in "$@"; do
    case "$arg" in
        --quick)    QUICK_FLAG="--quick" ;;
        --continue) CONTINUE=1 ;;
    esac
done

# Ensure binaries are built before any layer runs
echo ""
echo "======================================================"
echo "  OFDM HLS Test Suite"
echo "  Mode: ${QUICK_FLAG:-full}"
echo "======================================================"

source "$SCRIPT_DIR/test_common.sh"
info "Ensuring TX and RX C-sim binaries are built..."
ensure_binaries
info "Binaries ready."

LAYERS=(
    "test_blocks.sh:L1 — Per-Block Unit Tests"
    "test_chain_matrix.sh:L2+L3 — TX/RX Chain Matrix"
    "test_ber_full.sh:L4 — Full BER Sweep"
    "test_stress.sh:L5 — Stress & Boundary"
)

TOTAL_PASS=0
TOTAL_FAIL=0
declare -a RESULTS

for entry in "${LAYERS[@]}"; do
    SCRIPT="${entry%%:*}"
    NAME="${entry#*:}"

    echo ""
    echo "============================================================"
    echo "  Running: $NAME"
    echo "============================================================"

    "$SCRIPT_DIR/$SCRIPT" $QUICK_FLAG 2>&1
    EXIT=$?

    if [ $EXIT -eq 0 ]; then
        RESULTS+=("PASS  $NAME")
        TOTAL_PASS=$((TOTAL_PASS + 1))
    else
        RESULTS+=("FAIL  $NAME")
        TOTAL_FAIL=$((TOTAL_FAIL + 1))
        if [ $CONTINUE -eq 0 ]; then
            echo ""
            echo "  [STOP] $NAME FAILED — stopping. Use --continue to run all layers."
            break
        fi
    fi
done

# ── Final summary ─────────────────────────────────────────────
echo ""
echo "============================================================"
echo "  OFDM HLS Test Suite — Final Summary"
echo "============================================================"
for r in "${RESULTS[@]}"; do
    echo "  $r"
done
echo ""
echo "  Layers passed: $TOTAL_PASS / $((TOTAL_PASS + TOTAL_FAIL))"
echo "============================================================"

[ $TOTAL_FAIL -eq 0 ] && exit 0 || exit 1
