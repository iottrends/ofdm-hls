#!/usr/bin/env python3
"""
ofdm_channel_sim.py — AWGN channel simulation for OFDM TX+RX HLS verification

Reads the HLS TX output (tb_tx_output_hls.txt), adds AWGN noise, and decodes
using the same Python reference receiver as ofdm_reference.py.

Usage:
  # Add AWGN at SNR=20 dB and write noisy signal for HLS C-sim:
  python3 ofdm_channel_sim.py --snr 20 --write-noisy

  # Sweep SNR 5–35 dB and print empirical BER:
  python3 ofdm_channel_sim.py --sweep

  # Both (write noisy file AND sweep):
  python3 ofdm_channel_sim.py --snr 20 --write-noisy --sweep

  # Use Python reference signal instead of HLS output:
  python3 ofdm_channel_sim.py --sweep --input tb_tx_output_ref.txt
"""

import numpy as np
import argparse
import os
from scipy.special import erfc

# ── Parameters — must match ofdm_tx.h / ofdm_reference.py ──────────
FFT_SIZE     = 256
CP_LEN       = 32
NUM_DATA_SC  = 200
PILOT_IDX    = [50, 75, 100, 154, 179, 204]

DATA_SC_IDX = (
    list(range(25, 50)) + list(range(51, 75)) +
    list(range(76, 100)) + list(range(101, 128)) +
    list(range(129, 154)) + list(range(155, 179)) +
    list(range(180, 204)) + list(range(205, 232))
)
assert len(DATA_SC_IDX) == NUM_DATA_SC

N_SYMS = 255
MOD    = 1   # 0=QPSK, 1=16QAM

HLS_FILE   = "tb_tx_output_hls.txt"
NOISY_FILE = "tb_tx_output_hls_noise.txt"
IN_FILE    = "tb_input_to_tx.bin"

# Quantisation step for ap_fixed<16,1>
Q_STEP = 2**-15


# ── ZC sequence ──────────────────────────────────────────────────────
def zc_sequence(N=256, u=25):
    k = np.arange(N)
    return np.exp(-1j * np.pi * u * k * (k + 1) / N)


# ── AWGN channel ─────────────────────────────────────────────────────
def add_awgn(signal, snr_db, rng=None):
    """
    Add complex AWGN to signal.

    Signal power is computed from the signal itself.  Noise power is
    set so that SNR = 10*log10(Ps/Pn) = snr_db.

    Returns: noisy_signal (same length, complex128)
    """
    if rng is None:
        rng = np.random.default_rng()
    Ps = np.mean(np.abs(signal)**2)
    snr_lin = 10**(snr_db / 10.0)
    Pn = Ps / snr_lin          # total complex noise power per sample
    # Complex AWGN: real + imag each ~ N(0, Pn/2)
    noise = rng.normal(0, np.sqrt(Pn / 2), size=len(signal)) + \
            1j * rng.normal(0, np.sqrt(Pn / 2), size=len(signal))
    return signal + noise


# ── Python reference decoder (copied from ofdm_reference.py) ─────────
def _decode_axis_16qam(v):
    thresh = 2.0 / np.sqrt(10)
    if   v >= thresh:  return 0b00
    elif v >= 0:       return 0b01
    elif v >= -thresh: return 0b11
    else:              return 0b10


