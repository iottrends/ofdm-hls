#!/bin/bash
# ============================================================
# tests/run_regression.sh  —  Full OFDM TX → channel → RX regression.
#
# For each (modcod × channel × SNR × frame) point:
#   1. Run HLS TX C-sim once per modcod to produce clean IQ.
#   2. Apply channel impairment (channel sim writes the noisy IQ).
#   3. Decode through the requested Python RX paths (FP64 hard + Q15 hard
#      by default; --soft adds the SOFT Viterbi variants; --hls-rx adds
#      the HLS rx_noisy_csim path).
#   4. Record header CRC + data BER per decoder, write a row to the CSV,
#      print a live row to the terminal.
#
# Defaults:
#   modcods   : QPSK 1/2, QPSK 2/3, 16QAM 1/2, 16QAM 2/3
#   channels  : awgn multipath phase cfo combined   (5 channels — CFO included)
#   snr       : 15 dB (single point)
#   decoders  : fp64-hard q15-hard
#   frames    : 1
#
# Usage:
#   ./tests/run_regression.sh                               # defaults
#   ./tests/run_regression.sh --snr 10                      # one SNR
#   ./tests/run_regression.sh --snr "5 10 15 20"            # SNR list
#   ./tests/run_regression.sh --channels "awgn multipath"   # subset
#   ./tests/run_regression.sh --modcods "0:0 1:1"           # subset
#   ./tests/run_regression.sh --frames 5                    # average over 5 frames
#   ./tests/run_regression.sh --soft                        # also run soft decoders
#   ./tests/run_regression.sh --hls-rx                      # also run HLS rx_noisy_csim
#
# Outputs:
#   regression_results.csv         (per-frame raw rows)
#   regression_summary.csv         (mean BER per modcod×channel×SNR)
#   /tmp/regression.log            (full terminal output)
# ============================================================

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$REPO_DIR"

# Use the repo venv if it exists (has reedsolo + numpy); otherwise fall back.
if [ -x "$REPO_DIR/.venv/bin/python" ]; then
    PYTHON="$REPO_DIR/.venv/bin/python"
else
    PYTHON="python3"
fi

# ── Defaults ────────────────────────────────────────────────
MODCODS="0:0 0:1 1:0 1:1"
CHANNELS="awgn multipath phase cfo combined"
SNRS="15"
FRAMES=1
ENABLE_SOFT=0
ENABLE_HLS=0
ENABLE_SMOOTH=0
SMOOTH_TAPS=64
PHASE_SIGMA=0.005
CFO_SC=0.3

# ── Parse args ──────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --snr)         SNRS="$2";      shift 2 ;;
        --channels)    CHANNELS="$2";  shift 2 ;;
        --modcods)     MODCODS="$2";   shift 2 ;;
        --frames)      FRAMES="$2";    shift 2 ;;
        --phase-sigma) PHASE_SIGMA="$2"; shift 2 ;;
        --cfo-sc)      CFO_SC="$2";    shift 2 ;;
        --soft)        ENABLE_SOFT=1;  shift ;;
        --hls-rx)      ENABLE_HLS=1;   shift ;;
        --smooth)      ENABLE_SMOOTH=1; shift ;;
        --smooth-taps) SMOOTH_TAPS="$2"; shift 2 ;;
        -h|--help)
            grep -E "^# " "$0" | sed 's/^# //'
            exit 0
            ;;
        *) echo "[ERR] unknown arg: $1"; exit 1 ;;
    esac
done

# ── Helpers ─────────────────────────────────────────────────
mod_name()  { [ "$1" -eq 0 ] && echo "QPSK"  || echo "16QAM"; }
rate_name() { [ "$1" -eq 0 ] && echo "1/2"   || echo "2/3";   }
raw_bytes() {
    local M="$1" R="$2" coded
    if [ "$M" -eq 0 ]; then coded=12750; else coded=25500; fi
    if [ "$R" -eq 0 ]; then echo $((coded / 2)); else echo $((coded * 2 / 3)); fi
}

extract_be()  { echo "$1" | grep -oP 'bit_err=\K[0-9]+'        | head -1; }
extract_tb()  { echo "$1" | grep -oP 'bit_err=[0-9]+/\K[0-9]+' | head -1; }
extract_ber() { echo "$1" | grep -oP 'BER=\K[0-9.eE+-]+'       | head -1; }
extract_hdr() { echo "$1" | grep -oP 'CRC 0x[0-9A-F]+/0x[0-9A-F]+\s+\K(PASS|FAIL)' | head -1; }

