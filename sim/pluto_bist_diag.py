"""
pluto_bist_diag.py — Diagnostic for AD9363 BIST loopback. Captures one OFDM
frame through Pluto with BIST loopback enabled, saves both TX and RX IQ to
.npy files, and runs a sample-by-sample TX-vs-RX comparison to verify that
BIST is acting as a bit-exact (or near-bit-exact) digital loopback.

What it reports:
  • Cross-correlation peak between TX preamble and RX (alignment offset)
  • Linear fit  rx = alpha * tx + n,  where alpha is a constant complex
    scale.  |alpha| is the loopback gain, arg(alpha) the phase rotation.
  • Per-symbol SNR across the cycle (key check: TX-zero symbols should come
    back as 0; signal symbols should have high SNR vs the linear-fit residual).
  • Overall SNR averaged over TX-non-zero region.

Files written to cwd:
  tb_pluto_bist_tx.npy     complex64, the TX burst (post-DAC scaling)
  tb_pluto_bist_rx.npy     complex128, raw RX capture from Pluto

Usage:
  python3 pluto_bist_diag.py [--uri ip:192.168.2.1] [--rate 20e6]
"""

import argparse
import sys
import time
import numpy as np

import ofdm_reference as ofdm

SYM = ofdm.FFT_SIZE + ofdm.CP_LEN  # 288


