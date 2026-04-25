#!/bin/bash
# ============================================================
# run_loopback.sh  —  Full OFDM TX→RX loopback test
#
# Flow:
#   Step 1: Python generates random bits + floating-point TX reference
#   Step 2: HLS TX C-sim encodes bits → IQ samples (tb_tx_output_hls.txt)
#   Step 3: Compare HLS TX IQ vs Python TX reference → EVM check
#   Step 4: HLS RX C-sim decodes IQ → bytes, compares vs original bits → BER
#   Step 5: Python reference decoder decodes HLS TX IQ → BER (independent check)
#
# Usage:
#   ./run_loopback.sh                    # defaults: 16-QAM, rate-1/2
#   ./run_loopback.sh --mod 0            # QPSK rate-1/2
#   ./run_loopback.sh --mod 0 --rate 1   # QPSK rate-2/3
#   ./run_loopback.sh --mod 1 --rate 1   # 16-QAM rate-2/3
# ============================================================

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Args ──────────────────────────────────────────────────────
MOD=1     # 0=QPSK  1=16QAM
RATE=0    # 0=rate-1/2  1=rate-2/3
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mod)  MOD="$2";  shift 2 ;;
        --rate) RATE="$2"; shift 2 ;;
        -h|--help)
            grep -E "^# " "$0" | sed 's/^# //'
            exit 0
            ;;
        *) echo "[ERR] unknown arg: $1"; exit 1 ;;
    esac
done
MOD_NAME=$([[ $MOD  -eq 0 ]] && echo "QPSK"  || echo "16-QAM")
RATE_NAME=$([[ $RATE -eq 0 ]] && echo "1/2"  || echo "2/3")

PASS=0
FAIL=0

# ── Helpers ───────────────────────────────────────────────────
separator() { echo ""; echo "──────────────────────────────────────────────────────"; }
header()    { separator; echo "  $*"; separator; }
log()       { echo "  [INFO]  $*"; }
pass()      { echo "  [PASS]  $*"; PASS=$((PASS+1)); }
fail()      { echo "  [FAIL]  $*"; FAIL=$((FAIL+1)); }

echo ""
echo "======================================================"
echo "  OFDM TX→RX Full Loopback Test"
echo "  Target: Artix-50T  |  256 SC  |  ${MOD_NAME}  |  rate-${RATE_NAME}  |  255 symbols"
echo "======================================================"

# ── Step 1: Generate bits + Python TX reference ───────────────
header "Step 1 — Generate input bits + Python TX reference"
log "Running: python3 sim/ofdm_reference.py --gen --mod $MOD --rate $RATE"
log "  Produces tb_input_to_tx.bin  : random bits (seed=42)"
log "  Produces tb_tx_output_ref.txt: floating-point Python TX IQ samples"
echo ""
python3 sim/ofdm_reference.py --gen --mod "$MOD" --rate "$RATE"
pass "Input bits and Python TX reference generated"

# ── Step 2: HLS TX C-sim ──────────────────────────────────────
header "Step 2 — HLS TX C-sim  (bits → IQ samples)"
log "Running: ./setup_vitis.sh csim $MOD $RATE"
log "  Reads   tb_input_to_tx.bin"
log "  Produces tb_tx_output_hls.txt: IQ samples from HLS IFFT"
echo ""
./setup_vitis.sh csim "$MOD" "$RATE" 2>&1 | tee /tmp/loopback_tx_csim.log | grep -E "@I|@E|error" || true
echo ""
if grep -q "Output copied to" /tmp/loopback_tx_csim.log; then
    pass "HLS TX C-sim completed — tb_tx_output_hls.txt written"
else
    fail "HLS TX C-sim did not produce tb_tx_output_hls.txt"
    exit 1
fi

# ── Step 3: TX EVM check ──────────────────────────────────────
header "Step 3 — TX waveform quality check  (HLS IQ vs Python reference)"
log "Running: python3 sim/ofdm_reference.py --compare"
log "  Compares tb_tx_output_hls.txt vs tb_tx_output_ref.txt sample-by-sample"
log "  Reports EVM% — measures fixed-point quantisation error of the TX"
echo ""
evm_result=$(python3 sim/ofdm_reference.py --compare --mod "$MOD" --rate "$RATE")
echo "$evm_result" | sed 's/^/  /'
echo ""
if echo "$evm_result" | grep -q "PASS"; then
    evm_val=$(echo "$evm_result" | grep "EVM " | grep -oP '[0-9]+\.[0-9]+(?=%)' | head -1)
    pass "TX EVM check passed  (EVM = ${evm_val}%)"
else
    fail "TX EVM check failed — HLS TX waveform deviates too much from reference"
fi

# ── Step 4: HLS RX C-sim ──────────────────────────────────────
header "Step 4 — HLS RX C-sim  (IQ samples → decoded bytes)"
log "Running: ./setup_vitis.sh rx_csim $MOD $RATE"
log "  Reads   tb_tx_output_hls.txt  : IQ from HLS TX"
log "  Produces tb_rx_decoded_hls.bin: bytes decoded by HLS RX chain"
log "  Internally compares decoded bytes vs tb_input_to_tx.bin → BER"
echo ""
./setup_vitis.sh rx_csim "$MOD" "$RATE" 2>&1 | tee /tmp/loopback_rx_csim.log | grep -E "\[TB\]" || true
echo ""
if grep -q "PASS" /tmp/loopback_rx_csim.log && grep -q "BER         : 0" /tmp/loopback_rx_csim.log; then
    pass "HLS RX C-sim: decoded bytes match original bits exactly  (BER = 0)"
else
    ber=$(grep "BER" /tmp/loopback_rx_csim.log | tail -1)
    fail "HLS RX C-sim: decode errors detected  ($ber)"
fi

# ── Step 5: Python reference decoder on HLS TX output ─────────
header "Step 5 — Python reference decoder BER check  (independent verification)"
log "Running: python3 sim/ofdm_reference.py --decode-hls"
log "  Reads   tb_tx_output_hls.txt  : IQ from HLS TX"
log "  Decodes using Python numpy FFT (float, no HLS RX involved)"
log "  Compares decoded bytes vs tb_input_to_tx.bin → BER"
log "  Purpose: confirms HLS TX output is decodable independently of HLS RX"
echo ""
dec_result=$(python3 sim/ofdm_reference.py --decode-hls --mod "$MOD" --rate "$RATE")
echo "$dec_result" | sed 's/^/  /'
echo ""
if echo "$dec_result" | grep -q "PASS"; then
    pass "Python reference decoder: BER = 0  (HLS TX output is decodable)"
else
    fail "Python reference decoder: errors found in HLS TX output"
fi

# ── Summary ───────────────────────────────────────────────────
echo ""
echo "======================================================"
echo "  LOOPBACK TEST SUMMARY  (${MOD_NAME}  rate-${RATE_NAME})"
echo "======================================================"
echo "  Steps passed : $PASS / $((PASS+FAIL))"
echo "  Steps failed : $FAIL / $((PASS+FAIL))"
echo ""
echo "  Files produced:"
echo "    tb_input_to_tx.bin      — original random bits (ground truth)"
echo "    tb_tx_output_ref.txt    — Python TX floating-point IQ reference"
echo "    tb_tx_output_hls.txt    — HLS TX C-sim IQ output"
echo "    tb_rx_decoded_hls.bin   — HLS RX decoded bytes"
echo ""
if [ $FAIL -eq 0 ]; then
    echo "  RESULT: ALL PASS"
else
    echo "  RESULT: $FAIL STEP(S) FAILED"
fi
echo "======================================================"
echo ""

[ $FAIL -eq 0 ]
