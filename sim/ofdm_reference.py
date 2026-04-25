"""
ofdm_reference.py  —  Floating-point OFDM TX reference model + testbench driver

Steps:
  1.  Generate random bits, pack to bytes, write  tb_input_to_tx.bin
  2.  Run the same OFDM TX in Python (numpy) → floating-point "golden" output
  3.  Load HLS C-sim output from  tb_tx_output_hls.txt  (written by ofdm_tx_tb.cpp)
  4.  Compare: report per-sample error and EVM

Usage:
  # Step A: generate inputs and reference output
  python3 ofdm_reference.py --gen

  # Step B: after running Vitis HLS C-simulation, compare outputs
  python3 ofdm_reference.py --compare

  # Both steps at once (if HLS output already exists):
  python3 ofdm_reference.py --gen --compare
"""

import numpy as np
import struct
import argparse
import os

# ── Parameters  (must match ofdm_tx.h exactly) ──────────────────
FFT_SIZE     = 256
CP_LEN       = 32
NUM_DATA_SC  = 200
NUM_PILOT_SC = 6
PILOT_IDX    = [50, 75, 100, 154, 179, 204]

DATA_SC_IDX = (
    list(range(25, 50)) + list(range(51, 75)) +
    list(range(76, 100)) + list(range(101, 128)) +
    list(range(129, 154)) + list(range(155, 179)) +
    list(range(180, 204)) + list(range(205, 232))
)
assert len(DATA_SC_IDX) == NUM_DATA_SC

# Test config
N_SYMS    = 255
MOD       = 1    # 0 = QPSK, 1 = 16QAM
FEC_RATE  = 0    # 0 = rate 1/2, 1 = rate 2/3

from fec_reference import conv_encode, viterbi_decode as fec_decode

IN_FILE  = "tb_input_to_tx.bin"
REF_FILE = "tb_tx_output_ref.txt"
HLS_FILE = "tb_tx_output_hls.txt"

# ── Modulation tables ────────────────────────────────────────────
# QPSK: 2-bit index → complex symbol (Gray coded)
QPSK_TABLE = np.array([
    ( 0.7071 + 0.7071j),   # 00
    ( 0.7071 - 0.7071j),   # 01
    (-0.7071 + 0.7071j),   # 10
    (-0.7071 - 0.7071j),   # 11
])

# 16-QAM: 4-bit index → complex symbol (Gray coded, normalized)
def _qam16_val(two_bits):
    """Gray: 00→+3/√10  01→+1/√10  11→-1/√10  10→-3/√10"""
    table = {0: 3/np.sqrt(10), 1: 1/np.sqrt(10),
             3: -1/np.sqrt(10), 2: -3/np.sqrt(10)}
    return table[two_bits]

QAM16_TABLE = np.array([
    complex(_qam16_val((i >> 2) & 0x3), _qam16_val(i & 0x3))
    for i in range(16)
])

# ── ZC preamble ──────────────────────────────────────────────────
def zc_sequence(N=256, u=25):
    """Zadoff-Chu sequence, root u, length N."""
    k = np.arange(N)
    return np.exp(-1j * np.pi * u * k * (k + 1) / N)

# ── OFDM Symbol builder ──────────────────────────────────────────
def build_ofdm_symbol(freq_domain):
    """
    IFFT + cyclic prefix.
    freq_domain: length FFT_SIZE complex array (frequency bins).
    Returns: length (FFT_SIZE + CP_LEN) complex array (time samples).
    """
    time_domain = np.fft.ifft(freq_domain)
    cp           = time_domain[-CP_LEN:]
    return np.concatenate([cp, time_domain])

def crc16_hdr(payload_10bit):
    """CRC-16/CCITT (poly=0x1021, init=0xFFFF) over a 10-bit payload — matches HLS."""
    crc = 0xFFFF
    for i in range(9, -1, -1):
        x = ((crc >> 15) & 1) ^ ((payload_10bit >> i) & 1)
        crc = (crc << 1) & 0xFFFF
        if x:
            crc ^= 0x1021
    return crc

def build_header_symbol(mod, n_syms, fec_rate=0):
    """Build BPSK frame-header symbol (26 bits on DATA_SC_IDX[0..25]).

    Payload layout (matches HLS src/ofdm_tx.cpp:485 + ofdm_tx.h:54):
      bits[9:2] = n_syms[7:0]
      bits[1:0] = modcod = {mod, rate}   ← modcod[1]=mod, modcod[0]=rate
    """
    modcod  = ((mod & 1) << 1) | (fec_rate & 1)
    payload = (n_syms << 2) | (modcod & 0x3)
    crc     = crc16_hdr(payload)
    hdr     = (crc << 10) | payload   # 26-bit word

    freq = np.zeros(FFT_SIZE, dtype=complex)
    for p in PILOT_IDX:
        freq[p] = 0.999969 + 0j
    for d in range(26):
        bit = (hdr >> (25 - d)) & 1   # MSB first
        freq[DATA_SC_IDX[d]] = (-0.999969 if bit else 0.999969) + 0j
    # DATA_SC_IDX[26..199] stay zero
    return build_ofdm_symbol(freq)

def build_preamble():
    """Build ZC preamble symbol."""
    freq = np.zeros(FFT_SIZE, dtype=complex)
    zc   = zc_sequence()

    # Pilots: BPSK +1
    for p in PILOT_IDX:
        freq[p] = 0.999969 + 0j   # ap_fixed<16,1> max: 1 - 2^-15

    # Data subcarriers: ZC values
    for idx, sc in enumerate(DATA_SC_IDX):
        freq[sc] = zc[sc]

    return build_ofdm_symbol(freq)

def build_data_symbol(bits_2d, mod):
    """
    bits_2d: (NUM_DATA_SC, bps) uint8 array of per-subcarrier bits.
    mod:     0=QPSK, 1=16QAM.
    Returns: length (FFT_SIZE + CP_LEN) complex array.
    """
    freq = np.zeros(FFT_SIZE, dtype=complex)

    # Pilots
    for p in PILOT_IDX:
        freq[p] = 0.999969 + 0j   # ap_fixed<16,1> max: 1 - 2^-15

    # Data subcarriers
    for d, sc in enumerate(DATA_SC_IDX):
        if mod == 0:
            idx  = (int(bits_2d[d, 0]) << 1) | int(bits_2d[d, 1])
            freq[sc] = QPSK_TABLE[idx]
        else:
            idx  = (int(bits_2d[d, 0]) << 3 | int(bits_2d[d, 1]) << 2 |
                    int(bits_2d[d, 2]) << 1 | int(bits_2d[d, 3]))
            freq[sc] = QAM16_TABLE[idx]

    return build_ofdm_symbol(freq)