def main():
    ap = argparse.ArgumentParser(description=__doc__,
            formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--uri",      default="ip:192.168.2.1")
    ap.add_argument("--rate",     type=float, default=20e6)
    ap.add_argument("--freq",     type=float, default=2.4e9)
    ap.add_argument("--tx-gain",  type=float, default=-10.0)
    ap.add_argument("--rx-gain",  type=float, default=30.0)
    ap.add_argument("--mod",      type=int, default=1, choices=[0, 1])
    ap.add_argument("--n-syms",   type=int, default=255)
    ap.add_argument("--fec-rate", type=int, default=0, choices=[0, 1])
    ap.add_argument("--rx-samples", type=int, default=200_000)
    ap.add_argument("--tx-file",  default="tb_pluto_bist_tx.npy")
    ap.add_argument("--rx-file",  default="tb_pluto_bist_rx.npy")
    args = ap.parse_args()

    try:
        import adi
    except ImportError:
        sys.exit("error: pyadi-iio not installed")

    # ── Build burst ──────────────────────────────────────────────
    sig = ofdm.generate(mod=args.mod, n_syms=args.n_syms,
                        fec_rate=args.fec_rate).astype(np.complex64)
    LEAD = 2 * SYM
    burst = np.concatenate([
        np.zeros(LEAD, dtype=np.complex64),
        sig,
        np.zeros(LEAD, dtype=np.complex64),
    ])
    tx_n = burst * (2**13 / np.max(np.abs(burst)))

    # ── Configure Pluto + enable BIST ────────────────────────────
    sdr = adi.Pluto(uri=args.uri)
    sdr.sample_rate = int(args.rate)
    sdr.tx_lo = sdr.rx_lo = int(args.freq)
    rf_bw = int(max(args.rate * 1.25, 5e6))
    sdr.tx_rf_bandwidth = sdr.rx_rf_bandwidth = rf_bw
    sdr.tx_hardwaregain_chan0 = float(args.tx_gain)
    sdr.gain_control_mode_chan0 = "manual"
    sdr.rx_hardwaregain_chan0 = float(args.rx_gain)
    sdr.tx_cyclic_buffer = True
    sdr.rx_buffer_size = int(args.rx_samples)

    requested = "1"
    sdr._ctrl.debug_attrs["loopback"].value = requested
    readback = sdr._ctrl.debug_attrs["loopback"].value
    print(f"BIST loopback: requested={requested!r}  read-back={readback!r}")

    # ── TX/RX ────────────────────────────────────────────────────
    sdr.tx(tx_n)
    time.sleep(0.05)
    for _ in range(2):
        sdr.rx()
    rx = sdr.rx().astype(np.complex128)
    sdr.tx_destroy_buffer()

    # ── Save IQ to disk ──────────────────────────────────────────
    np.save(args.tx_file, tx_n.astype(np.complex64))
    np.save(args.rx_file, rx)
    print(f"Saved TX IQ: {args.tx_file}  ({len(tx_n)} samples, complex64)")
    print(f"Saved RX IQ: {args.rx_file}  ({len(rx)} samples, complex128)")

    # ── Cross-correlation alignment ──────────────────────────────
    tx_preamble = burst[LEAD + SYM : LEAD + 2*SYM].astype(np.complex128)
    tx_p = tx_preamble / (np.linalg.norm(tx_preamble) + 1e-12)
    N_search = len(rx) - 257*SYM
    xc = np.array([np.abs(np.vdot(tx_p, rx[i:i+SYM])) for i in range(N_search)])
    best = int(np.argmax(xc))
    rx_norm_at_peak = xc[best] / (np.linalg.norm(rx[best:best+SYM]) + 1e-12)
    print(f"\nPreamble cross-correlation peak: offset={best}  "
          f"|xc|/||rx||={rx_norm_at_peak:.4f}  (1.0 = perfect shape match)")

    # ── Linear fit  rx_cycle = alpha * tx_cycle + noise ──────────
    tx_cycle = burst.astype(np.complex128)
    rx_cycle_start = best - (LEAD + SYM)
    if rx_cycle_start < 0 or rx_cycle_start + len(tx_cycle) > len(rx):
        sys.exit(f"error: RX capture too short to extract one cycle "
                 f"(start={rx_cycle_start}, len={len(tx_cycle)}, "
                 f"rx_len={len(rx)})")
    rx_cycle = rx[rx_cycle_start : rx_cycle_start + len(tx_cycle)]

    sig_mask = np.abs(tx_cycle) > 1e-3
    alpha = (np.vdot(tx_cycle[sig_mask], rx_cycle[sig_mask])
             / np.vdot(tx_cycle[sig_mask], tx_cycle[sig_mask]))
    residual = rx_cycle - alpha * tx_cycle

    print(f"\nLinear fit  rx = alpha * tx + n")
    print(f"  alpha     = {alpha:.4f}")
    print(f"  |alpha|   = {abs(alpha):.4f}")
    print(f"  arg(alpha)= {np.angle(alpha, deg=True):+.3f}°")

    # ── Per-symbol SNR table ─────────────────────────────────────
    print(f"\nPer-symbol RX vs alpha*TX match (across one cycle):")
    print(f"  {'sym':>3}  {'pos':>6}  {'tx_rms':>10}  {'rx_rms':>10}  {'SNR_dB':>8}  note")
    sample_syms = [0, 1, 2, 3, 4, 5, 10, 50, 100, 200, 257, 258, 259, 260]
    notes = {0:"lead0", 1:"lead0", 2:"guard", 3:"preamble",
             4:"header", 257:"last data", 258:"trail0", 259:"trail0", 260:"trail0"}
    for sym in sample_syms:
        pos = sym * SYM
        if pos + SYM > len(tx_cycle):
            continue
        tx_seg = tx_cycle[pos:pos+SYM]
        rx_seg = rx_cycle[pos:pos+SYM]
        tx_rms = np.sqrt(np.mean(np.abs(tx_seg)**2))
        rx_rms = np.sqrt(np.mean(np.abs(rx_seg)**2))
        if np.sum(np.abs(tx_seg)**2) < 1e-9:
            snr_str = "    N/A"
        else:
            sig_p = np.sum(np.abs(alpha*tx_seg)**2)
            err_p = np.sum(np.abs(rx_seg - alpha*tx_seg)**2)
            snr = 10*np.log10(sig_p / max(err_p, 1e-30))
            snr_str = f"{snr:>8.2f}"
        note = notes.get(sym, "data")
        print(f"  {sym:>3}  {pos:>6}  {tx_rms:>10.4f}  {rx_rms:>10.4f}  {snr_str}  {note}")

    sig_p = np.sum(np.abs(alpha*tx_cycle[sig_mask])**2)
    err_p = np.sum(np.abs(residual[sig_mask])**2)
    overall_snr = 10*np.log10(sig_p / err_p)
    print(f"\nOverall SNR over TX-non-zero region: {overall_snr:.2f} dB")
    print(f"  > 30 dB: near bit-exact loopback")
    print(f"  15-25 dB: noisy but FEC will recover")
    print(f"  < 10 dB: real RF or BIST issue")


if __name__ == "__main__":
    main()