# Banner
echo ""
echo "════════════════════════════════════════════════════════════════════════════════"
echo "  OFDM Regression — TX → channel → RX"
echo "  Modcods   : $MODCODS  (mod:rate)"
echo "  Channels  : $CHANNELS"
echo "  SNRs      : $SNRS dB"
echo "  Frames    : $FRAMES per (modcod, channel, SNR)"
SMOOTH_TAG=""; [ "$ENABLE_SMOOTH" -eq 1 ] && SMOOTH_TAG="-smooth"
echo "  Decoders  : FP64-hard${SMOOTH_TAG}, Q15-hard${SMOOTH_TAG}$([ "$ENABLE_SOFT" -eq 1 ] && echo ", FP64-soft${SMOOTH_TAG}, Q15-soft${SMOOTH_TAG}")$([ "$ENABLE_HLS" -eq 1 ] && echo ", HLS-rx_noisy_csim")"
[ "$ENABLE_SMOOTH" -eq 1 ] && echo "  Smoothing : enabled (L_taps=$SMOOTH_TAPS time-domain channel taps kept)"
echo "  Channel knobs: phase σ = $PHASE_SIGMA rad/sample,  CFO = $CFO_SC SC"
echo "════════════════════════════════════════════════════════════════════════════════"

# ── CSV header: variable columns depending on --soft / --hls-rx ──
RAW_CSV=regression_results.csv
SUM_CSV=regression_summary.csv

CSV_COLS="modcod,channel,snr_db,frame,seed,fp64h_hdr,fp64h_be,fp64h_bits,fp64h_ber,q15h_hdr,q15h_be,q15h_bits,q15h_ber"
[ "$ENABLE_SOFT" -eq 1 ] && CSV_COLS="$CSV_COLS,fp64s_hdr,fp64s_be,fp64s_ber,q15s_hdr,q15s_be,q15s_ber"
[ "$ENABLE_HLS"  -eq 1 ] && CSV_COLS="$CSV_COLS,hls_be,hls_bits,hls_ber"
echo "$CSV_COLS" > "$RAW_CSV"