# ── Fixed-point quantisation model ───────────────────────────────
Q_FRAC_BITS = 15        # ap_fixed<16,1> has 15 fractional bits
Q_STEP      = 2**-Q_FRAC_BITS

def quantise(x):
    """Simulate ap_fixed<16,1> AP_RND AP_SAT rounding and saturation."""
    x_r = np.round(x / Q_STEP) * Q_STEP
    return np.clip(x_r, -1.0 + Q_STEP, 1.0 - Q_STEP)

def quantise_complex(c):
    return quantise(c.real) + 1j * quantise(c.imag)

# ── Pack/unpack helpers ──────────────────────────────────────────
def bits_to_bytes_qpsk(bits_flat):
    """Pack 1-D array of bits (length = N_SYMS*NUM_DATA_SC*2) into bytes.
    4 QPSK symbols per byte: [b1,b0, b3,b2, b5,b4, b7,b6] in bits [7:0].
    """
    n_symbols = len(bits_flat) // 2
    assert n_symbols % 4 == 0, "need multiple of 4 symbols for byte packing"
    out = []
    for i in range(0, n_symbols, 4):
        s0 = (int(bits_flat[i*2])   << 1) | int(bits_flat[i*2+1])
        s1 = (int(bits_flat[(i+1)*2]) << 1) | int(bits_flat[(i+1)*2+1])
        s2 = (int(bits_flat[(i+2)*2]) << 1) | int(bits_flat[(i+2)*2+1])
        s3 = (int(bits_flat[(i+3)*2]) << 1) | int(bits_flat[(i+3)*2+1])
        out.append((s0 << 6) | (s1 << 4) | (s2 << 2) | s3)
    return bytes(out)

def bits_to_bytes_16qam(bits_flat):
    """Pack 1-D array of bits (length = N_SYMS*NUM_DATA_SC*4) into bytes.
    2 16QAM symbols per byte: upper nibble = sym0, lower nibble = sym1.
    """
    n_symbols = len(bits_flat) // 4
    assert n_symbols % 2 == 0, "need even number of symbols"
    out = []
    for i in range(0, n_symbols, 2):
        s0 = (int(bits_flat[i*4])   << 3 | int(bits_flat[i*4+1]) << 2 |
              int(bits_flat[i*4+2]) << 1 | int(bits_flat[i*4+3]))
        s1 = (int(bits_flat[(i+1)*4])   << 3 | int(bits_flat[(i+1)*4+1]) << 2 |
              int(bits_flat[(i+1)*4+2]) << 1 | int(bits_flat[(i+1)*4+3]))
        out.append((s0 << 4) | s1)
    return bytes(out)

# ── Scrambler (802.11a, matches scrambler.cpp) ───────────────────
# 7-stage LFSR, poly x⁷+x⁴+1, fixed seed 0x7F.
# TX and RX both start with the same seed → descramble = scramble.
def py_scramble(data_bytes):
    lfsr = 0x7F
    out  = bytearray()
    for byte in data_bytes:
        out_byte = 0
        for b in range(7, -1, -1):   # MSB first (bit 7 down to 0)
            feedback  = ((lfsr >> 6) & 1) ^ ((lfsr >> 3) & 1)
            out_byte |= (((byte >> b) & 1) ^ feedback) << b
            lfsr      = ((lfsr << 1) & 0x7F) | feedback
        out.append(out_byte)
    return bytes(out)

# ── Interleaver TX permutation (matches interleaver.cpp is_rx=0) ─
def _src_qpsk_tx(j):
    return 16 * (j % 25) + j // 25

def _src_16qam_tx(j):
    c   = j // 50
    r   = j  % 50
    k_r = (r + 1 if r % 2 == 0 else r - 1) if c % 2 == 1 else r
    return 16 * k_r + c

def _src_qpsk_rx(j):
    return 25 * (j % 16) + j // 16

def _src_16qam_rx(j):
    c = j % 16
    r = j // 16
    k = 50 * c + r
    return (k + 1 if r % 2 == 0 else k - 1) if c % 2 == 1 else k

def _apply_interleave_perm(coded_bytes, mod, n_syms, src_fn):
    """Generic permutation engine used by both interleave and deinterleave."""
    bytes_per_sym = 50  if mod == 0 else 100
    ncbps         = 400 if mod == 0 else 800
    out = bytearray()
    for s in range(n_syms):
        sym_bytes = coded_bytes[s * bytes_per_sym : (s + 1) * bytes_per_sym]
        bits = []
        for byte in sym_bytes:
            for b in range(7, -1, -1):
                bits.append((byte >> b) & 1)
        out_bits = [bits[src_fn(j)] for j in range(ncbps)]
        for i in range(0, ncbps, 8):
            byte = 0
            for b in range(8):
                byte = (byte << 1) | out_bits[i + b]
            out.append(byte)
    return bytes(out)

def py_interleave(coded_bytes, mod, n_syms):
    """TX interleaver — matches HLS interleaver(is_rx=0)."""
    src_fn = _src_qpsk_tx if mod == 0 else _src_16qam_tx
    return _apply_interleave_perm(coded_bytes, mod, n_syms, src_fn)

def py_deinterleave(coded_bytes, mod, n_syms):
    """RX deinterleaver — matches HLS interleaver(is_rx=1)."""
    src_fn = _src_qpsk_rx if mod == 0 else _src_16qam_rx
    return _apply_interleave_perm(coded_bytes, mod, n_syms, src_fn)

