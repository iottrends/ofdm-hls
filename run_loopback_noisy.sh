#!/bin/bash
# ============================================================
# run_loopback_noisy.sh  —  OFDM TX→Noise→RX loopback test
#
# Flow:
#   Step 1: Generate input bits + Python TX reference
#   Step 2: HLS TX C-sim  (tb_input_to_tx.bin → tb_tx_output_hls.txt)
#   Step 3: Add AWGN noise (tb_tx_output_hls.txt → tb_tx_output_hls_noise.txt)
#   Step 4: HLS RX C-sim on noisy signal → tb_rx_decoded_hls.bin → BER
#   Step 5: Python reference decoder on noisy signal → BER (independent check)
#
# Usage:
#   ./run_loopback_noisy.sh                          # defaults: SNR=20 mod=1 rate=0
#   ./run_loopback_noisy.sh --snr 15                 # custom SNR
#   ./run_loopback_noisy.sh --snr 10 --mod 0         # QPSK at 10 dB
#   ./run_loopback_noisy.sh --snr 15 --mod 1 --rate 1  # 16-QAM rate-2/3
# ============================================================

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Parse arguments ───────────────────────────────────────────
SNR=20
MOD=1    # 0=QPSK 1=16QAM
RATE=0   # 0=rate-1/2 1=rate-2/3
while [[ $# -gt 0 ]]; do
    case "$1" in
        --snr)  SNR="$2";  shift 2 ;;
        --mod)  MOD="$2";  shift 2 ;;
        --rate) RATE="$2"; shift 2 ;;
        *) echo "Unknown argument: $1"; exit 1 ;;
    esac
done
MOD_NAME=$([ "$MOD" -eq 0 ] && echo "QPSK" || echo "16QAM")
RATE_NAME=$([ "$RATE" -eq 0 ] && echo "1/2" || echo "2/3")

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
echo "  OFDM TX→Noise→RX Loopback Test"
echo "  Target : Artix-50T  |  256 SC  |  ${MOD_NAME}  |  rate-${RATE_NAME}  |  255 symbols"
echo "  Channel: AWGN at SNR = ${SNR} dB"
echo "======================================================"

# ── Step 1: Generate bits + Python TX reference ───────────────
header "Step 1 — Generate input bits + Python TX reference"
log "Running: python3 sim/ofdm_reference.py --gen --mod $MOD --rate $RATE"
log "  Produces tb_input_to_tx.bin   : random bits (seed=42)"
log "  Produces tb_tx_output_ref.txt : floating-point Python TX IQ samples"
echo ""
python3 sim/ofdm_reference.py --gen --mod "$MOD" --rate "$RATE"
pass "Input bits and Python TX reference generated"

# ── Step 2: HLS TX C-sim ──────────────────────────────────────
header "Step 2 — HLS TX C-sim  (bits → IQ samples)"
log "Running: ./setup_vitis.sh csim"
log "  Reads    tb_input_to_tx.bin"
log "  Produces tb_tx_output_hls.txt : clean IQ samples from HLS IFFT"
echo ""
./setup_vitis.sh csim "$MOD" "$RATE" 2>&1 | tee /tmp/noisy_tx_csim.log | grep -E "@I|@E|error" || true
echo ""
if grep -q "Output copied to" /tmp/noisy_tx_csim.log; then
    pass "HLS TX C-sim completed — tb_tx_output_hls.txt written"
else
    fail "HLS TX C-sim did not produce tb_tx_output_hls.txt"
    exit 1
fi

# ── Step 3: Add AWGN noise ────────────────────────────────────
header "Step 3 — Add AWGN noise at SNR = ${SNR} dB"
log "Running: python3 sim/ofdm_channel_sim.py --snr ${SNR} --write-noisy"
log "  Reads    tb_tx_output_hls.txt         : clean HLS TX IQ"
log "  Adds     AWGN noise at ${SNR} dB SNR"
log "  Produces tb_tx_output_hls_noise.txt   : noisy IQ (quantised to ap_fixed<16,1>)"
echo ""
noise_result=$(python3 sim/ofdm_channel_sim.py --snr "${SNR}" --mod "$MOD" --write-noisy --input tb_tx_output_hls.txt)
echo "$noise_result" | sed 's/^/  /'
echo ""
if echo "$noise_result" | grep -q "Wrote"; then
    python_ber=$(echo "$noise_result" | grep "BER" | grep -oP '[0-9]+\.[0-9]+e[+-][0-9]+' | head -1)
    pass "Noisy signal written — Python quick-check BER = ${python_ber:-N/A}"
else
    fail "ofdm_channel_sim.py did not produce tb_tx_output_hls_noise.txt"
    exit 1
fi