def python_rx(sig_c, mod=MOD, n_syms=N_SYMS):
    """
    Decode an OFDM IQ signal using numpy FFT.
    Returns (decoded_bytes, byte_errors, bit_errors) vs tb_input_to_tx.bin.
    Scale-invariant equalization: works for any FFT scaling convention.
    """
    sym_len = FFT_SIZE + CP_LEN  # 288

    zc = zc_sequence()

    # Preamble channel estimation
    preamble_body = sig_c[CP_LEN : sym_len]
    Y_pre = np.fft.fft(preamble_body)
    G = np.zeros(FFT_SIZE, dtype=complex)
    for d, k in enumerate(DATA_SC_IDX):
        G[k] = Y_pre[k] * np.conj(zc[k])

    # Decode each data symbol
    bps = 2 if mod == 0 else 4
    decoded_bits = []

    for s in range(n_syms):
        offset = sym_len * (s + 1)
        sym_iq = sig_c[offset + CP_LEN : offset + sym_len]
        Y = np.fft.fft(sym_iq)

        for d, k in enumerate(DATA_SC_IDX):
            mag2 = abs(G[k])**2
            X_hat = Y[k] * np.conj(G[k]) / mag2 if mag2 > 1e-12 else 0 + 0j

            if mod == 0:  # QPSK
                b1 = 1 if X_hat.real < 0 else 0
                b0 = 1 if X_hat.imag < 0 else 0
                decoded_bits.extend([b1, b0])
            else:         # 16-QAM
                ib = _decode_axis_16qam(X_hat.real)
                qb = _decode_axis_16qam(X_hat.imag)
                decoded_bits.extend([(ib >> 1) & 1, ib & 1, (qb >> 1) & 1, qb & 1])

    # Pack bits to bytes
    decoded_bits = np.array(decoded_bits, dtype=np.uint8)
    if mod == 0:
        n_s = len(decoded_bits) // 2
        decoded_bytes = []
        for i in range(0, n_s, 4):
            s0 = (int(decoded_bits[i*2])     << 1) | int(decoded_bits[i*2+1])
            s1 = (int(decoded_bits[(i+1)*2]) << 1) | int(decoded_bits[(i+1)*2+1])
            s2 = (int(decoded_bits[(i+2)*2]) << 1) | int(decoded_bits[(i+2)*2+1])
            s3 = (int(decoded_bits[(i+3)*2]) << 1) | int(decoded_bits[(i+3)*2+1])
            decoded_bytes.append((s0 << 6) | (s1 << 4) | (s2 << 2) | s3)
    else:
        n_s = len(decoded_bits) // 4
        decoded_bytes = []
        for i in range(0, n_s, 2):
            s0 = (int(decoded_bits[i*4])   << 3 | int(decoded_bits[i*4+1]) << 2 |
                  int(decoded_bits[i*4+2]) << 1 | int(decoded_bits[i*4+3]))
            s1 = (int(decoded_bits[(i+1)*4])   << 3 | int(decoded_bits[(i+1)*4+1]) << 2 |
                  int(decoded_bits[(i+1)*4+2]) << 1 | int(decoded_bits[(i+1)*4+3]))
            decoded_bytes.append((s0 << 4) | s1)
    decoded_bytes = np.array(decoded_bytes, dtype=np.uint8)

    # Compare with original
    if not os.path.exists(IN_FILE):
        return decoded_bytes, None, None

    original = np.frombuffer(open(IN_FILE, 'rb').read(), dtype=np.uint8)
    if len(decoded_bytes) != len(original):
        return decoded_bytes, -1, -1

    byte_errors = int(np.sum(decoded_bytes != original))
    diff_xor    = decoded_bytes ^ original
    bit_errors  = int(sum(bin(int(x)).count('1') for x in diff_xor))
    return decoded_bytes, byte_errors, bit_errors


# ── Quantise to ap_fixed<16,1> range ─────────────────────────────────
def quantise_iq(sig):
    """Simulate ap_fixed<16,1>: clamp to [-1, 1-Q_STEP], round to grid."""
    q = np.round(sig / Q_STEP) * Q_STEP
    return np.clip(q, -(1.0), 1.0 - Q_STEP)


# ── Theoretical BER (AWGN) ────────────────────────────────────────────
def qpsk_ber_theory(snr_db):
    """QPSK theoretical BER in AWGN: 0.5*erfc(sqrt(SNR_per_bit))."""
    snr_lin = 10**(snr_db / 10.0)
    # QPSK: 2 bits/symbol, each axis treated independently
    # Eb/N0 = SNR_symbol / 2  (SNR here is per complex sample)
    eb_n0 = snr_lin / 2.0
    return 0.5 * erfc(np.sqrt(eb_n0))


def qam16_ber_theory(snr_db):
    """16-QAM approximate theoretical BER in AWGN."""
    snr_lin = 10**(snr_db / 10.0)
    # Eb/N0 = SNR_symbol / 4  (4 bits/symbol)
    eb_n0 = snr_lin / 4.0
    return 0.75 * erfc(np.sqrt(0.4 * eb_n0))


# ── Phase noise ───────────────────────────────────────────────────────
def add_phase_noise(signal, sigma_rad_per_sample=0.005, rng=None):
    """
    Wiener-process phase noise — models oscillator phase drift.
    sigma=0.005 rad/sample : decent TCXO at 2.4 GHz (mild impairment)
    sigma=0.020 rad/sample : poor crystal (stress test)

    CPE tracking (per symbol) corrects slow inter-symbol drift.
    Fast intra-symbol phase variation causes ICI — not correctable.
    """
    if rng is None:
        rng = np.random.default_rng()
    phase_walk = np.cumsum(rng.normal(0, sigma_rad_per_sample, len(signal)))
    return signal * np.exp(1j * phase_walk)


# ── Multipath ─────────────────────────────────────────────────────────
def add_multipath(signal, taps=None):
    """
    FIR multipath channel.  taps = list of (delay_samples, complex_amplitude).
    Default 2-tap UAV model:
      (1, 0.30+0j) : ground reflection ~ -10 dB, 50 ns at 20 MSPS
      (3, 0.15+0j) : structure reflection ~ -16 dB, 150 ns at 20 MSPS
    Both delays are < CP_LEN=32, so no ISI — only per-SC amplitude/phase distortion
    corrected by channel estimation.
    """
    if taps is None:
        taps = [(1, 0.30 + 0j), (3, 0.15 + 0j)]
    result = signal.copy().astype(complex)
    for delay, amp in taps:
        if 0 < delay < len(signal):
            result[delay:] += signal[:-delay] * amp
    return result