# ── Main ─────────────────────────────────────────────────────────
def generate(mod=MOD, n_syms=N_SYMS, fec_rate=FEC_RATE):
    """Generate random raw bits, FEC encode, write packed coded bytes and TX reference."""
    bps = 2 if mod == 0 else 4

    # Total coded bytes that OFDM TX will consume
    coded_per_sym = NUM_DATA_SC // 4 if mod == 0 else NUM_DATA_SC // 2
    total_coded   = n_syms * coded_per_sym

    # Raw data bytes (before FEC)
    total_raw = total_coded // 2 if fec_rate == 0 else total_coded * 2 // 3

    # Generate random raw bytes
    rng = np.random.default_rng(seed=42)
    raw_bytes = bytes(rng.integers(0, 256, total_raw, dtype=np.uint8))

    # Write raw bytes to file — this is the ground truth for BER comparison
    with open(IN_FILE, 'wb') as f:
        f.write(raw_bytes)
    print(f"[REF] Wrote {len(raw_bytes)} bytes to {IN_FILE}")

    # Scramble → FEC encode → interleave  (matches ofdm_tx_tb.cpp chain)
    scrambled_bytes = py_scramble(raw_bytes)
    coded_bytes     = conv_encode(scrambled_bytes, fec_rate)
    assert len(coded_bytes) == total_coded, \
        f"FEC output size mismatch: {len(coded_bytes)} != {total_coded}"
    interleaved_bytes = py_interleave(coded_bytes, mod, n_syms)

    # Unpack interleaved bytes to per-SC bit arrays for OFDM symbol building
    coded_flat = []
    for b in interleaved_bytes:
        for p in range(7, -1, -1):
            coded_flat.append((b >> p) & 1)
    coded_flat = np.array(coded_flat, dtype=np.uint8)

    # Reshape to (n_syms, NUM_DATA_SC, bps)
    bits = coded_flat.reshape(n_syms, NUM_DATA_SC, bps)

    # Build floating-point reference output (using FEC-coded bits)
    all_samples = []

    # Preamble
    preamble = build_preamble()
    all_samples.append(preamble)

    # Header symbol
    header = build_header_symbol(mod, n_syms, fec_rate)
    all_samples.append(header)

    # Data symbols
    for s in range(n_syms):
        sym = build_data_symbol(bits[s], mod)
        all_samples.append(sym)

    signal = np.concatenate(all_samples)

    # Write reference (apply same quantisation as HLS Q1.14 output)
    with open(REF_FILE, 'w') as f:
        for samp in signal:
            qi = quantise(samp.real)
            qq = quantise(samp.imag)
            f.write(f"{qi:.8f} {qq:.8f}\n")

    expected = (n_syms + 2) * (FFT_SIZE + CP_LEN)
    print(f"[REF] Wrote {len(signal)} samples to {REF_FILE} "
          f"(expected {expected})")

    # Quick sanity: PAPR of preamble (should be low for ZC)
    pwr_zc = np.abs(preamble[CP_LEN:])**2     # symbol body only
    papr_db = 10*np.log10(np.max(pwr_zc) / np.mean(pwr_zc))
    print(f"[REF] ZC preamble PAPR = {papr_db:.2f} dB "
          f"(ideal ZC ≈ 0 dB, expect ~1–3 dB after fixed-pt IFFT scaling)")

    return signal

def compare():
    """Load HLS output and reference, compute error metrics."""
    if not os.path.exists(HLS_FILE):
        print(f"[CMP] {HLS_FILE} not found — run HLS C-simulation first")
        return

    ref  = np.loadtxt(REF_FILE,  dtype=float)
    hls  = np.loadtxt(HLS_FILE,  dtype=float)

    ref_c = ref[:, 0]  + 1j * ref[:, 1]
    hls_c = hls[:, 0]  + 1j * hls[:, 1]

    # C2 fix: HLS TX output has one guard symbol (288 samples) prepended before
    # the preamble; Python reference has no guard.  Strip it before comparing.
    sym_len = FFT_SIZE + CP_LEN  # 288
    if len(hls_c) == len(ref_c) + sym_len:
        hls_c = hls_c[sym_len:]

    if len(ref_c) != len(hls_c):
        print(f"[CMP] FAIL: length mismatch — ref={len(ref_c)} hls={len(hls_c)}")
        return

    err    = hls_c - ref_c
    rms_e  = np.sqrt(np.mean(np.abs(err)**2))
    rms_s  = np.sqrt(np.mean(np.abs(ref_c)**2))
    evm_pct = 100.0 * rms_e / rms_s

    max_err = np.max(np.abs(err))

    print(f"[CMP] Samples    : {len(ref_c)}")
    print(f"[CMP] RMS error  : {rms_e:.6f}  (quantisation noise floor)")
    print(f"[CMP] Max error  : {max_err:.6f}")
    print(f"[CMP] EVM        : {evm_pct:.3f}%")
    print(f"[CMP] EVM (dB)   : {20*np.log10(evm_pct/100):.2f} dB")

    # Pass/fail: EVM < 5% is a reasonable threshold for QPSK/16QAM
    # (expected ~1% for fixed-point Q1.14 vs float)
    threshold = 5.0
    if evm_pct < threshold:
        print(f"[CMP] PASS  (EVM < {threshold}%)")
    else:
        print(f"[CMP] FAIL  (EVM >= {threshold}%)")

    # Per-section breakdown
    sym_len = FFT_SIZE + CP_LEN   # 288
    n_sections = len(ref_c) // sym_len
    print(f"\n[CMP] Per-symbol EVM breakdown:")
    for i in range(n_sections):
        sl  = slice(i * sym_len, (i+1) * sym_len)
        e_i = np.sqrt(np.mean(np.abs(hls_c[sl] - ref_c[sl])**2))
        s_i = np.sqrt(np.mean(np.abs(ref_c[sl])**2))
        ev  = 100.0 * e_i / s_i
        label = "PREAMBLE" if i == 0 else ("HEADER" if i == 1 else f"DATA[{i-2}]")
        print(f"  {label:12s}  EVM = {ev:.3f}%")

# ── Reference Decoder ────────────────────────────────────────────
# Decodes an OFDM signal (from tx_file) using numpy FFT and the known
# ZC preamble for channel estimation.  Compares decoded bytes with
# tb_input_to_tx.bin and reports BER.
#
# numpy.fft.fft is the unnormalized DFT.
# numpy.fft.ifft is ÷N (what the TX used).
# Round-trip: fft(ifft(X)) = X  → no scale adjustment needed here.
# ─────────────────────────────────────────────────────────────────
def decode_axis_16qam(v):
    """16-QAM single-axis decision: returns 2-bit Gray code."""
    thresh = 2.0 / np.sqrt(10)
    if   v >= thresh:  return 0b00
    elif v >= 0:       return 0b01
    elif v >= -thresh: return 0b11
    else:              return 0b10