# ── Main loops ──────────────────────────────────────────────
for MC in $MODCODS; do
    M="${MC%:*}"; R="${MC#*:}"
    LABEL="$(mod_name "$M")-$(rate_name "$R")"
    RAW=$(raw_bytes "$M" "$R")

    echo ""
    echo "  ─────────────────────────────────────────────────────────────────────────"
    echo "  MODCOD: $LABEL  (mod=$M rate=$R)   raw payload = $RAW bytes/frame"
    echo "  ─────────────────────────────────────────────────────────────────────────"

    # Step A: gen bits + TX — ONCE per modcod.
    # Default flow: Python TX (--gen produces tb_tx_output_ref.txt with all the
    # Path-A defaults — header FEC, etc).  When --hls-rx is set, also run the
    # HLS TX C-sim — but the HLS chain doesn't have header FEC yet, so the
    # whole sweep falls back to NO_HEADER_FEC mode for that run.
    NO_HDR_FEC_FLAG=""
    NO_RS_FLAG=""
    if [ "$ENABLE_HLS" -eq 1 ]; then
        # HLS-RX comparison: HLS TX has neither header FEC nor RS, so disable
        # both on the Python side to keep the comparison fair.
        "$PYTHON" sim/ofdm_reference.py --gen --mod "$M" --rate "$R" --no-header-fec --no-rs > /dev/null 2>&1
        ./setup_vitis.sh csim "$M" "$R" > /tmp/regression_tx_${M}${R}.log 2>&1
        if [[ ! -f tb_tx_output_hls.txt ]]; then
            echo "  [ERR] HLS TX C-sim failed for $LABEL — see /tmp/regression_tx_${M}${R}.log"
            continue
        fi
        TX_INPUT_FILE=tb_tx_output_hls.txt
        NO_HDR_FEC_FLAG="--no-header-fec"
        NO_RS_FLAG="--no-rs"
    else
        # Python-only flow: generate the FEC-coded reference once.
        "$PYTHON" sim/ofdm_reference.py --gen --mod "$M" --rate "$R" > /dev/null 2>&1
        if [[ ! -f tb_tx_output_ref.txt ]]; then
            echo "  [ERR] Python TX --gen failed for $LABEL"
            continue
        fi
        TX_INPUT_FILE=tb_tx_output_ref.txt
    fi

    # Header row of the per-modcod table (column count varies)
    # Each Python decoder shows two columns: header CRC (PASS/FAIL) and data BER.
    if [ "$ENABLE_SOFT" -eq 1 ] && [ "$ENABLE_HLS" -eq 1 ]; then
        printf "  %-9s  %5s  %-7s  %5s %12s  %5s %12s  %5s %12s  %5s %12s  %12s\n" \
               "channel" "SNR" "frame" "FP-h" "FP64-hard" "Q15-h" "Q15-hard" "FP-h" "FP64-soft" "Q15-h" "Q15-soft" "HLS-rx"
    elif [ "$ENABLE_SOFT" -eq 1 ]; then
        printf "  %-9s  %5s  %-7s  %5s %12s  %5s %12s  %5s %12s  %5s %12s\n" \
               "channel" "SNR" "frame" "FP-h" "FP64-hard" "Q15-h" "Q15-hard" "FP-h" "FP64-soft" "Q15-h" "Q15-soft"
    elif [ "$ENABLE_HLS" -eq 1 ]; then
        printf "  %-9s  %5s  %-7s  %5s %12s  %5s %12s  %12s\n" \
               "channel" "SNR" "frame" "FP-h" "FP64-hard" "Q15-h" "Q15-hard" "HLS-rx"
    else
        printf "  %-9s  %5s  %-7s  %5s %12s  %5s %12s\n" \
               "channel" "SNR" "frame" "FP-h" "FP64-hard" "Q15-h" "Q15-hard"
    fi
    echo "  ─────────────────────────────────────────────────────────────────────────"

    for CH in $CHANNELS; do
        for SNR in $SNRS; do
            for FRAME in $(seq 1 "$FRAMES"); do
                SEED=$(( SNR * 10007 + M * 1009 + R * 103 + FRAME * 17 \
                         + $(echo "$CH" | cksum | awk '{print $1}') % 991 ))

                # ── Apply channel ─────────────────────────────────────
                "$PYTHON" sim/ofdm_channel_sim.py \
                    --channel "$CH" --snr "$SNR" --seed "$SEED" --mod "$M" \
                    --phase-sigma "$PHASE_SIGMA" --cfo-sc "$CFO_SC" \
                    --write-noisy --input "$TX_INPUT_FILE" > /dev/null 2>&1
                if [[ ! -f tb_tx_output_hls_noise.txt ]]; then
                    echo "  [ERR] channel sim failed: $CH @ SNR=$SNR seed=$SEED"
                    continue
                fi

                # ── FP64 + sync + HARD (+ smoothing if --smooth) ─────
                FP64H_FLAG=$([ "$ENABLE_SMOOTH" -eq 1 ] && echo "--decode-full-smooth --smooth-taps $SMOOTH_TAPS" || echo "--decode-full-sync")
                FP64H_OUT=$("$PYTHON" sim/ofdm_reference.py $FP64H_FLAG $NO_HDR_FEC_FLAG $NO_RS_FLAG \
                    --input tb_tx_output_hls_noise.txt --mod "$M" --rate "$R" 2>&1)
                FP64H_HDR=$(extract_hdr "$(echo "$FP64H_OUT" | grep 'phase_err')")
                FP64H_LINE=$(echo "$FP64H_OUT" | grep "data: byte" | head -1)
                FP64H_BE=$(extract_be "$FP64H_LINE");  FP64H_TB=$(extract_tb "$FP64H_LINE")
                FP64H_BER=$(extract_ber "$FP64H_LINE")

                # ── Q15  + sync + HARD (+ smoothing if --smooth) ─────
                Q15H_FLAG=$([ "$ENABLE_SMOOTH" -eq 1 ] && echo "--decode-full-q15-smooth --smooth-taps $SMOOTH_TAPS" || echo "--decode-full-q15-sync")
                Q15H_OUT=$("$PYTHON" sim/ofdm_reference.py $Q15H_FLAG $NO_HDR_FEC_FLAG $NO_RS_FLAG \
                    --input tb_tx_output_hls_noise.txt --mod "$M" --rate "$R" 2>&1)
                Q15H_HDR=$(extract_hdr "$(echo "$Q15H_OUT" | grep 'phase_err')")
                Q15H_LINE=$(echo "$Q15H_OUT" | grep "data: byte" | head -1)
                Q15H_BE=$(extract_be "$Q15H_LINE");   Q15H_TB=$(extract_tb "$Q15H_LINE")
                Q15H_BER=$(extract_ber "$Q15H_LINE")

                # ── Optional FP64 SOFT ────────────────────────────────
                FP64S_HDR=""; FP64S_BE=""; FP64S_BER=""
                Q15S_HDR="";  Q15S_BE="";  Q15S_BER=""
                if [ "$ENABLE_SOFT" -eq 1 ]; then
                    FP64S_FLAG=$([ "$ENABLE_SMOOTH" -eq 1 ] && echo "--decode-full-soft-smooth --smooth-taps $SMOOTH_TAPS" || echo "--decode-full-soft")
                    FP64S_OUT=$("$PYTHON" sim/ofdm_reference.py $FP64S_FLAG $NO_HDR_FEC_FLAG $NO_RS_FLAG \
                        --input tb_tx_output_hls_noise.txt --mod "$M" --rate "$R" 2>&1)
                    FP64S_HDR=$(extract_hdr "$(echo "$FP64S_OUT" | grep 'phase_err')")
                    FP64S_LINE=$(echo "$FP64S_OUT" | grep "data: byte" | head -1)
                    FP64S_BE=$(extract_be "$FP64S_LINE")
                    FP64S_BER=$(extract_ber "$FP64S_LINE")

                    Q15S_FLAG=$([ "$ENABLE_SMOOTH" -eq 1 ] && echo "--decode-full-q15-soft-smooth --smooth-taps $SMOOTH_TAPS" || echo "--decode-full-q15-soft")
                    Q15S_OUT=$("$PYTHON" sim/ofdm_reference.py $Q15S_FLAG $NO_HDR_FEC_FLAG $NO_RS_FLAG \
                        --input tb_tx_output_hls_noise.txt --mod "$M" --rate "$R" 2>&1)
                    Q15S_HDR=$(extract_hdr "$(echo "$Q15S_OUT" | grep 'phase_err')")
                    Q15S_LINE=$(echo "$Q15S_OUT" | grep "data: byte" | head -1)
                    Q15S_BE=$(extract_be "$Q15S_LINE")
                    Q15S_BER=$(extract_ber "$Q15S_LINE")
                fi

                # ── Optional HLS RX C-sim ─────────────────────────────
                HLS_BE=""; HLS_TB=""; HLS_BER=""
                if [ "$ENABLE_HLS" -eq 1 ]; then
                    ./setup_vitis.sh rx_noisy_csim "$M" "$R" > /dev/null 2>&1 || true
                    HLS_BE=$(grep '\[TB\] Bit  errors' vitis_rx_noisy_csim.log | head -1 | awk '{print $5}')
                    HLS_TB=$(grep '\[TB\] Bit  errors' vitis_rx_noisy_csim.log | head -1 | awk '{print $7}')
                    if [[ -n "$HLS_BE" && -n "$HLS_TB" && "$HLS_TB" -gt 0 ]]; then
                        HLS_BER=$("$PYTHON" -c "print(f'{$HLS_BE/$HLS_TB:.3e}')")
                    fi
                fi

                # ── Print row + write CSV ─────────────────────────────
                # Header columns first (PASS/FAIL/?) then data BER per decoder.
                if [ "$ENABLE_SOFT" -eq 1 ] && [ "$ENABLE_HLS" -eq 1 ]; then
                    printf "  %-9s  %5s  %-7s  %5s %12s  %5s %12s  %5s %12s  %5s %12s  %12s\n" \
                        "$CH" "$SNR" "$FRAME" \
                        "${FP64H_HDR:-?}" "${FP64H_BER:-fail}" \
                        "${Q15H_HDR:-?}"  "${Q15H_BER:-fail}" \
                        "${FP64S_HDR:-?}" "${FP64S_BER:-fail}" \
                        "${Q15S_HDR:-?}"  "${Q15S_BER:-fail}" \
                        "${HLS_BER:-fail}"
                elif [ "$ENABLE_SOFT" -eq 1 ]; then
                    printf "  %-9s  %5s  %-7s  %5s %12s  %5s %12s  %5s %12s  %5s %12s\n" \
                        "$CH" "$SNR" "$FRAME" \
                        "${FP64H_HDR:-?}" "${FP64H_BER:-fail}" \
                        "${Q15H_HDR:-?}"  "${Q15H_BER:-fail}" \
                        "${FP64S_HDR:-?}" "${FP64S_BER:-fail}" \
                        "${Q15S_HDR:-?}"  "${Q15S_BER:-fail}"
                elif [ "$ENABLE_HLS" -eq 1 ]; then
                    printf "  %-9s  %5s  %-7s  %5s %12s  %5s %12s  %12s\n" \
                        "$CH" "$SNR" "$FRAME" \
                        "${FP64H_HDR:-?}" "${FP64H_BER:-fail}" \
                        "${Q15H_HDR:-?}"  "${Q15H_BER:-fail}" \
                        "${HLS_BER:-fail}"
                else
                    printf "  %-9s  %5s  %-7s  %5s %12s  %5s %12s\n" \
                        "$CH" "$SNR" "$FRAME" \
                        "${FP64H_HDR:-?}" "${FP64H_BER:-fail}" \
                        "${Q15H_HDR:-?}"  "${Q15H_BER:-fail}"
                fi

                ROW="$LABEL,$CH,$SNR,$FRAME,$SEED,$FP64H_HDR,$FP64H_BE,$FP64H_TB,$FP64H_BER,$Q15H_HDR,$Q15H_BE,$Q15H_TB,$Q15H_BER"
                [ "$ENABLE_SOFT" -eq 1 ] && ROW="$ROW,$FP64S_HDR,$FP64S_BE,$FP64S_BER,$Q15S_HDR,$Q15S_BE,$Q15S_BER"
                [ "$ENABLE_HLS"  -eq 1 ] && ROW="$ROW,$HLS_BE,$HLS_TB,$HLS_BER"
                echo "$ROW" >> "$RAW_CSV"
            done
        done
    done
