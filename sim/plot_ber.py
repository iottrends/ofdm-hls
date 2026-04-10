#!/usr/bin/env python3
"""
plot_ber.py  —  BER vs SNR waterfall plot from run_ber_sweep.sh output

Reads ber_results.csv and produces:
  ber_waterfall.png  — one subplot per channel type, QPSK + 16QAM curves,
                       with theoretical AWGN reference lines

Usage:
  python3 plot_ber.py                        # reads ber_results.csv
  python3 plot_ber.py --csv my_results.csv   # custom input file
  python3 plot_ber.py --no-theory            # suppress theoretical curves
"""

import argparse
import os
import numpy as np
import matplotlib
matplotlib.use('Agg')   # headless — saves PNG without display
import matplotlib.pyplot as plt
from matplotlib.ticker import LogLocator
from scipy.special import erfc

# ── Theoretical BER (uncoded AWGN — for reference only) ──────────────
# Note: with K=7 rate-1/2 Viterbi, coded BER is ~4-5 dB better than
# these uncoded curves.  Coded theoretical curves require simulation.

def qpsk_ber_awgn(snr_db):
    """Uncoded QPSK BER in AWGN."""
    snr_lin = 10 ** (snr_db / 10.0)
    return 0.5 * erfc(np.sqrt(snr_lin / 2.0))


def qam16_ber_awgn(snr_db):
    """Uncoded 16-QAM BER in AWGN (approximate)."""
    snr_lin = 10 ** (snr_db / 10.0)
    return 0.75 * erfc(np.sqrt(snr_lin / 10.0))


# ── Load and aggregate CSV ────────────────────────────────────────────
def load_results(csv_file):
    """
    Returns dict: {(channel, snr_db): (total_errors, total_bits)}
    Reads per-frame rows and aggregates across frames.
    """
    results = {}
    if not os.path.exists(csv_file):
        print(f"[PLT] ERROR: {csv_file} not found")
        return results

    with open(csv_file) as f:
        header = f.readline().strip().split(',')
        for line in f:
            parts = line.strip().split(',')
            if len(parts) < 5:
                continue
            channel    = parts[0]
            snr_db     = float(parts[1])
            bit_errors = int(parts[3]) if parts[3].isdigit() else 0
            total_bits = int(parts[4]) if parts[4].isdigit() else 0
            key = (channel, snr_db)
            if key not in results:
                results[key] = [0, 0]
            results[key][0] += bit_errors
            results[key][1] += total_bits

    return results


def compute_ber(results):
    """Convert aggregated counts to BER values per (channel, snr)."""
    ber_data = {}   # {channel: [(snr_db, ber), ...]}
    for (channel, snr_db), (errors, total) in sorted(results.items()):
        if total == 0:
            continue
        ber = errors / total
        # Floor at 1e-7 for plotting (0 errors → plot as <1e-7)
        ber_plot = max(ber, 1e-7) if ber > 0 else 1e-7
        is_floor = (ber == 0)
        if channel not in ber_data:
            ber_data[channel] = []
        ber_data[channel].append((snr_db, ber_plot, is_floor, errors, total))
    return ber_data


# ── Plot ──────────────────────────────────────────────────────────────
CHANNEL_LABELS = {
    'awgn':      'AWGN only',
    'phase':     'AWGN + Phase noise (σ=0.005 rad/samp)',
    'multipath': 'AWGN + Multipath (2-tap UAV)',
    'cfo':       'CFO (0.3 SC) + AWGN',
    'combined':  'Combined (CFO + Multipath + AWGN + Phase)',
}

CHANNEL_COLORS = {
    'awgn':      '#1f77b4',   # blue
    'phase':     '#ff7f0e',   # orange
    'multipath': '#2ca02c',   # green
    'cfo':       '#d62728',   # red
    'combined':  '#9467bd',   # purple
}