def decode(tx_file=None, mod=MOD, n_syms=N_SYMS, fec_rate=FEC_RATE, guard_syms=0):
    """
    Decode an OFDM signal file and compare with original bits.

    tx_file:    path to IQ signal file (format: "I Q" per line).
                Defaults to REF_FILE (floating-point TX reference).
    guard_syms: number of guard (zero) symbols prepended before the preamble.
                Set to 1 when decoding HLS TX output (C2 fix adds one guard symbol).
    """
    if tx_file is None:
        tx_file = REF_FILE

    if not os.path.exists(tx_file):
        print(f"[DEC] {tx_file} not found")
        return

    sig = np.loadtxt(tx_file, dtype=float)
    sig_c = sig[:, 0] + 1j * sig[:, 1]

    sym_len   = FFT_SIZE + CP_LEN  # 288
    guard_len = guard_syms * sym_len
    expected_samps = (n_syms + 2 + guard_syms) * sym_len  # guard + preamble + header + data
    if len(sig_c) < expected_samps:
        print(f"[DEC] FAIL: signal too short ({len(sig_c)} < {expected_samps})")
        return

    zc = zc_sequence()

    # ── Preamble: channel estimation ─────────────────────────
    preamble_body = sig_c[guard_len + CP_LEN : guard_len + sym_len]   # skip guard + CP
    Y_pre = np.fft.fft(preamble_body)                # unnormalized DFT
    # G[k] = Y_pre[k] / ZC[k] = Y_pre[k] * conj(ZC[k]) (|ZC|=1)
    G = np.zeros(FFT_SIZE, dtype=complex)
    for d, k in enumerate(DATA_SC_IDX):
        G[k] = Y_pre[k] * np.conj(zc[k])
    # G[k] ≈ 1.0 in perfect channel (fft is unnormalized, ifft is ÷N → Y=X)

    # Symbol 1 is the header — skip it, data starts at symbol index 2.

    # ── Decode data symbols ───────────────────────────────────
    bps = 2 if mod == 0 else 4
    decoded_bits = []

    for s in range(n_syms):
        offset  = guard_len + sym_len * (s + 2)   # skip guard + preamble + header
        sym_iq  = sig_c[offset + CP_LEN : offset + sym_len]
        Y = np.fft.fft(sym_iq)

        # Equalize: X_hat = Y * conj(G) / |G|²
        for d, k in enumerate(DATA_SC_IDX):
            mag2 = abs(G[k])**2
            X_hat = Y[k] * np.conj(G[k]) / mag2 if mag2 > 1e-12 else 0 + 0j

            if mod == 0:  # QPSK
                b1 = 1 if X_hat.real < 0 else 0
                b0 = 1 if X_hat.imag < 0 else 0
                decoded_bits.extend([b1, b0])
            else:         # 16-QAM
                i_bits = decode_axis_16qam(X_hat.real)
                q_bits = decode_axis_16qam(X_hat.imag)
                b3 = (i_bits >> 1) & 1
                b2 =  i_bits       & 1
                b1 = (q_bits >> 1) & 1
                b0 =  q_bits       & 1
                decoded_bits.extend([b3, b2, b1, b0])

    # ── Pack decoded bits into bytes (same format as TX) ──────
    decoded_bits = np.array(decoded_bits, dtype=np.uint8)
    if mod == 0:
        n_syms_total = len(decoded_bits) // 2
        decoded_bytes = []
        for i in range(0, n_syms_total, 4):
            s0 = (int(decoded_bits[i*2])     << 1) | int(decoded_bits[i*2+1])
            s1 = (int(decoded_bits[(i+1)*2]) << 1) | int(decoded_bits[(i+1)*2+1])
            s2 = (int(decoded_bits[(i+2)*2]) << 1) | int(decoded_bits[(i+2)*2+1])
            s3 = (int(decoded_bits[(i+3)*2]) << 1) | int(decoded_bits[(i+3)*2+1])
            decoded_bytes.append((s0 << 6) | (s1 << 4) | (s2 << 2) | s3)
    else:
        n_syms_total = len(decoded_bits) // 4
        decoded_bytes = []
        for i in range(0, n_syms_total, 2):
            s0 = (int(decoded_bits[i*4])   << 3 | int(decoded_bits[i*4+1]) << 2 |
                  int(decoded_bits[i*4+2]) << 1 | int(decoded_bits[i*4+3]))
            s1 = (int(decoded_bits[(i+1)*4])   << 3 | int(decoded_bits[(i+1)*4+1]) << 2 |
                  int(decoded_bits[(i+1)*4+2]) << 1 | int(decoded_bits[(i+1)*4+3]))
            decoded_bytes.append((s0 << 4) | s1)

    decoded_bytes = np.array(decoded_bytes, dtype=np.uint8)

    # ── FEC decode: coded bytes → raw data bytes ─────────────
    coded_per_sym = NUM_DATA_SC // 4 if mod == 0 else NUM_DATA_SC // 2
    total_coded   = n_syms * coded_per_sym
    total_raw     = total_coded // 2 if fec_rate == 0 else total_coded * 2 // 3

    coded_packed   = bytes(decoded_bytes)
    deinterleaved  = py_deinterleave(coded_packed, mod, n_syms)   # undo TX interleaver
    raw_decoded    = fec_decode(deinterleaved, fec_rate, total_raw)
    raw_decoded    = py_scramble(raw_decoded)                      # descramble (self-inverse)
    decoded_bytes  = np.frombuffer(raw_decoded, dtype=np.uint8)

    # ── Compare with original raw bytes ──────────────────────
    if not os.path.exists(IN_FILE):
        print(f"[DEC] {IN_FILE} not found — cannot compute BER")
        return

    original = np.frombuffer(open(IN_FILE, 'rb').read(), dtype=np.uint8)

    if len(decoded_bytes) != len(original):
        print(f"[DEC] FAIL: length mismatch — decoded={len(decoded_bytes)} "
              f"original={len(original)}")
        return

    byte_errors = int(np.sum(decoded_bytes != original))
    diff_xor    = decoded_bytes ^ original
    bit_errors  = int(sum(bin(x).count('1') for x in diff_xor))
    total_bits  = len(original) * 8
    ber         = bit_errors / total_bits

    print(f"[DEC] Signal file  : {tx_file}")
    print(f"[DEC] Byte errors  : {byte_errors} / {len(original)}")
    print(f"[DEC] Bit  errors  : {bit_errors} / {total_bits}")
    print(f"[DEC] BER          : {ber:.2e}")

    if bit_errors == 0:
        print("[DEC] PASS  — BER = 0")
    else:
        print(f"[DEC] FAIL  — {bit_errors} bit errors")

    return decoded_bytes


