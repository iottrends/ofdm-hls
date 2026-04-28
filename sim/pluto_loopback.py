"""
pluto_loopback.py — End-to-end OFDM TX/RX over ADALM-Pluto using the Python
reference chain (`ofdm_reference.generate` for TX, `ofdm_reference.decode_full`
for RX).

Modes (--mode):
  digital    No radio. TX samples fed straight into the RX (sanity check
             that the wrapper plumbing is correct before involving HW).
  pluto-bist One Pluto, AD9363 internal digital BIST loopback. Path:
             TX baseband → DAC → digital-loopback inside the chip → ADC →
             RX baseband. RF front-end is bypassed entirely. Smoke-tests
             IIO + buffers without exercising the analog path.
  pluto-rf   One Pluto, RF self-loopback. TX SMA → (cable + attenuator OR
             two close-coupled antennas) → RX SMA. Full analog path:
             mixer, PA, LNA, ADC. CFO is shared between TX/RX (same LO),
             so this isolates analog impairments from frequency offset.
  two-pluto  Two Plutos: --uri-tx and --uri-rx. Independent TCXOs → real
             CFO and SRO. OTA or cable.

Operating point (RX target ≈ -45 dBm at the SMA):
  ADC sweet spot for AD9363 is roughly -30 to -50 dBm at the RX port.
  Default gains aim for that under typical bench setups:
    pluto-rf   : --tx-gain -30 dB → ~-23 dBm out; with a 30 dB attenuator
                 in line, RX sees ~-53 dBm; --rx-gain 40 dB lands the ADC
                 near mid-scale.
    two-pluto  : --tx-gain -10 dB at close range / antenna coupling — tune
                 by eye on the RX |x| histogram (you want ~10-20% of full
                 scale, not clipping).
  AGC mode (--agc):
    manual       fixed RX gain via --rx-gain (deterministic, recommended
                 for first bring-up so you can tell signal level from BER)
    slow_attack  AD9363 builtin AGC, gentle (good for OTA where path loss
                 wanders)
    hybrid       slow_attack + manual override on certain attrs
    fast_attack  aggressive AGC (rarely useful for OFDM; PAPR can fool it)

Quick start:
  # No hardware required — wrapper sanity check:
  python3 pluto_loopback.py --mode digital

  # One Pluto, BIST (no RF):
  python3 pluto_loopback.py --mode pluto-bist --uri ip:192.168.2.1

  # One Pluto, RF self-loopback at 2.4 GHz with 30 dB attenuator:
  python3 pluto_loopback.py --mode pluto-rf  --uri ip:192.168.2.1 \\
      --freq 2.4e9 --rate 20e6 --tx-gain -30 --rx-gain 40

  # Two Plutos OTA at 2.4 GHz, slow-attack AGC:
  python3 pluto_loopback.py --mode two-pluto \\
      --uri-tx ip:192.168.2.1 --uri-rx ip:192.168.3.1 \\
      --freq 2.4e9 --rate 20e6 --tx-gain -10 --agc slow_attack

The RX is fixed to Q15 fixed-point + soft Viterbi + RS outer decode (the
production path); FP64 and hard-decision modes are intentionally not exposed.

The script must be run from the directory containing tb_input_to_tx.bin so
that decode_full's BER comparison can find the ground-truth bytes that
generate() writes.
"""

import argparse
import os
import sys
import time
import numpy as np

import ofdm_reference as ofdm
from ofdm_reference import FFT_SIZE, CP_LEN

SYM_LEN = FFT_SIZE + CP_LEN  # 288


def build_tx_burst(mod, n_syms, fec_rate, lead_zeros, trail_zeros):
    """Generate one OFDM frame and pad with zeros front + back so the cyclic
    TX buffer has clean dead-air between repeats — gives sync_detect a clean
    rising edge on the preamble correlation."""
    sig = ofdm.generate(mod=mod, n_syms=n_syms, fec_rate=fec_rate)
    sig = sig.astype(np.complex64)
    burst = np.concatenate([
        np.zeros(lead_zeros,  dtype=np.complex64),
        sig,
        np.zeros(trail_zeros, dtype=np.complex64),
    ])
    return burst


def normalize_for_dac(samples, peak=2**13):
    """Scale complex baseband to int16 range expected by Pluto. Peak set to
    2^13 (~12 dB below full-scale) so OFDM PAPR spikes (8-10 dB on data
    symbols, ~5 dB on the ZC preamble) don't clip the DAC."""
    m = np.max(np.abs(samples))
    if m < 1e-12:
        return samples * 0
    return samples * (peak / m)