done

# ── Aggregate: mean BER per (modcod, channel, SNR) across frames ─
"$PYTHON" - "$RAW_CSV" "$SUM_CSV" "$ENABLE_SOFT" "$ENABLE_HLS" <<'PYEOF'
import csv, sys, statistics
raw_path, sum_path, en_soft, en_hls = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
rows = list(csv.DictReader(open(raw_path)))
groups = {}
for r in rows:
    k = (r['modcod'], r['channel'], int(r['snr_db']))
    groups.setdefault(k, []).append(r)

cols = ['modcod','channel','snr_db','frames',
        'fp64h_mean_ber','fp64h_std_ber','q15h_mean_ber','q15h_std_ber']
if en_soft: cols += ['fp64s_mean_ber','fp64s_std_ber','q15s_mean_ber','q15s_std_ber']
if en_hls:  cols += ['hls_mean_ber','hls_std_ber']

def col_stats(rows, beK, bitsK=None):
    # Compute BERs from be / bits when possible (for HLS); use ber field otherwise.
    bers=[]
    for r in rows:
        if bitsK and r.get(bitsK) and int(r[bitsK]) > 0:
            bers.append(int(r[beK]) / int(r[bitsK]))
        elif r.get(beK.replace('_be','_ber') if '_be' in beK else beK):
            try:
                v = float(r.get(beK.replace('_be','_ber') if '_be' in beK else beK,''))
                bers.append(v)
            except (ValueError, TypeError):
                pass
    if not bers: return ('', '')
    return (f"{statistics.mean(bers):.3e}",
            f"{(statistics.stdev(bers) if len(bers)>1 else 0):.3e}")

with open(sum_path,'w',newline='') as f:
    w = csv.writer(f); w.writerow(cols)
    for k in sorted(groups):
        modcod, ch, snr = k
        g = groups[k]
        fh = col_stats(g, 'fp64h_be', 'fp64h_bits')
        qh = col_stats(g, 'q15h_be',  'q15h_bits')
        row = [modcod, ch, snr, len(g), fh[0], fh[1], qh[0], qh[1]]
        if en_soft:
            fs = col_stats(g, 'fp64s_be')
            qs = col_stats(g, 'q15s_be')
            row += [fs[0], fs[1], qs[0], qs[1]]
        if en_hls:
            hs = col_stats(g, 'hls_be', 'hls_bits')
            row += [hs[0], hs[1]]
        w.writerow(row)

print(f"  → wrote summary: {sum_path}")
PYEOF

echo ""
echo "════════════════════════════════════════════════════════════════════════════════"
echo "  REGRESSION COMPLETE"
echo "  Per-frame raw    : $RAW_CSV"
echo "  Aggregated mean  : $SUM_CSV"
echo "════════════════════════════════════════════════════════════════════════════════"