def decode_header(tx_file=None, mod=MOD, n_syms=N_SYMS, fec_rate=FEC_RATE, guard_syms=1):
    """
    BPSK-header-only decoder, mirroring HLS src/ofdm_rx.cpp step-by-step.

    Pipeline (matches HLS exactly):
      1. FFT preamble body, compute G[k] = Y_pre[k] * conj(ZC[k]) for data SCs;
         G[k] = Y_pre[k] for pilot SCs.
      2. FFT header body.
      3. compute_pilot_cpe: equalize 6 pilots with G, sum_cos += eq.real,
         sum_sin += eq.imag, phase_err = atan2(sum_sin, sum_cos).
      4. compute_geq_final: G_eq_final = G_eq * exp(-j*phase_err).  (Pure rotation
         in Python — HLS uses CORDIC which has gain K₁₆≈1.647, but BPSK uses sign
         only so K is irrelevant for the decision.)
      5. HDR_DEMAP: hdr_bit[d] = sign(equalize(Y_hdr[k], G_eq_final[k]).real() < 0)
      6. CRC-16 check on the 26-bit word.

    Used to isolate whether HLS's header CRC failure at low SNR is an algorithmic
    issue (Python also fails) or an HLS implementation bug (Python passes).
    """
    if tx_file is None:
        tx_file = HLS_FILE

    if not os.path.exists(tx_file):
        print(f"[HDR] {tx_file} not found")
        return False

    sig = np.loadtxt(tx_file, dtype=float)
    sig_c = sig[:, 0] + 1j * sig[:, 1]

    sym_len   = FFT_SIZE + CP_LEN  # 288
    guard_len = guard_syms * sym_len

    zc = zc_sequence()

    # Step 1: preamble FFT → G
    pre_body = sig_c[guard_len + CP_LEN : guard_len + sym_len]
    Y_pre    = np.fft.fft(pre_body)
    G        = np.zeros(FFT_SIZE, dtype=complex)
    for d, k in enumerate(DATA_SC_IDX):
        G[k] = Y_pre[k] * np.conj(zc[k])
    for k in PILOT_IDX:
        G[k] = Y_pre[k]                        # pilots are +1, so G = Y/X = Y

    # Step 2: header FFT
    hdr_off  = guard_len + sym_len             # skip guard + preamble
    hdr_body = sig_c[hdr_off + CP_LEN : hdr_off + sym_len]
    Y_hdr    = np.fft.fft(hdr_body)

    # Step 3: pilot CPE.  HLS uses ZF equalize: eq = Y * conj(G) / |G|².  G ≈ Y_pre
    # for pilots → eq = Y_hdr * conj(Y_pre) / |Y_pre|² ≈ X_hdr_pilot/X_pre_pilot ≈ 1.
    sum_re = 0.0
    sum_im = 0.0
    for k in PILOT_IDX:
        mag2 = abs(G[k]) ** 2
        eq   = Y_hdr[k] * np.conj(G[k]) / mag2 if mag2 > 1e-12 else 0
        sum_re += eq.real
        sum_im += eq.imag
    phase_err = np.arctan2(sum_im, sum_re)
    print(f"  [HDR] pilot sum: ({sum_re:+.4f}, {sum_im:+.4f})  phase_err = {np.rad2deg(phase_err):+.2f} deg")

    # Step 4: G_eq_final = G_eq * exp(-j*phase_err)
    rot = np.exp(-1j * phase_err)
    G_eq_final = np.zeros(FFT_SIZE, dtype=complex)
    for k in range(FFT_SIZE):
        if abs(G[k]) ** 2 > 1e-12:
            G_eq_k       = np.conj(G[k]) / (abs(G[k]) ** 2)   # ZF G_eq
            G_eq_final[k] = G_eq_k * rot

    # Step 5: demap 26 BPSK bits.  HLS layout: hdr_bits[25-d] = (eq.real < 0)
    hdr_bits = 0
    eq_reals = []
    for d in range(26):
        k  = DATA_SC_IDX[d]
        eq = Y_hdr[k] * G_eq_final[k]
        bit = 1 if eq.real < 0 else 0
        eq_reals.append(eq.real)
        hdr_bits |= (bit << (25 - d))

    # Step 6: CRC check
    rx_crc     = (hdr_bits >> 10) & 0xFFFF
    rx_payload = hdr_bits & 0x3FF
    exp_crc    = crc16_hdr(rx_payload)
    rx_n_syms  = (rx_payload >> 2) & 0xFF
    rx_modcod  = rx_payload & 0x3
    rx_mod     = (rx_modcod >> 1) & 1
    rx_rate    = rx_modcod & 1

    crc_ok = (rx_crc == exp_crc)
    print(f"  [HDR] eq.real range: min={min(eq_reals):+.4f}  max={max(eq_reals):+.4f}  median={sorted(eq_reals)[13]:+.4f}")
    print(f"  [HDR] decoded payload: n_syms={rx_n_syms} mod={rx_mod} rate={rx_rate}  rx_crc=0x{rx_crc:04X}  exp_crc=0x{exp_crc:04X}")
    if crc_ok:
        print(f"  [HDR] PASS — CRC ok, expected n_syms={n_syms} mod={mod} rate={fec_rate}")
    else:
        # Show which expected bits flipped
        exp_payload = (n_syms << 2) | ((mod & 1) << 1) | (fec_rate & 1)
        exp_word    = (crc16_hdr(exp_payload) << 10) | exp_payload
        n_flipped = bin(hdr_bits ^ exp_word).count("1")
        print(f"  [HDR] FAIL — CRC mismatch.  {n_flipped}/26 header bits flipped vs expected")
    return crc_ok


# ============================================================
# HLS-precision-class header decoder
# ============================================================
# Mirrors the HLS chain at the SAME fixed-point widths used in
# src/ofdm_rx.cpp.  Quantization grids:
#   Q15  (LSB = 2⁻¹⁵)   sample_t  ap_fixed<16,1>   freq_buf, G[d] storage
#   Q20  (LSB = 2⁻²⁰)   inv_t     ap_fixed<40,20>  |G|², 1/|G|²
#   Q22  (LSB = 2⁻²²)   geq_t     ap_fixed<32,10>  G_eq, eq result
# Quantization mode = AP_TRN (truncation toward -inf), matches HLS default.
# Multiplications happen in float32 (24-bit mantissa) — wider than any
# of the storage types, mimics HLS's wide intermediate before storage.

_LSB_Q15 = 2.0 ** -15
_LSB_Q20 = 2.0 ** -20
_LSB_Q22 = 2.0 ** -22

def _trn_q(x_real, lsb, lo, hi):
    """ap_fixed AP_TRN (round toward -inf) + saturate to [lo, hi)."""
    q = np.floor(np.asarray(x_real) / lsb) * lsb
    return np.clip(q, lo, hi - lsb)

def _q15(z):
    """Quantize complex array (or scalar) to Q15 grid, range [-1, 1)."""
    z = np.asarray(z, dtype=np.complex128)
    return _trn_q(z.real, _LSB_Q15, -1.0, 1.0) + 1j * _trn_q(z.imag, _LSB_Q15, -1.0, 1.0)