def normalize_from_adc(samples):
    """Scale Pluto RX (int16 magnitudes) back to the ~unit-magnitude range
    decode_full expects. 95th percentile of |x| as the AGC reference so a
    single high-PAPR spike doesn't crush the body of the signal."""
    a = np.abs(samples)
    if a.size == 0:
        return samples
    ref = np.percentile(a, 95)
    if ref < 1e-9:
        return samples
    return samples * (0.5 / ref)


def write_rx_textfile(path, samples):
    """Two-column float text dump for debug inspection (--save-rx)."""
    iq = np.column_stack([samples.real.astype(np.float64),
                          samples.imag.astype(np.float64)])
    np.savetxt(path, iq, fmt="%.8f")


def estimate_rx_dbm(samples):
    """Rough RSSI from captured I/Q (relative to ADC full-scale, calibrated
    against the AD9363 spec where 0 dBFS ≈ +2 dBm at the RX port with 0 dB
    of LNA/mixer gain). Treats the RX hardwaregain as already applied — i.e.
    this is the apparent signal level *at the SMA after gain compensation*
    only if the caller supplies a gain-corrected sample stream. Otherwise
    it's dBFS-referred."""
    if samples.size == 0:
        return float("nan")
    p_lin = float(np.mean(np.abs(samples) ** 2))
    if p_lin <= 0:
        return float("-inf")
    # int16 full-scale → 0 dBFS, AD9363 ≈ +2 dBm at full-scale.
    fs = 2**15
    return 10 * np.log10(p_lin / (fs * fs)) + 2.0


# ── Pluto I/O ──────────────────────────────────────────────────────
def _import_adi():
    try:
        return __import__("adi")
    except ImportError:
        sys.exit("error: pyadi-iio not installed. Run "
                 "'pip install pyadi-iio' (and ensure libiio is on the system).")


def configure_radio(sdr, args, role):
    """role: 'tx', 'rx', or 'both'.

    RF bandwidth set to max(rate * 1.25, 5 MHz) so the AD9363 anti-alias /
    image filter sits comfortably outside the OFDM occupied bandwidth (which
    is ~80% of fs, since 200 of 256 subcarriers carry data + pilots).
    Setting BW = fs exactly leaves zero margin — filter skirt sits on the
    signal."""
    rf_bw = int(max(args.rate * 1.25, 5e6))
    sdr.sample_rate = int(args.rate)
    if role in ("tx", "both"):
        sdr.tx_lo = int(args.freq)
        sdr.tx_rf_bandwidth = rf_bw
        sdr.tx_hardwaregain_chan0 = float(args.tx_gain)
        sdr.tx_cyclic_buffer = True
    if role in ("rx", "both"):
        sdr.rx_lo = int(args.freq)
        sdr.rx_rf_bandwidth = rf_bw
        sdr.gain_control_mode_chan0 = args.agc
        if args.agc == "manual":
            sdr.rx_hardwaregain_chan0 = float(args.rx_gain)
        sdr.rx_buffer_size = int(args.rx_samples)


def capture_with_pluto(args, burst):
    """Drive Pluto(s) with the burst and return captured RX samples (complex)."""
    adi = _import_adi()

    if args.mode == "two-pluto":
        if not (args.uri_tx and args.uri_rx):
            sys.exit("error: --uri-tx and --uri-rx are required for --mode two-pluto")
        tx = adi.Pluto(uri=args.uri_tx)
        rx = adi.Pluto(uri=args.uri_rx)
        configure_radio(tx, args, "tx")
        configure_radio(rx, args, "rx")
    else:
        sdr = adi.Pluto(uri=args.uri)
        configure_radio(sdr, args, "both")
        tx = sdr
        rx = sdr
        if args.mode == "pluto-bist":
            # AD9363 BIST digital loopback: TX baseband → DAC → digital
            # loopback inside the chip → ADC → RX baseband. RF disabled.
            try:
                rx._ctrl.debug_attrs["loopback"].value = "1"
                # Verify the attribute actually took — pyadi-iio sometimes
                # accepts the write silently without applying it.
                actual = rx._ctrl.debug_attrs["loopback"].value
                print(f"[pluto] BIST loopback attr set: requested='1', read-back='{actual}'")
                if str(actual) != "1":
                    print("[pluto] WARN: BIST attr did NOT take — RF path is active. "
                          "Either firmware doesn't support BIST or attr name differs. "
                          "Try --mode pluto-rf with an attenuator instead.")
            except Exception as e:
                print(f"warning: could not enable BIST loopback ({e}); "
                      "AD9363 will run RF path — connect TX/RX with a cable.")

    tx_samples = normalize_for_dac(burst)
    tx.tx(tx_samples)

    # Let cyclic TX push out a full frame (~3.7 ms at 20 MSPS) and the RX
    # path settle. 50 ms is comfortable headroom even for slow_attack AGC.
    time.sleep(args.settle)

    # Drain stale samples (AGC ramp, partial frames) before the real capture.
    for _ in range(args.flush):
        rx.rx()

    captured = rx.rx()

    tx.tx_destroy_buffer()
    return np.asarray(captured, dtype=np.complex64)


