#!/bin/bash
# ============================================================
# run_ber_sweep.sh  —  Full BER sweep across channel types and SNR
#
# Strategy: compile HLS RX C-sim binary ONCE, then run the binary
# directly for each test point (avoids per-point Vitis HLS startup).
#
# Test matrix (default):
#   Modulation : 16QAM  (pass --mod 0 for QPSK, no recompile needed)
#   Channels   : awgn  phase  multipath  combined
#   SNR range  : 0, 5, 10, 15, 20, 25 dB
#   Frames     : 10 independent noise seeds per point
#
# Usage:
#   ./run_ber_sweep.sh                  # full sweep (16QAM, 10 frames)
#   ./run_ber_sweep.sh --mod 0          # QPSK sweep (no recompile)
#   ./run_ber_sweep.sh --quick          # 3 frames, key SNR/channels only
#   ./run_ber_sweep.sh --frames 5       # 5 frames per point
#   ./run_ber_sweep.sh --snr "10 15 20" # custom SNR list
#
# Output:
#   ber_results.csv  — raw per-frame results
#   (run python3 sim/plot_ber.py to generate plots)
# ============================================================

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Defaults ──────────────────────────────────────────────────
FRAMES=10
SNR_POINTS="0 5 10 15 20 25"
CHANNELS="awgn phase multipath combined"
PHASE_SIGMA=0.005
CFO_SC=0.3
MOD=1   # 0=QPSK, 1=16QAM
RATE=0  # 0=rate-1/2, 1=rate-2/3

# Build dir and LD path (matches setup_vitis.sh)
BUILD_DIR="$SCRIPT_DIR/ofdm_rx_proj/sol1/csim/build"
LD_PATH="/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fft_v9_1:\
/home/abhinavb/Xilinx/2025.2/Vitis/lnx64/tools/fpo_v7_1:\
/home/abhinavb/Xilinx/2025.2/Vitis/tps/lnx64/gcc-8.3.0/lib"

# ── Parse arguments ───────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --frames)      FRAMES="$2";        shift 2 ;;
        --snr)         SNR_POINTS="$2";    shift 2 ;;
        --channels)    CHANNELS="$2";      shift 2 ;;
        --phase-sigma) PHASE_SIGMA="$2";   shift 2 ;;
        --cfo-sc)      CFO_SC="$2";        shift 2 ;;
        --mod)         MOD="$2";           shift 2 ;;
        --rate)        RATE="$2";          shift 2 ;;
        --quick)
            FRAMES=3
            SNR_POINTS="5 10 15 20"
            CHANNELS="awgn phase combined"
            shift ;;
        *)  echo "Unknown argument: $1"; exit 1 ;;
    esac
done

# ── Helpers ───────────────────────────────────────────────────
sep()  { echo ""; echo "──────────────────────────────────────────────────────"; }
hdr()  { sep; echo "  $*"; sep; }
info() { echo "  [INFO]  $*"; }
ok()   { echo "  [OK]    $*"; }
err()  { echo "  [ERR]   $*"; }

echo ""
MOD_NAME=$([ "$MOD" -eq 0 ] && echo "QPSK" || echo "16QAM")
CSV_FILE="ber_results_mod${MOD}.csv"
echo ""
echo "======================================================"
echo "  OFDM BER Sweep — HLS Chain (C-sim)"
echo "  Modulation: $MOD_NAME (--mod $MOD)"
echo "  Channels : $CHANNELS"
echo "  SNR pts  : $SNR_POINTS dB"
echo "  Frames   : $FRAMES per point"
echo "  Output   : $CSV_FILE"
echo "======================================================"

# ── Step 1: Generate bits + HLS TX IQ ────────────────────────
hdr "Step 1 — Generate input bits and HLS TX IQ"
info "Generates tb_input_to_tx.bin and runs HLS TX C-sim → tb_tx_output_hls.txt"
echo ""
python3 sim/ofdm_reference.py --gen --mod $MOD --rate $RATE
./setup_vitis.sh csim $MOD $RATE 2>&1 | grep -E "@I|@E|error" || true
if [ ! -f tb_tx_output_hls.txt ]; then
    err "HLS TX C-sim failed — tb_tx_output_hls.txt not found"
    exit 1
fi
ok "TX IQ ready: tb_tx_output_hls.txt"

# ── Step 2: Build RX binary once ─────────────────────────────
hdr "Step 2 — Build HLS RX C-sim binary (compile once)"
info "Binary will be reused for all test points (no recompile per point)"
echo ""
./setup_vitis.sh rx_noisy_build 2>&1 | grep -E "@I|@E|error" || true
if [ ! -f "$BUILD_DIR/csim.exe" ]; then
    err "RX binary not found at $BUILD_DIR/csim.exe"
    exit 1