def _q20_real(x):
    """Real Q20 truncation, ap_fixed<40,20> wide range."""
    return _trn_q(x, _LSB_Q20, -(2**20), 2**20)

def _q22(z):
    """Quantize complex to Q22, range [-512, 512)."""
    z = np.asarray(z, dtype=np.complex128)
    return _trn_q(z.real, _LSB_Q22, -512.0, 512.0) + 1j * _trn_q(z.imag, _LSB_Q22, -512.0, 512.0)


def decode_header_q15(tx_file=None, mod=MOD, n_syms=N_SYMS, fec_rate=FEC_RATE, guard_syms=1):
    """
    Same header decode as decode_header(), but with HLS-equivalent
    fixed-point quantization at every storage boundary.

    Diverges from float64 decode_header() only via the quantize_* calls.
    Used to test the precision-cliff theory: if Q15 here reproduces HLS's
    phase_err = 3.09° at 15 dB → precision is the bug.  If Q15 still gives
    Python's 0.45° → bug is elsewhere.
    """
    if tx_file is None:
        tx_file = HLS_FILE
    if not os.path.exists(tx_file):
        print(f"[Q15] {tx_file} not found")
        return False

    sig = np.loadtxt(tx_file, dtype=float)
    sig_c = (sig[:, 0] + 1j * sig[:, 1]).astype(np.complex64)   # input already Q15

    sym_len   = FFT_SIZE + CP_LEN
    guard_len = guard_syms * sym_len
    zc = zc_sequence().astype(np.complex64)
    # ZC LUT in HLS is sample_t — quantize the reference too
    zc = _q15(zc)

    # ── Step 1: preamble FFT, ÷N, store as Q15 (mirrors HLS run_fft + sample_t cast) ──
    pre_body = sig_c[guard_len + CP_LEN : guard_len + sym_len]
    Y_pre    = np.fft.fft(pre_body) / FFT_SIZE
    Y_pre    = _q15(Y_pre)                       # csample_t storage

    # ── Step 2: G[d] = Y_pre × conj(ZC), stored in sample_t (Q15)
    # HLS line 409: result of (wide × wide) cast back to sample_t.
    G = np.zeros(FFT_SIZE, dtype=np.complex128)
    for d, k in enumerate(DATA_SC_IDX):
        g = Y_pre[k] * np.conj(zc[k])
        G[k] = _q15(np.array([g]))[0]            # match HLS Q15 truncation
    for k in PILOT_IDX:
        G[k] = Y_pre[k]                          # already Q15

    # ── Step 3: header FFT, /N, Q15 storage ──
    hdr_off  = guard_len + sym_len
    hdr_body = sig_c[hdr_off + CP_LEN : hdr_off + sym_len]
    Y_hdr    = np.fft.fft(hdr_body) / FFT_SIZE
    Y_hdr    = _q15(Y_hdr)

    # ── Step 4: pilot CPE — for each pilot, |G|² in Q20, inv in Q20, G_eq in Q22, eq in Q22
    sum_re = 0.0
    sum_im = 0.0
    print("  [Q15] per-pilot:")
    for k in PILOT_IDX:
        gre, gim = G[k].real, G[k].imag
        denom    = _q20_real(gre*gre + gim*gim)               # |G|² in inv_t (Q20)
        if denom <= 0:
            continue
        inv      = _q20_real(1.0 / denom)                     # 1/|G|² in inv_t
        # G_eq = conj(G) × inv.  Stored in geq_t (Q22).
        Geq      = _q22(np.array([(gre - 1j*gim) * inv]))[0]  # match HLS line 440
        # eq = Y_hdr × G_eq.  HLS computes in acc_t (Q22) and returns geq_t.
        eq       = _q22(np.array([Y_hdr[k] * Geq]))[0]
        sum_re  += eq.real
        sum_im  += eq.imag
        print(f"    k={k:3d}  G=({gre:+.6f},{gim:+.6f})  |G|²={denom:.3e}  "
              f"inv={inv:.3e}  G_eq=({Geq.real:+.4f},{Geq.imag:+.4f})  "
              f"eq=({eq.real:+.4f},{eq.imag:+.4f})")
    phase_err = np.arctan2(sum_im, sum_re)   # csim path: Python double atan2, same as HLS line 562
    print(f"  [Q15] pilot sum: ({sum_re:+.4f}, {sum_im:+.4f})  phase_err = {np.rad2deg(phase_err):+.3f} deg")

    # ── Step 5: G_eq_final = G_eq × exp(-j·phase_err) (Python uses exp; HLS uses CORDIC with K₁₆ gain
    # but we only need sign for BPSK → K doesn't affect decision)
    rot = np.exp(-1j * phase_err).astype(np.complex64)
    G_eq_final = np.zeros(FFT_SIZE, dtype=np.complex128)
    for d in range(26):
        k = DATA_SC_IDX[d]
        gre, gim = G[k].real, G[k].imag
        denom = _q20_real(gre*gre + gim*gim)
        if denom <= 0:
            continue
        inv  = _q20_real(1.0 / denom)
        Geq  = _q22(np.array([(gre - 1j*gim) * inv]))[0]
        G_eq_final[k] = _q22(np.array([Geq * rot]))[0]

    # ── Step 6: demap 26 bits ──
    hdr_bits = 0
    eq_reals = []
    for d in range(26):
        k  = DATA_SC_IDX[d]
        eq = _q22(np.array([Y_hdr[k] * G_eq_final[k]]))[0]
        bit = 1 if eq.real < 0 else 0
        eq_reals.append(eq.real)
        hdr_bits |= (bit << (25 - d))

    rx_crc     = (hdr_bits >> 10) & 0xFFFF
    rx_payload = hdr_bits & 0x3FF
    exp_crc    = crc16_hdr(rx_payload)
    rx_n_syms  = (rx_payload >> 2) & 0xFF
    rx_mod     = (rx_payload >> 1) & 1
    rx_rate    = rx_payload & 1

    crc_ok = (rx_crc == exp_crc)
    print(f"  [Q15] eq.real range: min={min(eq_reals):+.4f}  max={max(eq_reals):+.4f}  median={sorted(eq_reals)[13]:+.4f}")
    print(f"  [Q15] decoded payload: n_syms={rx_n_syms} mod={rx_mod} rate={rx_rate}  rx_crc=0x{rx_crc:04X}  exp_crc=0x{exp_crc:04X}")
    if crc_ok:
        print(f"  [Q15] PASS — CRC ok")
    else:
        exp_payload = (n_syms << 2) | ((mod & 1) << 1) | (fec_rate & 1)
        exp_word    = (crc16_hdr(exp_payload) << 10) | exp_payload
        n_flipped = bin(hdr_bits ^ exp_word).count("1")
        print(f"  [Q15] FAIL — CRC mismatch.  {n_flipped}/26 header bits flipped vs expected")
    return crc_ok