# ── Main ───────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                 description=__doc__)
    ap.add_argument("--mode", choices=["digital", "pluto-bist",
                                        "pluto-rf", "two-pluto"],
                    default="digital")
    ap.add_argument("--uri",    default="ip:192.168.2.1",
                    help="Pluto URI for single-radio modes")
    ap.add_argument("--uri-tx", default=None, help="TX Pluto URI (two-pluto)")
    ap.add_argument("--uri-rx", default=None, help="RX Pluto URI (two-pluto)")

    # RF / sample-rate config.  Defaults target the 20 MSPS, 2.4 GHz ISM
    # corner and an RX level around -45 dBm at the SMA — see module docstring.
    ap.add_argument("--freq",   type=float, default=2.4e9,
                    help="LO frequency in Hz (default 2.4e9 = 2.4 GHz ISM)")
    ap.add_argument("--rate",   type=float, default=20e6,
                    help="sample rate in Hz (default 20e6 = 20 MSPS, "
                         "matches the FPGA chain)")
    ap.add_argument("--tx-gain", type=float, default=-30.0,
                    help="AD9363 TX hardwaregain dB, negative = more atten "
                         "(0 = max ~+7 dBm out, default -30 = ~-23 dBm)")
    ap.add_argument("--rx-gain", type=float, default=40.0,
                    help="AD9363 RX hardwaregain dB (manual AGC only); "
                         "default 40 puts a -45 dBm input near ADC mid-scale")
    ap.add_argument("--agc", choices=["manual", "slow_attack",
                                      "fast_attack", "hybrid"],
                    default="manual",
                    help="RX AGC mode (default manual = fixed --rx-gain)")
    ap.add_argument("--rx-samples", type=int, default=200_000,
                    help="samples per rx() call (default 200k = ~10 ms at 20 MSPS)")
    ap.add_argument("--settle", type=float, default=0.05,
                    help="seconds to wait after starting TX before capture")
    ap.add_argument("--flush", type=int, default=2,
                    help="number of rx() reads to drop before keeping one")

    # OFDM frame config.
    ap.add_argument("--mod",      type=int, default=ofdm.MOD,      choices=[0, 1],
                    help="0=QPSK, 1=16-QAM")
    ap.add_argument("--n-syms",   type=int, default=ofdm.N_SYMS)
    ap.add_argument("--fec-rate", type=int, default=ofdm.FEC_RATE, choices=[0, 1],
                    help="0=rate-1/2, 1=rate-2/3 (inner conv code)")
    ap.add_argument("--no-cfo",   action="store_true", help="disable CFO correction")

    ap.add_argument("--lead-zeros",  type=int, default=2*SYM_LEN,
                    help="zero pad before frame (helps sync rising edge)")
    ap.add_argument("--trail-zeros", type=int, default=2*SYM_LEN)

    ap.add_argument("--save-rx", default=None, metavar="PATH",
                    help="optionally dump captured RX samples to a 2-column "
                         "text file (for debug inspection); decode itself runs "
                         "on the in-memory array regardless")

    args = ap.parse_args()

    print(f"[pluto] mode={args.mode}  rx=Q15+SOFT+RS")
    print(f"[pluto] RF: freq={args.freq/1e9:.3f} GHz  rate={args.rate/1e6:.2f} MSPS  "
          f"tx_gain={args.tx_gain:+.1f} dB  rx_gain={args.rx_gain:+.1f} dB ({args.agc})")
    print(f"[pluto] OFDM: mod={args.mod}  n_syms={args.n_syms}  "
          f"fec_rate={args.fec_rate}")

    burst = build_tx_burst(args.mod, args.n_syms, args.fec_rate,
                           args.lead_zeros, args.trail_zeros)
    print(f"[pluto] TX burst: {len(burst)} samples "
          f"({len(burst)/args.rate*1e3:.2f} ms)")

    if args.mode == "digital":
        # Digital sanity path: feed TX burst directly back to RX, bypass radio.
        # The padding zeros must be stripped to match what generate() produced
        # so decode_full sees the same input it would in --gen mode.
        rx_unpadded = burst[args.lead_zeros : args.lead_zeros + (len(burst) - args.lead_zeros - args.trail_zeros)]
        rx_samples_to_decode = rx_unpadded.astype(np.complex128)
    else:
        rx_samples = capture_with_pluto(args, burst)
        rssi_dbfs = estimate_rx_dbm(rx_samples)
        print(f"[pluto] RX captured: {len(rx_samples)} samples "
              f"({len(rx_samples)/args.rate*1e3:.2f} ms)  "
              f"avg level ≈ {rssi_dbfs:+.1f} dBFS")
        if rssi_dbfs > -3:
            print("[pluto] WARN: RX appears to be clipping — back off TX or RX gain")
        elif rssi_dbfs < -50:
            print("[pluto] WARN: RX very weak — increase TX or RX gain")

        # Cross-correlation alignment: the cyclic-TX capture lands at a random
        # phase of the burst, and the captured stream contains 2-3 partial
        # cycles.  sync_detect's autocorrelation can find a preamble but is
        # ambiguous when multiple complete cycles + partial cycles coexist.
        # Since we know the TX waveform exactly, cross-correlate to find the
        # first full cycle and slice it out before handing to decode_full.
        template = burst[:5000].astype(np.complex128)
        corr = np.abs(np.correlate(rx_samples.astype(np.complex128), template, mode="valid"))
        offset = int(np.argmax(corr))
        rx_one = rx_samples[offset : offset + len(burst)]
        if len(rx_one) < len(burst):
            print(f"[pluto] WARN: RX capture ended mid-burst "
                  f"({len(rx_one)}/{len(burst)} samples); decode may fail. "
                  f"Increase --rx-samples.")

        # Deterministic magnitude restore (NO percentile guessing):
        #   pluto-bist : signal is exactly TX × (1/16) due to BIST tap point
        #   pluto-rf / two-pluto : RF path scale unknown, use 95th-percentile fallback
        # In both cases TX peak was 2^13 (normalize_for_dac default).
        if args.mode == "pluto-bist":
            scale_back = 16.0 / (2**13)             # × 16 BIST × ÷ 8192 DAC peak
            rx_scaled = rx_one * scale_back
        else:
            rx_scaled = normalize_from_adc(rx_one)

        # Strip the lead/trail zero pad we added in build_tx_burst so the slice
        # matches what generate() produces (decode_full's expected input).
        sig_len = len(burst) - args.lead_zeros - args.trail_zeros
        rx_unpadded = rx_scaled[args.lead_zeros : args.lead_zeros + sig_len]
        rx_samples_to_decode = rx_unpadded.astype(np.complex128)
        print(f"[pluto] aligned at offset={offset}  scale={scale_back if args.mode == 'pluto-bist' else 'percentile'}  "
              f"peak |rx| = {np.max(np.abs(rx_unpadded)):.4f}")

    if args.save_rx:
        write_rx_textfile(args.save_rx, rx_samples_to_decode)
        print(f"[pluto] saved RX samples to {args.save_rx}")

    print("[pluto] decoding via ofdm_reference.decode_full ...")
    res = ofdm.decode_full(
        tx_file=rx_samples_to_decode,
        mod=args.mod, n_syms=args.n_syms, fec_rate=args.fec_rate,
        use_sync=True,
        use_q15=True,
        use_soft=True,
        use_reed_solomon=True,
        use_cfo_correct=not args.no_cfo,
    )

    if res is None:
        print("[pluto] decode failed (sync miss or short capture)")
        sys.exit(2)
    if res.get("sync_fail"):
        print("[pluto] FAIL: sync_detect did not trigger")
        sys.exit(3)
    bit_err = res.get("bit_errors", 0)
    total   = res.get("total_bits", 0)
    ber     = bit_err / total if total else float("nan")
    print(f"[pluto] header_pass={res.get('header_pass')}  "
          f"bit_err={bit_err}/{total}  BER={ber:.2e}")


if __name__ == "__main__":
    main()
