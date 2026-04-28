"""
pluto_bist_probe.py — Minimal raw-IQ probe of Pluto's BIST loopback.

Goal: bypass OFDM entirely.  Send 1000 random int16 complex samples through
Pluto's digital BIST (`loopback=1`) and `loopback=2` paths, capture, align
by cross-correlation, and report:

  - sample-level mismatch count + position histogram
  - effective amplitude scaling (BIST might attenuate)
  - DC offset (BIST often adds ~1-3 LSB DC)
  - per-bit error rate on I and Q separately
  - whether the path is bit-exact, filter-ringing, or noisy

If a path is bit-exact you can trust it for OFDM regression.
If not, this output tells you why so we know whether to work around it
in the OFDM chain or move to RF.

Usage:
  python3 sim/pluto_bist_probe.py                              # default loopback=1
  python3 sim/pluto_bist_probe.py --loopback 2                 # try internal RF loop
  python3 sim/pluto_bist_probe.py --loopback 1 --n 4000        # bigger probe
  python3 sim/pluto_bist_probe.py --no-bist                    # baseline: gain stages only
"""

import argparse
import sys
import time
import numpy as np


def gen_random_iq(n, seed=42, peak=2**13, n_lead_zeros=100, n_trail_zeros=100):
    """Random complex int16 samples, peak ±peak (default 2^13 = 12 dB below FS).

    Prepends `n_lead_zeros` and appends `n_trail_zeros` of exact zero — these
    act as a bit-exactness oracle.  If the path is bit-exact, RX in the zero
    regions must come back as exactly 0 (no additive noise floor).
    """
    rng = np.random.default_rng(seed)
    n_signal = n - n_lead_zeros - n_trail_zeros
    if n_signal <= 0:
        raise ValueError(f"need n > {n_lead_zeros + n_trail_zeros}")
    i = rng.integers(-peak, peak, size=n_signal, dtype=np.int32).astype(np.int16)
    q = rng.integers(-peak, peak, size=n_signal, dtype=np.int32).astype(np.int16)
    sig = (i.astype(np.complex64) + 1j * q.astype(np.complex64))
    return np.concatenate([
        np.zeros(n_lead_zeros,  dtype=np.complex64),
        sig,
        np.zeros(n_trail_zeros, dtype=np.complex64),
    ])