# ============================================================
# Unified full RX decoder — header + data, optional Q15 precision
# ============================================================
def decode_full(tx_file=None, mod=MOD, n_syms=N_SYMS, fec_rate=FEC_RATE,
                guard_syms=1, use_q15=False):
    """
    Full RX chain mirroring HLS step-by-step:
      1. preamble channel estimate G
      2. BPSK header CRC check
      3. per-symbol pilot CPE on each data symbol (matches HLS compute_pilot_cpe)
      4. data demap → bits → deinterleave → Viterbi → descramble → bytes
      5. compare with tb_input_to_tx.bin → BER

    use_q15 = True applies HLS-equivalent fixed-point quantization at storage
    points (Q15 for freq_buf/G, Q20 for inv_t, Q22 for G_eq/eq).  This makes
    Python a precision-class apples-to-apples reference for HLS.
    """
    label = "Q15 " if use_q15 else "FP64"

    if tx_file is None:
        tx_file = HLS_FILE
    if not os.path.exists(tx_file):
        print(f"[{label}] {tx_file} not found")
        return None

    sig = np.loadtxt(tx_file, dtype=float)
    sig_c = sig[:, 0] + 1j * sig[:, 1]

    sym_len   = FFT_SIZE + CP_LEN
    guard_len = guard_syms * sym_len
    expected_samps = (n_syms + 2 + guard_syms) * sym_len
    if len(sig_c) < expected_samps:
        print(f"[{label}] FAIL: signal too short ({len(sig_c)} < {expected_samps})")
        return None

    zc = zc_sequence()
    if use_q15:
        zc = _q15(zc)

    def _fft_body(body):
        Y = np.fft.fft(body)
        if use_q15:
            Y = Y / FFT_SIZE
            Y = _q15(Y)
        return Y

    def _compute_geq_at(k, G_arr):
        gre, gim = G_arr[k].real, G_arr[k].imag
        if use_q15:
            denom = _q20_real(gre*gre + gim*gim)
            if denom <= 0:
                return 0 + 0j
            inv = _q20_real(1.0 / denom)
            return _q22(np.array([(gre - 1j*gim) * inv]))[0]
        else:
            mag2 = gre*gre + gim*gim
            if mag2 <= 1e-12:
                return 0 + 0j
            return (gre - 1j*gim) / mag2

    # ── Step 1: channel estimation from preamble ──
    pre_body = sig_c[guard_len + CP_LEN : guard_len + sym_len]
    Y_pre = _fft_body(pre_body)
    G = np.zeros(FFT_SIZE, dtype=np.complex128)
    for d, k in enumerate(DATA_SC_IDX):
        g = Y_pre[k] * np.conj(zc[k])
        G[k] = _q15(np.array([g]))[0] if use_q15 else g
    for k in PILOT_IDX:
        G[k] = Y_pre[k]   # pilot transmitted at +1 → G = Y_pre

    # ── Step 2: header decode ──
    hdr_off  = guard_len + sym_len
    hdr_body = sig_c[hdr_off + CP_LEN : hdr_off + sym_len]
    Y_hdr    = _fft_body(hdr_body)

    sum_re = sum_im = 0.0
    for k in PILOT_IDX:
        Geq_p = _compute_geq_at(k, G)
        eq = Y_hdr[k] * Geq_p
        if use_q15:
            eq = _q22(np.array([eq]))[0]
        sum_re += eq.real
        sum_im += eq.imag
    hdr_phase_err = np.arctan2(sum_im, sum_re)

    rot = np.exp(-1j * hdr_phase_err)
    if use_q15:
        rot = rot.astype(np.complex64)

    hdr_bits = 0
    for d in range(26):
        k    = DATA_SC_IDX[d]
        Geq  = _compute_geq_at(k, G)
        Gf   = Geq * rot
        if use_q15:
            Gf = _q22(np.array([Gf]))[0]
        eq   = Y_hdr[k] * Gf
        if use_q15:
            eq = _q22(np.array([eq]))[0]
        bit  = 1 if eq.real < 0 else 0
        hdr_bits |= (bit << (25 - d))

    rx_crc     = (hdr_bits >> 10) & 0xFFFF
    rx_payload = hdr_bits & 0x3FF
    exp_crc    = crc16_hdr(rx_payload)
    rx_n_syms  = (rx_payload >> 2) & 0xFF
    rx_mod     = (rx_payload >> 1) & 1
    rx_rate    = rx_payload & 1
    header_pass = (rx_crc == exp_crc)

    print(f"  [{label}] phase_err = {np.rad2deg(hdr_phase_err):+.3f} deg  "
          f"header: n_syms={rx_n_syms} mod={rx_mod} rate={rx_rate}  "
          f"CRC 0x{rx_crc:04X}/0x{exp_crc:04X}  {'PASS' if header_pass else 'FAIL'}")

    # ── Step 3: data demap with per-symbol pilot CPE ──
    bps = 2 if mod == 0 else 4
    decoded_bits = []

    for s in range(n_syms):
        offset = guard_len + sym_len * (s + 2)
        sym_iq = sig_c[offset + CP_LEN : offset + sym_len]
        Y      = _fft_body(sym_iq)

        # per-symbol CPE
        sum_re = sum_im = 0.0
        for k in PILOT_IDX:
            Geq_p = _compute_geq_at(k, G)
            eq = Y[k] * Geq_p
            if use_q15:
                eq = _q22(np.array([eq]))[0]
            sum_re += eq.real
            sum_im += eq.imag
        sym_phase_err = np.arctan2(sum_im, sum_re)
        sym_rot = np.exp(-1j * sym_phase_err)
        if use_q15:
            sym_rot = sym_rot.astype(np.complex64)

        for d, k in enumerate(DATA_SC_IDX):
            Geq = _compute_geq_at(k, G)
            Gf  = Geq * sym_rot
            if use_q15:
                Gf = _q22(np.array([Gf]))[0]
            X_hat = Y[k] * Gf
            if use_q15:
                X_hat = _q22(np.array([X_hat]))[0]

            if mod == 0:  # QPSK
                b1 = 1 if X_hat.real < 0 else 0
                b0 = 1 if X_hat.imag < 0 else 0
                decoded_bits.extend([b1, b0])
            else:  # 16-QAM
                i_bits = decode_axis_16qam(X_hat.real)
                q_bits = decode_axis_16qam(X_hat.imag)
                b3 = (i_bits >> 1) & 1
                b2 =  i_bits       & 1
                b1 = (q_bits >> 1) & 1
                b0 =  q_bits       & 1
                decoded_bits.extend([b3, b2, b1, b0])

    # ── Step 4: pack bits → bytes (matches TX format) ──
    decoded_bits = np.array(decoded_bits, dtype=np.uint8)
    if mod == 0:
        n_total = len(decoded_bits) // 2
        decoded_bytes = []
        for i in range(0, n_total, 4):
            s0 = (int(decoded_bits[i*2])     << 1) | int(decoded_bits[i*2+1])
            s1 = (int(decoded_bits[(i+1)*2]) << 1) | int(decoded_bits[(i+1)*2+1])
            s2 = (int(decoded_bits[(i+2)*2]) << 1) | int(decoded_bits[(i+2)*2+1])
            s3 = (int(decoded_bits[(i+3)*2]) << 1) | int(decoded_bits[(i+3)*2+1])
            decoded_bytes.append((s0 << 6) | (s1 << 4) | (s2 << 2) | s3)
    else:
        n_total = len(decoded_bits) // 4
        decoded_bytes = []
        for i in range(0, n_total, 2):
            s0 = (int(decoded_bits[i*4])     << 3 | int(decoded_bits[i*4+1]) << 2 |
                  int(decoded_bits[i*4+2])   << 1 | int(decoded_bits[i*4+3]))
            s1 = (int(decoded_bits[(i+1)*4]) << 3 | int(decoded_bits[(i+1)*4+1]) << 2 |
                  int(decoded_bits[(i+1)*4+2]) << 1 | int(decoded_bits[(i+1)*4+3]))
            decoded_bytes.append((s0 << 4) | s1)
    decoded_bytes = np.array(decoded_bytes, dtype=np.uint8)

    # ── Step 5: FEC decode + descramble ──
    coded_per_sym = NUM_DATA_SC // 4 if mod == 0 else NUM_DATA_SC // 2
    total_coded   = n_syms * coded_per_sym
    total_raw     = total_coded // 2 if fec_rate == 0 else total_coded * 2 // 3

    deinterleaved = py_deinterleave(bytes(decoded_bytes), mod, n_syms)
    raw_decoded   = fec_decode(deinterleaved, fec_rate, total_raw)
    raw_decoded   = py_scramble(raw_decoded)
    decoded_bytes = np.frombuffer(raw_decoded, dtype=np.uint8)

    # ── Step 6: BER vs original ──
    if not os.path.exists(IN_FILE):
        print(f"  [{label}] {IN_FILE} not found — cannot compute BER")
        return None

    original = np.frombuffer(open(IN_FILE, 'rb').read(), dtype=np.uint8)
    if len(decoded_bytes) != len(original):
        print(f"  [{label}] FAIL: length mismatch decoded={len(decoded_bytes)} original={len(original)}")
        return None

    byte_errors = int(np.sum(decoded_bytes != original))
    diff_xor    = decoded_bytes ^ original
    bit_errors  = int(sum(bin(x).count('1') for x in diff_xor))
    total_bits  = len(original) * 8
    ber         = bit_errors / total_bits

    overall = "PASS" if (header_pass and bit_errors == 0) else "FAIL"
    print(f"  [{label}] data: byte_err={byte_errors}/{len(original)}  bit_err={bit_errors}/{total_bits}  BER={ber:.2e}  → {overall}")

    return {'header_pass': header_pass,
            'phase_err_deg': np.rad2deg(hdr_phase_err),
            'byte_errors': byte_errors,
            'bit_errors': bit_errors,
            'total_bits': total_bits}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--gen",     action="store_true", help="Generate bits and reference output")
    parser.add_argument("--compare", action="store_true", help="Compare HLS TX output to reference")
    parser.add_argument("--decode",  action="store_true", help="Decode TX output and check BER")
    parser.add_argument("--decode-hls", action="store_true",
                        help="Decode HLS TX output and check BER")
    parser.add_argument("--decode-header", action="store_true",
                        help="Python-side BPSK header decode (mirrors HLS) for low-SNR diagnosis")
    parser.add_argument("--decode-header-q15", action="store_true",
                        help="HLS-precision-class (Q15/Q20/Q22) header decode — pins down precision-vs-algorithm bug")
    parser.add_argument("--decode-full", action="store_true",
                        help="Full RX chain (header + data) in float64 with per-symbol pilot CPE")
    parser.add_argument("--decode-full-q15", action="store_true",
                        help="Full RX chain (header + data) at HLS-equivalent Q15/Q20/Q22 precision")
    parser.add_argument("--input",   type=str, default=None,
                        help="Override IQ input file for --decode-hls (default: tb_tx_output_hls.txt)")
    parser.add_argument("--mod",     type=int, default=MOD,      help="0=QPSK 1=16QAM")
    parser.add_argument("--nsyms",   type=int, default=N_SYMS,   help="Number of data symbols")
    parser.add_argument("--rate",    type=int, default=FEC_RATE,  help="0=rate-1/2 1=rate-2/3")
    args = parser.parse_args()

    if not any([args.gen, args.compare, args.decode, args.decode_hls, args.decode_header,
                args.decode_header_q15, args.decode_full, args.decode_full_q15]):
        parser.print_help()
    if args.gen:
        generate(mod=args.mod, n_syms=args.nsyms, fec_rate=args.rate)
    if args.compare:
        compare()
    if args.decode:
        decode(tx_file=REF_FILE, mod=args.mod, n_syms=args.nsyms, fec_rate=args.rate)
    if args.decode_hls:
        decode(tx_file=args.input if args.input else HLS_FILE, mod=args.mod, n_syms=args.nsyms,
               fec_rate=args.rate, guard_syms=1)
    if args.decode_header:
        decode_header(tx_file=args.input if args.input else HLS_FILE,
                      mod=args.mod, n_syms=args.nsyms, fec_rate=args.rate, guard_syms=1)
    if args.decode_header_q15:
        decode_header_q15(tx_file=args.input if args.input else HLS_FILE,
                          mod=args.mod, n_syms=args.nsyms, fec_rate=args.rate, guard_syms=1)
    if args.decode_full:
        decode_full(tx_file=args.input if args.input else HLS_FILE,
                    mod=args.mod, n_syms=args.nsyms, fec_rate=args.rate,
                    guard_syms=1, use_q15=False)
    if args.decode_full_q15:
        decode_full(tx_file=args.input if args.input else HLS_FILE,
                    mod=args.mod, n_syms=args.nsyms, fec_rate=args.rate,
                    guard_syms=1, use_q15=True)