# ── CFO injection ─────────────────────────────────────────────────────
def add_cfo(signal, cfo_sc, fft_size=FFT_SIZE):
    """
    Inject a carrier frequency offset of cfo_sc subcarrier spacings.
    Phase rotation per sample = 2π × cfo_sc / FFT_SIZE.
    sync_detect Schmidl-Cox estimates fractional CFO up to ±0.5 SC.
    Use cfo_sc=0.3 for a realistic AD9364 crystal offset at 2.4 GHz.
    """
    n = np.arange(len(signal))
    return signal * np.exp(1j * 2.0 * np.pi * cfo_sc / fft_size * n)


# ── Combined channel ──────────────────────────────────────────────────
def apply_channel(signal, channel='awgn', snr_db=20.0, rng=None,
                  phase_sigma=0.005, cfo_sc=0.3, multipath_taps=None):
    """
    Apply impairments in physical order: CFO → multipath → AWGN → phase noise.

    channel options:
      'awgn'      : AWGN only (baseline)
      'phase'     : AWGN + phase noise
      'multipath' : AWGN + multipath (no phase noise)
      'cfo'       : CFO + AWGN  (tests sync_detect + cfo_correct)
      'combined'  : CFO + multipath + AWGN + phase noise (worst case)
    """
    if rng is None:
        rng = np.random.default_rng()

    sig = signal.copy()

    if channel in ('cfo', 'combined'):
        sig = add_cfo(sig, cfo_sc)

    if channel in ('multipath', 'combined'):
        sig = add_multipath(sig, multipath_taps)

    # AWGN applied for all channel types
    sig = add_awgn(sig, snr_db, rng)

    if channel in ('phase', 'combined'):
        sig = add_phase_noise(sig, phase_sigma, rng)

    return sig


# ── Main functions ────────────────────────────────────────────────────
def write_noisy(input_file, snr_db, mod=MOD, n_syms=N_SYMS, seed=0,
                channel='awgn', phase_sigma=0.005, cfo_sc=0.3,
                multipath_taps=None):
    """
    Apply channel impairment at snr_db and write tb_tx_output_hls_noise.txt.
    Output is quantised to ap_fixed<16,1> to match HLS RX input format.
    """
    if not os.path.exists(input_file):
        print(f"[CHN] ERROR: {input_file} not found")
        return

    raw = np.loadtxt(input_file, dtype=float)
    sig = raw[:, 0] + 1j * raw[:, 1]

    rng = np.random.default_rng(seed)
    noisy = apply_channel(sig, channel=channel, snr_db=snr_db, rng=rng,
                          phase_sigma=phase_sigma, cfo_sc=cfo_sc,
                          multipath_taps=multipath_taps)

    # Quantise to ap_fixed<16,1> range
    noisy_i = quantise_iq(noisy.real)
    noisy_q = quantise_iq(noisy.imag)

    with open(NOISY_FILE, 'w') as f:
        for i, q in zip(noisy_i, noisy_q):
            f.write(f"{i:.8f} {q:.8f}\n")

    print(f"[CHN] Wrote {len(noisy_i)} noisy samples to {NOISY_FILE}  "
          f"(SNR = {snr_db} dB, channel = {channel})")

    # Quick Python decode check on the noisy signal
    noisy_c = noisy_i + 1j * noisy_q
    _, _, bit_errors = python_rx(noisy_c, mod=mod, n_syms=n_syms)
    total_bits = n_syms * NUM_DATA_SC * (2 if mod == 0 else 4)
    if bit_errors is not None and bit_errors >= 0:
        ber = bit_errors / total_bits
        print(f"[CHN] Python BER check: {bit_errors}/{total_bits} bit errors  "
              f"BER = {ber:.2e}")
    else:
        print("[CHN] Python BER check: skipped (reference file not found)")