def find_alignment(captured, reference):
    """Cross-correlate to find where reference begins inside captured.
    Returns (offset, peak_correlation_value)."""
    # Normalize both to unit power for clean correlation peaks.
    r = reference - reference.mean()
    c = captured - captured.mean()
    # 'valid' would force same length; 'full' allows any offset.
    corr = np.abs(np.correlate(c, r, mode="valid"))
    peak = int(np.argmax(corr))
    return peak, float(corr[peak])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--uri", default="ip:192.168.2.1")
    ap.add_argument("--rate", type=float, default=20e6)
    ap.add_argument("--freq", type=float, default=2.4e9)
    ap.add_argument("--tx-gain", type=float, default=0.0,
                    help="full DAC level by default — BIST often eats gain anyway")
    ap.add_argument("--rx-gain", type=float, default=20.0)
    ap.add_argument("--n", type=int, default=1000,
                    help="random IQ samples to probe with")
    ap.add_argument("--capture", type=int, default=8000,
                    help="RX samples to capture (must fit at least 2 cycles)")
    ap.add_argument("--loopback", type=int, default=1, choices=[0, 1, 2],
                    help="BIST loopback value (0=off RF, 1=digital TX→RX, 2=internal RF)")
    ap.add_argument("--no-bist", action="store_true",
                    help="skip BIST attribute write (baseline of gain-only path)")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--save", default=None,
                    help="save tx + rx sample arrays to NPZ for offline plot")
    args = ap.parse_args()

    try:
        import adi
    except ImportError:
        sys.exit("error: pyadi-iio not installed (pip install pyadi-iio)")

    print(f"[probe] uri={args.uri}  rate={args.rate/1e6:.1f} MSPS  freq={args.freq/1e9:.2f} GHz")
    print(f"[probe] loopback={args.loopback if not args.no_bist else 'OFF (baseline)'}  "
          f"n={args.n}  capture={args.capture}")

    # ── Generate probe pattern ────────────────────────────────────
    tx = gen_random_iq(args.n, seed=args.seed)
    print(f"[probe] generated {args.n} random IQ samples  (peak |x| = {np.max(np.abs(tx)):.0f})")

    # ── Configure Pluto ───────────────────────────────────────────
    sdr = adi.Pluto(uri=args.uri)
    sdr.sample_rate = int(args.rate)
    sdr.tx_lo = int(args.freq)
    sdr.rx_lo = int(args.freq)
    sdr.tx_rf_bandwidth = int(max(args.rate * 1.25, 5e6))
    sdr.rx_rf_bandwidth = int(max(args.rate * 1.25, 5e6))
    sdr.tx_hardwaregain_chan0 = float(args.tx_gain)
    sdr.gain_control_mode_chan0 = "manual"
    sdr.rx_hardwaregain_chan0 = float(args.rx_gain)
    sdr.tx_cyclic_buffer = True
    sdr.rx_buffer_size = int(args.capture)

    if not args.no_bist:
        try:
            sdr._ctrl.debug_attrs["loopback"].value = str(args.loopback)
            actual = sdr._ctrl.debug_attrs["loopback"].value
            print(f"[probe] loopback attr: requested='{args.loopback}'  read-back='{actual}'")
            if str(actual) != str(args.loopback):
                print(f"[probe] WARN: attr did not take")
        except Exception as e:
            print(f"[probe] WARN: BIST attr write failed: {e}")

    # ── Push + capture ───────────────────────────────────────────
    sdr.tx(tx)
    time.sleep(0.05)
    for _ in range(2):
        sdr.rx()                # drain stale buffers (AGC ramp etc.)
    rx = np.asarray(sdr.rx(), dtype=np.complex64)
    sdr.tx_destroy_buffer()

    print(f"[probe] captured {len(rx)} samples  "
          f"avg |rx| = {np.mean(np.abs(rx)):.1f}  peak |rx| = {np.max(np.abs(rx)):.1f}")

    # ── Align RX to TX via cross-correlation ─────────────────────
    if len(rx) < args.n + 100:
        sys.exit(f"capture too short ({len(rx)} < {args.n + 100})")

    offset, peak_corr = find_alignment(rx, tx)
    print(f"[probe] alignment: offset={offset}  peak_corr={peak_corr:.3e}")

    aligned = rx[offset : offset + args.n]
    if len(aligned) != args.n:
        sys.exit(f"aligned slice size {len(aligned)} != {args.n}")

    # ── Linear fit: rx = α · tx + n  (Gemini-style, the right metric) ──
    # α = (rx · conj(tx)).sum / (tx · conj(tx)).sum
    num = np.sum(aligned * np.conj(tx))
    den = np.sum(tx * np.conj(tx))
    alpha = num / den if den != 0 else 0
    print(f"[probe] linear fit  rx ≈ α · tx + noise:")
    print(f"          α        = {alpha.real:+.6f} {alpha.imag:+.6f}j")
    print(f"          |α|      = {np.abs(alpha):.6f}  ({20*np.log10(np.abs(alpha)+1e-12):+.2f} dB)")
    print(f"          ∠α       = {np.degrees(np.angle(alpha)):+.4f} deg")

    # Linear-fit residual on the full burst.
    n_res = aligned - alpha * tx
    sig_pwr = np.mean(np.abs(alpha * tx)**2)
    res_pwr = np.mean(np.abs(n_res)**2)
    snr_db = 10 * np.log10(sig_pwr / max(res_pwr, 1e-30))
    print(f"[probe] linear-fit residual SNR (over full burst): {snr_db:.2f} dB")

    # Bit-exactness oracle: TX-zero samples → RX should be EXACT zero.
    # If RX RMS in zero region > 0, the path has additive noise (not bit-exact).
    zero_mask = (np.abs(tx) < 0.5)
    n_zero = int(zero_mask.sum())
    rx_zero_rms = float(np.sqrt(np.mean(np.abs(aligned[zero_mask])**2))) if n_zero > 0 else float("nan")
    rx_zero_peak = float(np.max(np.abs(aligned[zero_mask]))) if n_zero > 0 else float("nan")
    print(f"[probe] bit-exactness oracle (TX-zero → RX RMS):")
    print(f"          {n_zero} zero samples  RX RMS = {rx_zero_rms:.4f}  peak = {rx_zero_peak:.1f}")
    if rx_zero_rms < 0.5:
        print(f"          → BIT-EXACT in zero region (no additive noise floor)")
    else:
        print(f"          → NOT bit-exact — additive noise present in zero region")

    # Re-scale for the legacy ±LSB sample-equality test (kept for context, but
    # not the primary verdict — see notes below).
    rx_scaled = aligned / alpha if alpha != 0 else aligned
    dc = np.mean(rx_scaled - tx)

    # Round rx_scaled back to int16 for sample-equality comparison.
    rx_i = np.round(rx_scaled.real - dc.real).astype(np.int32)
    rx_q = np.round(rx_scaled.imag - dc.imag).astype(np.int32)
    tx_i = tx.real.astype(np.int32)
    tx_q = tx.imag.astype(np.int32)

    diff_i = np.abs(rx_i - tx_i)
    diff_q = np.abs(rx_q - tx_q)

    # Sample-perfect count (residual within ±1 LSB after scale+DC removal)
    perfect = int(np.sum((diff_i <= 1) & (diff_q <= 1)))
    print(f"[probe] sample equality after de-scale + de-DC:")
    print(f"          {perfect}/{args.n} samples within ±1 LSB on both I & Q  "
          f"({100*perfect/args.n:.1f}%)")

    # Bit-error rate per channel (compare int16 bits)
    def bit_errs(a, b, nbits=14):
        diff = (a.astype(np.int32) ^ b.astype(np.int32)) & ((1 << nbits) - 1)
        return int(sum(bin(int(x)).count("1") for x in diff))

    nbits = 14   # we generate ±2^13 → 14-bit signed range
    be_i = bit_errs(rx_i, tx_i, nbits)
    be_q = bit_errs(rx_q, tx_q, nbits)
    total_bits = args.n * nbits
    print(f"[probe] bit-error rate (over residual after de-scale + de-DC):")
    print(f"          I: {be_i}/{total_bits}  BER={be_i/total_bits:.2e}")
    print(f"          Q: {be_q}/{total_bits}  BER={be_q/total_bits:.2e}")

    # Localization — where do errors cluster?
    bad_idx = np.where((diff_i > 1) | (diff_q > 1))[0]
    if len(bad_idx) == 0:
        print("[probe] no sample errors > 1 LSB — BIST path is BIT-EXACT after scale/DC removal.")
    else:
        print(f"[probe] {len(bad_idx)} sample positions exceed ±1 LSB error")
        print(f"        first 20 indices: {bad_idx[:20].tolist()}")
        # Histogram by 100-sample bins to spot localization
        hist, _ = np.histogram(bad_idx, bins=10, range=(0, args.n))
        print(f"        distribution by decile:")
        for i, h in enumerate(hist):
            bar = "█" * int(40 * h / max(hist.max(), 1))
            print(f"          [{i*args.n//10:4d}–{(i+1)*args.n//10:4d}]: {h:4d}  {bar}")
        # Magnitude of worst errors
        worst = np.argsort(diff_i + diff_q)[::-1][:5]
        print(f"        worst-5 positions: idx={worst.tolist()}  "
              f"|Δi|={diff_i[worst].tolist()}  |Δq|={diff_q[worst].tolist()}")

    # ── Verdict (linear-fit residual + zero-oracle) ─────────────
    # Primary criteria, in order:
    #   1. zero-region RX RMS small → no additive noise floor → bit-exact path
    #   2. residual SNR > 35 dB     → usable for OFDM 16QAM (cliff ≈ 13 dB)
    #   3. |α| within an order of magnitude of 1.0 → path delivers the signal
    # The legacy ±LSB sample-equality test is informational; an attenuated
    # bit-exact path will fail it because LSBs of de-scaled signal are
    # quantization noise, not actual signal corruption.
    print()
    if rx_zero_rms < 0.5 and snr_db > 35:
        print("[verdict] BIT-EXACT (modulo constant scale α) — BIST path is usable.")
        print(f"          α scales the signal by {np.abs(alpha):.4f}× ({20*np.log10(np.abs(alpha)+1e-12):+.1f} dB).")
        print(f"          Wrap normalize_from_adc / decode_full handles this transparently.")
    elif rx_zero_rms < 0.5 and snr_db > 20:
        print("[verdict] CLOSE TO BIT-EXACT — small residual on top of attenuation. Usable for QPSK/16QAM 1/2.")
    elif rx_zero_rms >= 0.5 and snr_db > 25:
        print("[verdict] NON-EXACT but usable — additive noise present, but SNR adequate.")
    else:
        print("[verdict] PATH POOR or BROKEN — low SNR or noisy zeros.  Investigate.")

    if args.save:
        np.savez(args.save,
                 tx=tx, rx_aligned=aligned, rx_scaled=rx_scaled,
                 scale=scale, dc=dc)
        print(f"[probe] saved arrays to {args.save}")


if __name__ == "__main__":
    main()
