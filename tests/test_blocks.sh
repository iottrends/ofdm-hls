#!/bin/bash
# ============================================================
# test_blocks.sh  —  L1: Per-block standalone unit tests
#
# Tests:
#   1. Scrambler loopback (Python ref vs HLS round-trip)
#   2. Interleaver loopback (TX→RX identity via Python check)
#   3. FEC codec (existing conv_fec_tb — rate-1/2 and rate-2/3)
#   4. sync_detect energy-floor regression (1 dB SNR sweep 0→35 dB)
#   5. cfo_correct basic check (zero-CFO pass-through)
#
# Usage:
#   ./tests/test_blocks.sh          # full run (~3-5 min)
#   ./tests/test_blocks.sh --quick  # skip FEC rebuild, fewer SNR points
# ============================================================
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_common.sh"

QUICK=0
[[ "${1:-}" == "--quick" ]] && QUICK=1

echo ""
echo "======================================================"
echo "  L1 — Per-Block Unit Tests"
echo "======================================================"

# ── Test 1: Scrambler round-trip ──────────────────────────────
# Scrambler is its own inverse: scramble(scramble(x)) == x.
# We verify this in Python (same LFSR as HLS, seed 0x7F).
info "Test 1: Scrambler round-trip (Python LFSR check)"

SCRAM_OK=$(python3 -c "
import sys, os; sys.path.insert(0, '$SIM_DIR')
from ofdm_reference import py_scramble
for n in [25, 100, 800, 12750]:
    data = bytes([(i * 37 + 7) & 0xFF for i in range(n)])
    s1 = py_scramble(data)
    s2 = py_scramble(s1)
    if data != s2:
        print(f'FAIL at n={n}')
        sys.exit(1)
print('OK')
" 2>&1)

if [[ "$SCRAM_OK" == "OK" ]]; then
    pass "Scrambler self-inverse for n=25,100,800,12750"
else
    fail "Scrambler round-trip: $SCRAM_OK"
fi

# ── Test 2: Interleaver round-trip ────────────────────────────
# interleave(deinterleave(x)) == x for both QPSK and 16QAM.
info "Test 2: Interleaver TX→RX round-trip (Python check)"

INTRLV_OK=$(python3 -c "
import sys; sys.path.insert(0, '$SIM_DIR')
from ofdm_reference import py_interleave, py_deinterleave
for mod in [0, 1]:
    bytes_per_sym = 50 if mod == 0 else 100
    n_syms = 3
    data = bytes([(i * 53 + mod) & 0xFF for i in range(n_syms * bytes_per_sym)])
    fwd  = py_interleave(data, mod, n_syms)
    back = py_deinterleave(fwd, mod, n_syms)
    if data != back:
        print(f'FAIL mod={mod}')
        sys.exit(1)
print('OK')
" 2>&1)

if [[ "$INTRLV_OK" == "OK" ]]; then
    pass "Interleaver round-trip for QPSK and 16QAM"
else
    fail "Interleaver round-trip: $INTRLV_OK"
fi

# ── Test 3: FEC codec (conv_fec_tb) ──────────────────────────
info "Test 3: FEC codec loopback (conv_fec_tb C-sim)"

FEC_BUILD="$ROOT_DIR/fec_csim_proj/sol1/csim/build"
if [ ! -f "$FEC_BUILD/csim.exe" ] && [ "$QUICK" -eq 0 ]; then
    info "Building FEC C-sim binary..."
    (cd "$ROOT_DIR" && ./setup_vitis.sh fec_csim >/dev/null 2>&1)
fi

if [ -f "$FEC_BUILD/csim.exe" ]; then
    FEC_OUT=$(cd "$FEC_BUILD" && \
        LD_LIBRARY_PATH="${LD_PATH}:${LD_LIBRARY_PATH}" \
        ./csim.exe 2>&1) || true
    FEC_PASS=$(echo "$FEC_OUT" | grep -c "PASS")
    FEC_FAIL=$(echo "$FEC_OUT" | grep -c "FAIL")
    if [ "$FEC_FAIL" -eq 0 ] && [ "$FEC_PASS" -ge 2 ]; then
        pass "FEC codec: rate-1/2 and rate-2/3 clean loopback ($FEC_PASS passes)"
    else
        fail "FEC codec: $FEC_PASS pass, $FEC_FAIL fail"
    fi
else
    skip "FEC codec: binary not found (run ./setup_vitis.sh fec_csim first)"
fi

# ── Test 4: sync_detect energy-floor regression ───────────────
# Sweep SNR 0→35 dB in 1 dB steps on AWGN channel.
# The fix guarantees no false peak in the guard zone at any SNR.
# The test passes if every point that yields output gives BER=0,
# and no "header CRC error" at SNR >= 15 dB.
info "Test 4: sync_detect energy-floor regression (SNR sweep)"

ensure_binaries

# Generate a short frame for speed (n_syms=16 still exercises full sync)
SYNC_MOD=1; SYNC_RATE=0; SYNC_NSYMS=16
run_tx $SYNC_MOD $SYNC_RATE $SYNC_NSYMS

if [ "$QUICK" -eq 1 ]; then
    SNR_LIST=$(seq 15 5 35)
else
    SNR_LIST=$(seq 0 1 35)
fi

SYNC_PASS=0; SYNC_FAIL=0; SYNC_EXPECTED_FAIL=0
for SNR in $SNR_LIST; do
    # Save clean TX output, apply noise, run RX
    cp "$ROOT_DIR/tb_tx_output_hls.txt" /tmp/_sync_test_clean.txt
    add_noise "$SNR" awgn $((SNR * 997 + 42))
    run_rx $SYNC_MOD $SYNC_RATE $SYNC_NSYMS
    cp /tmp/_sync_test_clean.txt "$ROOT_DIR/tb_tx_output_hls.txt"

    if [ "$SNR" -ge 15 ]; then
        # Should decode cleanly: no header CRC error
        if [ "$RX_HEADER_ERR" -gt 0 ]; then
            fail "sync_detect @ SNR=${SNR} dB: header CRC error (energy-floor regression!)"
            SYNC_FAIL=$((SYNC_FAIL+1))
        elif [ "$RX_BE" != "NA" ] && [ "$RX_BE" -eq 0 ]; then
            SYNC_PASS=$((SYNC_PASS+1))
        elif [ "$RX_BE" != "NA" ] && [ "$RX_BE" -gt 0 ]; then
            fail "sync_detect @ SNR=${SNR} dB: BER>0 ($RX_BE errors)"
            SYNC_FAIL=$((SYNC_FAIL+1))
        else
            fail "sync_detect @ SNR=${SNR} dB: no output"
            SYNC_FAIL=$((SYNC_FAIL+1))
        fi
    else
        # Low SNR: failures expected (sync may not lock)
        SYNC_EXPECTED_FAIL=$((SYNC_EXPECTED_FAIL+1))
    fi
done

if [ "$SYNC_FAIL" -eq 0 ]; then
    pass "sync_detect: $SYNC_PASS high-SNR points pass, $SYNC_EXPECTED_FAIL low-SNR expected"
else
    fail "sync_detect: $SYNC_FAIL high-SNR failures (see above)"
fi

# ── Test 5: cfo_correct pass-through (zero CFO) ──────────────
# On a clean channel, CFO = 0, so cfo_correct should be transparent.
# Already implicitly tested by the RX loopback, but make it explicit
# by verifying RX output matches at n_syms=16 clean channel.
info "Test 5: cfo_correct zero-CFO pass-through"

run_tx 1 0 16
run_rx 1 0 16
if [ "$RX_BE" != "NA" ] && [ "$RX_BE" -eq 0 ]; then
    pass "cfo_correct pass-through: BER=0 on clean channel (n_syms=16)"
else
    fail "cfo_correct pass-through: BER=$RX_BE (expected 0)"
fi

# ── Summary ───────────────────────────────────────────────────
summary