def sweep(input_file, snr_range_db, mod=MOD, n_syms=N_SYMS,
          n_trials=5, seed=42, channel='awgn',
          phase_sigma=0.005, cfo_sc=0.3, multipath_taps=None):
    """
    Sweep SNR and report empirical BER vs theoretical BER.

    n_trials: number of independent noise realizations per SNR point
              (averaged to reduce variance with small packet count).
    """
    if not os.path.exists(input_file):
        print(f"[SWP] ERROR: {input_file} not found")
        return

    raw = np.loadtxt(input_file, dtype=float)
    sig = raw[:, 0] + 1j * raw[:, 1]

    total_bits = n_syms * NUM_DATA_SC * (2 if mod == 0 else 4)

    mod_name = "QPSK" if mod == 0 else "16-QAM"
    print(f"\n[SWP] SNR sweep — {mod_name}, channel={channel}, {n_syms} symbols × "
          f"{NUM_DATA_SC} data SC × {2 if mod==0 else 4} bps")
    print(f"[SWP] {n_trials} trials/point, {total_bits} bits/trial")
    print(f"\n{'SNR(dB)':>8}  {'Emp BER':>10}  {'Theory BER':>12}  "
          f"{'Bit errs':>9}  {'Total bits':>10}")
    print("-" * 58)

    rng = np.random.default_rng(seed)

    results = []
    for snr_db in snr_range_db:
        total_bit_errors = 0
        total_bits_tested = 0

        for trial in range(n_trials):
            noisy = apply_channel(sig, channel=channel, snr_db=snr_db, rng=rng,
                                  phase_sigma=phase_sigma, cfo_sc=cfo_sc,
                                  multipath_taps=multipath_taps)
            noisy_q = quantise_iq(noisy.real) + 1j * quantise_iq(noisy.imag)
            _, _, be = python_rx(noisy_q, mod=mod, n_syms=n_syms)
            if be is not None and be >= 0:
                total_bit_errors  += be
                total_bits_tested += total_bits

        if total_bits_tested == 0:
            print(f"{snr_db:>8.1f}  {'N/A':>10}  {'N/A':>12}")
            continue

        emp_ber    = total_bit_errors / total_bits_tested
        theory_ber = qpsk_ber_theory(snr_db) if mod == 0 else qam16_ber_theory(snr_db)

        print(f"{snr_db:>8.1f}  {emp_ber:>10.2e}  {theory_ber:>12.2e}  "
              f"{total_bit_errors:>9}  {total_bits_tested:>10}")
        results.append((snr_db, emp_ber, theory_ber))

    # Write CSV
    csv_file = f"ber_vs_snr_{channel}.csv"
    with open(csv_file, 'w') as f:
        f.write("snr_db,empirical_ber,theory_ber,channel\n")
        for snr_db, emp, th in results:
            f.write(f"{snr_db:.1f},{emp:.6e},{th:.6e},{channel}\n")
    print(f"\n[SWP] Results written to {csv_file}")

    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="OFDM AWGN channel simulator")
    parser.add_argument("--input",       default=HLS_FILE,
                        help=f"Input IQ signal file (default: {HLS_FILE})")
    parser.add_argument("--snr",         type=float, default=20.0,
                        help="SNR in dB for --write-noisy (default: 20)")
    parser.add_argument("--write-noisy", action="store_true",
                        help=f"Write AWGN-corrupted signal to {NOISY_FILE}")
    parser.add_argument("--sweep",       action="store_true",
                        help="Sweep SNR from 0 to 30 dB and print BER table")
    parser.add_argument("--snr-min",      type=float, default=0.0)
    parser.add_argument("--snr-max",      type=float, default=30.0)
    parser.add_argument("--snr-step",     type=float, default=2.0)
    parser.add_argument("--trials",       type=int,   default=5,
                        help="Noise trials per SNR point for sweep (default: 5)")
    parser.add_argument("--mod",          type=int,   default=MOD,
                        help="0=QPSK 1=16QAM")
    parser.add_argument("--nsyms", "--n-syms", dest="nsyms",
                        type=int, default=N_SYMS, help="Number of data symbols [1..255]")
    parser.add_argument("--seed",         type=int,   default=42)
    # Channel type and impairment parameters
    parser.add_argument("--channel",      default="awgn",
                        choices=["awgn", "phase", "multipath", "cfo", "combined"],
                        help="Channel model (default: awgn)")
    parser.add_argument("--phase-sigma",  type=float, default=0.005,
                        help="Phase noise sigma rad/sample (default: 0.005)")
    parser.add_argument("--cfo-sc",       type=float, default=0.3,
                        help="CFO in subcarrier units (default: 0.3)")
    args = parser.parse_args()

    if not args.write_noisy and not args.sweep:
        parser.print_help()
        raise SystemExit(0)

    if args.write_noisy:
        write_noisy(args.input, args.snr, mod=args.mod,
                    n_syms=args.nsyms, seed=args.seed,
                    channel=args.channel,
                    phase_sigma=args.phase_sigma,
                    cfo_sc=args.cfo_sc)

    if args.sweep:
        snr_range = list(np.arange(args.snr_min, args.snr_max + 0.001,
                                   args.snr_step))
        sweep(args.input, snr_range, mod=args.mod,
              n_syms=args.nsyms, n_trials=args.trials, seed=args.seed,
              channel=args.channel,
              phase_sigma=args.phase_sigma,
              cfo_sc=args.cfo_sc)