fi
ok "RX binary ready: $BUILD_DIR/csim.exe"

# Copy input bits to build dir once (same bits for all frames)
cp tb_input_to_tx.bin "$BUILD_DIR/tb_input_to_tx.bin"

# ── Step 3: BER sweep ─────────────────────────────────────────
hdr "Step 3 — BER sweep"

# CSV header
echo "channel,snr_db,frame,bit_errors,total_bits" > "$CSV_FILE"

TOTAL_POINTS=0
TOTAL_PASS=0

for CHANNEL in $CHANNELS; do
    echo ""
    echo "  Channel: $CHANNEL"
    printf "  %8s  %8s  %8s  %10s  %s\n" "SNR(dB)" "Errors" "Total" "BER" "Status"
    echo "  --------------------------------------------------------"

    for SNR in $SNR_POINTS; do
        POINT_ERRORS=0
        POINT_BITS=0
        FRAME_FAIL=0

        for FRAME in $(seq 1 $FRAMES); do
            # Unique seed per (channel, SNR, frame)
            SEED=$(( FRAME * 9973 + $(echo "$SNR" | tr -d '.-') * 997 + \
                     $(echo "$CHANNEL" | cksum | awk '{print $1}') % 997 ))

            # Generate noisy IQ file
            python3 sim/ofdm_channel_sim.py \
                --channel    "$CHANNEL"    \
                --snr        "$SNR"        \
                --seed       "$SEED"       \
                --phase-sigma "$PHASE_SIGMA" \
                --cfo-sc     "$CFO_SC"     \
                --write-noisy              \
                --input tb_tx_output_hls.txt > /dev/null 2>&1

            if [ ! -f tb_tx_output_hls_noise.txt ]; then
                err "channel sim failed for $CHANNEL SNR=$SNR frame=$FRAME"
                FRAME_FAIL=$((FRAME_FAIL + 1))
                continue
            fi

            # Copy noisy IQ to build dir and run binary
            cp tb_tx_output_hls_noise.txt "$BUILD_DIR/tb_tx_output_hls.txt"

            RUN_OUT=$(cd "$BUILD_DIR" && \
                LD_LIBRARY_PATH="${LD_PATH}:${LD_LIBRARY_PATH}" \
                ./csim.exe --mod $MOD --rate $RATE 2>&1) || true

            # Parse bit errors and total bits from TB output
            BE=$(echo "$RUN_OUT" | grep -i "Bit.*errors" | \
                 grep -oP '\d+\s*/\s*\d+' | head -1 | awk -F'/' '{print $1}' | tr -d ' ')
            TB_BITS=$(echo "$RUN_OUT" | grep -i "Bit.*errors" | \
                      grep -oP '\d+\s*/\s*\d+' | head -1 | awk -F'/' '{print $2}' | tr -d ' ')

            BE=${BE:-0}
            TB_BITS=${TB_BITS:-0}

            POINT_ERRORS=$((POINT_ERRORS + BE))
            POINT_BITS=$((POINT_BITS + TB_BITS))

            # Write per-frame row to CSV
            echo "$CHANNEL,$SNR,$FRAME,$BE,$TB_BITS" >> "$CSV_FILE"
        done

        # Compute and print aggregate for this (channel, SNR) point
        TOTAL_POINTS=$((TOTAL_POINTS + 1))
        if [ "$POINT_BITS" -gt 0 ]; then
            BER=$(python3 -c "print(f'{$POINT_ERRORS/$POINT_BITS:.3e}')")

            # Pass criteria: BER=0 at high SNR, or reasonable BER at low SNR
            STATUS=""
            if [ "$POINT_ERRORS" -eq 0 ] && [ "$SNR" -ge 20 ]; then
                STATUS="PASS"
                TOTAL_PASS=$((TOTAL_PASS + 1))
            elif [ "$SNR" -lt 10 ]; then
                STATUS="--"   # low SNR: errors expected
            fi

            printf "  %8s  %8d  %8d  %10s  %s\n" \
                "$SNR" "$POINT_ERRORS" "$POINT_BITS" "$BER" "$STATUS"
        else
            printf "  %8s  %8s  %8s  %10s  %s\n" \
                "$SNR" "N/A" "N/A" "N/A" "FAIL (no output)"
        fi
    done
done

# ── Summary ───────────────────────────────────────────────────
echo ""
echo "======================================================"
echo "  BER SWEEP COMPLETE"
echo "======================================================"
echo "  Results  : $CSV_FILE"
echo "  Plot     : python3 sim/plot_ber.py $CSV_FILE  →  ${CSV_FILE%.csv}.png"
echo ""
echo "  High-SNR pass (BER=0 at ≥20 dB): $TOTAL_PASS / $TOTAL_POINTS points"
echo "======================================================"
echo ""