def plot_ber(csv_file='ber_results.csv', output='ber_waterfall.png',
             show_theory=True):
    results  = load_results(csv_file)
    if not results:
        print("[PLT] No data to plot.")
        return

    ber_data = compute_ber(results)
    channels = list(ber_data.keys())
    n_chan   = len(channels)

    if n_chan == 0:
        print("[PLT] No valid BER data found.")
        return

    # Layout: one subplot per channel type
    ncols = min(n_chan, 2)
    nrows = (n_chan + ncols - 1) // ncols
    fig, axes = plt.subplots(nrows, ncols,
                              figsize=(7 * ncols, 5 * nrows),
                              squeeze=False)
    axes_flat = [axes[r][c] for r in range(nrows) for c in range(ncols)]

    # Theoretical SNR range
    snr_th = np.linspace(-2, 30, 200)

    for idx, channel in enumerate(channels):
        ax = axes_flat[idx]
        color = CHANNEL_COLORS.get(channel, '#333333')
        label = CHANNEL_LABELS.get(channel, channel)

        pts = ber_data[channel]
        snr_vals  = [p[0] for p in pts]
        ber_vals  = [p[1] for p in pts]
        is_floors = [p[2] for p in pts]

        # Measured BER line
        ax.semilogy(snr_vals, ber_vals,
                    'o-', color=color, linewidth=2, markersize=6,
                    label='HLS chain (measured)')

        # Mark floor points (BER=0 → plotted at 1e-7) with downward triangle
        floor_snrs = [s for s, f in zip(snr_vals, is_floors) if f]
        if floor_snrs:
            ax.semilogy(floor_snrs, [1e-7] * len(floor_snrs),
                        'v', color=color, markersize=10,
                        label='BER=0 (no errors)')

        # Theoretical AWGN curves (uncoded — shown as lower-bound reference)
        if show_theory:
            th_qpsk  = [qpsk_ber_awgn(s)  for s in snr_th]
            th_16qam = [qam16_ber_awgn(s) for s in snr_th]
            ax.semilogy(snr_th, th_qpsk,  'k--', linewidth=1,
                        alpha=0.5, label='Uncoded QPSK theory')
            ax.semilogy(snr_th, th_16qam, 'k:',  linewidth=1,
                        alpha=0.5, label='Uncoded 16QAM theory')
            # Annotate: coded gain
            ax.annotate('←  ~4–5 dB\n   Viterbi\n   coding gain',
                        xy=(5, qpsk_ber_awgn(5)),
                        xytext=(8, 1e-2),
                        fontsize=8, color='gray',
                        arrowprops=dict(arrowstyle='->', color='gray'))

        # BER threshold lines
        ax.axhline(1e-3, color='gray', linestyle=':', linewidth=0.8, alpha=0.6)
        ax.axhline(1e-5, color='gray', linestyle=':', linewidth=0.8, alpha=0.6)
        ax.text(ax.get_xlim()[0] if ax.get_xlim()[0] > -10 else -2,
                1.3e-3, '10⁻³', fontsize=7, color='gray')
        ax.text(ax.get_xlim()[0] if ax.get_xlim()[0] > -10 else -2,
                1.3e-5, '10⁻⁵', fontsize=7, color='gray')

        ax.set_xlabel('SNR (dB)', fontsize=11)
        ax.set_ylabel('BER', fontsize=11)
        ax.set_title(label, fontsize=11, fontweight='bold')
        ax.set_ylim(5e-8, 1.0)
        ax.set_xlim(-2, 28)
        ax.yaxis.set_major_locator(LogLocator(base=10, numticks=8))
        ax.grid(True, which='both', alpha=0.3)
        ax.legend(fontsize=8, loc='upper right')

    # Hide unused subplots
    for idx in range(n_chan, len(axes_flat)):
        axes_flat[idx].set_visible(False)

    # ── Annotate with pass criteria ──────────────────────────
    fig.text(0.5, 0.01,
             'Pass criteria: BER=0 at ≥20 dB SNR for all channel types  |  '
             'QPSK r½ target: BER<10⁻⁴ @ 5 dB  |  16QAM r½ target: BER<10⁻⁴ @ 10 dB',
             ha='center', fontsize=8, color='#555555')

    fig.suptitle('OFDM HLS Chain — BER vs SNR  (K=7 r½ Viterbi + BPSK pilots + CPE tracking)',
                 fontsize=13, fontweight='bold', y=1.01)

    plt.tight_layout()
    plt.savefig(output, dpi=150, bbox_inches='tight')
    print(f"[PLT] Saved → {output}")

    # ── Print text summary ────────────────────────────────────
    print("")
    print("BER Summary:")
    print(f"  {'Channel':<30} {'SNR':>6}  {'Errors':>8}  {'Total':>8}  {'BER':>10}")
    print("  " + "-" * 70)
    for channel in channels:
        for (snr_db, ber_plot, is_floor, errors, total) in ber_data[channel]:
            ber_str = "0 (floor)" if is_floor else f"{errors/total:.3e}"
            print(f"  {CHANNEL_LABELS.get(channel,channel):<30} "
                  f"{snr_db:>6.0f}  {errors:>8}  {total:>8}  {ber_str:>10}")
    print("")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot BER vs SNR from sweep CSV")
    parser.add_argument("csv_pos",      nargs="?", default=None,
                        help="Input CSV file (positional, optional)")
    parser.add_argument("--csv",        default="ber_results.csv",
                        help="Input CSV file (default: ber_results.csv)")
    parser.add_argument("--output",     default=None,
                        help="Output PNG (default: <csv_basename>.png)")
    parser.add_argument("--no-theory",  action="store_true",
                        help="Suppress theoretical AWGN reference curves")
    args = parser.parse_args()
    csv_file = args.csv_pos if args.csv_pos else args.csv
    output   = args.output  if args.output  else os.path.splitext(csv_file)[0] + ".png"
    plot_ber(csv_file=csv_file, output=output,
             show_theory=not args.no_theory)