# ── Step 4: HLS RX C-sim on noisy signal ─────────────────────
header "Step 4 — HLS RX C-sim on noisy signal"
log "Running: ./setup_vitis.sh rx_noisy_csim"
log "  Reads    tb_tx_output_hls_noise.txt   : noisy IQ"
log "  Produces tb_rx_decoded_hls.bin        : bytes decoded by HLS RX chain"
log "  Compares decoded bytes vs tb_input_to_tx.bin → BER"
echo ""
./setup_vitis.sh rx_noisy_csim "$MOD" "$RATE" 2>&1 | tee /tmp/noisy_rx_csim.log | grep -E "\[TB\]" || true
echo ""
bit_errors=$(grep "\[TB\] Bit  errors" /tmp/noisy_rx_csim.log | awk '{print $5}' | head -1)
total_bits=$(grep "\[TB\] Bit  errors" /tmp/noisy_rx_csim.log | awk '{print $7}' | head -1)
ber_val=$(grep    "\[TB\] BER"         /tmp/noisy_rx_csim.log | awk '{print $NF}' | head -1)
if [ -n "$bit_errors" ]; then
    if [ "$bit_errors" -eq 0 ]; then
        pass "HLS RX C-sim: BER = 0  (perfect decode despite noise)"
    else
        pass "HLS RX C-sim: ${bit_errors}/${total_bits} bit errors  BER = ${ber_val}  (expected at SNR=${SNR} dB)"
    fi
else
    fail "HLS RX C-sim: could not determine BER — check log"
fi

# ── Step 5: Python reference decoders (full chain — header + data, both precisions) ─────────
header "Step 5 — Python reference decoders  (header + data, FP64 vs Q15)"
log "Reads    tb_tx_output_hls_noise.txt   : noisy IQ"
log "Runs     decode_full() at TWO precision classes:"
log "  --decode-full       float64 throughout (idealised reference)"
log "  --decode-full-q15   Q15/Q20/Q22 (HLS-equivalent precision class)"
log "Both decode preamble + header + 255 data symbols + Viterbi + descramble"
log "and report header CRC PASS/FAIL plus data BER vs tb_input_to_tx.bin."
echo ""
echo "  ── float64 ──"
dec_fp=$(python3 sim/ofdm_reference.py --decode-full     --input tb_tx_output_hls_noise.txt --mod "$MOD" --rate "$RATE")
echo "$dec_fp"
echo ""
echo "  ── Q15 (HLS-equivalent precision) ──"
dec_q15=$(python3 sim/ofdm_reference.py --decode-full-q15 --input tb_tx_output_hls_noise.txt --mod "$MOD" --rate "$RATE")
echo "$dec_q15"
echo ""

# Overall = (header PASS) AND (data PASS).  decode_full prints the overall
# verdict on the "data: ... → PASS/FAIL" line — match that, not header-PASS.
# `|| true` swallows grep's exit-code-1 on zero matches (set -e would
# otherwise kill the script before printing the SUMMARY).
fp_pass=$(echo "$dec_fp"  | grep -cE "data: .* → PASS$" || true)
q15_pass=$(echo "$dec_q15" | grep -cE "data: .* → PASS$" || true)
if [ "$fp_pass" -gt 0 ] && [ "$q15_pass" -gt 0 ]; then
    pass "Python reference decoders: BOTH FP64 and Q15 pass at SNR = ${SNR} dB"
elif [ "$fp_pass" -gt 0 ]; then
    fail "Python decoders: FP64 pass, Q15 fail at SNR = ${SNR} dB  (precision-sensitive case)"
elif [ "$q15_pass" -gt 0 ]; then
    fail "Python decoders: Q15 pass, FP64 fail — unexpected, check setup"
else
    fail "Python decoders: both FAIL at SNR = ${SNR} dB  (algorithm cliff or HLS-side bug)"
fi

# ── Summary ───────────────────────────────────────────────────
echo ""
echo "======================================================"
echo "  NOISY LOOPBACK TEST SUMMARY  (SNR=${SNR} dB  mod=${MOD_NAME}  rate=${RATE_NAME})"
echo "======================================================"
echo "  Steps passed : $PASS / $((PASS+FAIL))"
echo "  Steps failed : $FAIL / $((PASS+FAIL))"
echo ""
echo "  Files produced:"
echo "    tb_input_to_tx.bin            — original random bits (ground truth)"
echo "    tb_tx_output_hls.txt          — HLS TX C-sim clean IQ"
echo "    tb_tx_output_hls_noise.txt    — noisy IQ at SNR = ${SNR} dB"
echo "    tb_rx_decoded_hls.bin         — HLS RX decoded bytes"
echo ""
if [ $FAIL -eq 0 ]; then
    echo "  RESULT: ALL PASS"
else
    echo "  RESULT: $FAIL STEP(S) FAILED"
fi
echo "======================================================"
echo ""

[ $FAIL -eq 0 ]
