#!/usr/bin/env python3
"""
fec_reference.py  —  K=7 Convolutional FEC Reference Implementation

Matches conv_enc.cpp / viterbi_dec.cpp exactly:
  - G0 = 0x5B (1011011), G1 = 0x79 (1111001)
  - State convention: sr[5] = newest bit, after input b: new_sr = (b<<5)|(old>>1)
  - Rate 1/2: emit G0, G1 for every bit
  - Rate 2/3: emit G0, G1 for even bits; G0 only for odd bits (G1 punctured)
  - Byte format: MSB first

Usage:
  python3 fec_reference.py          # run self-test (BER=0 checks + noisy test)
  python3 fec_reference.py --sweep  # BER vs noise sweep
"""

import sys
import numpy as np

# ── Reed-Solomon outer code (RS(255, 223), CCSDS deep-space standard) ───────
#
# nsym=32 parity bytes per codeword, t=16 byte-error correction.  reedsolo's
# default RSCodec(32) auto-chunks: messages up to 223 bytes → single
# codeword (output K + 32); longer messages → multiple full codewords with
# the last one shortened.  Output length is exactly K + 32 * ceil(K/223),
# but only at *reachable* K — there is a "dead zone" of output sizes between
# 256 and 287 bytes (1 codeword maxes at 255, 2 codewords starts at 288).
# Callers in dead-zone regimes use rs_max_payload() to pick the largest
# reachable K and zero-pad the unused tail before conv encoding.
#
# Library: reedsolo (pure Python).  HLS production uses the Xilinx RS IP.
# ────────────────────────────────────────────────────────────────────────────
import reedsolo as _reedsolo

RS_NSYM     = 32                          # parity bytes per codeword
RS_K_PER_CW = 255 - RS_NSYM               # 223 — max msg bytes per codeword
_RSC        = _reedsolo.RSCodec(RS_NSYM)


def rs_encode(msg: bytes) -> bytes:
    """RS(255, 223) encode.  Output length: K + 32*ceil(K/223)."""
    return bytes(_RSC.encode(msg))


def rs_decode(coded: bytes) -> bytes:
    """Inverse of rs_encode.  Raises reedsolo.ReedSolomonError if uncorrectable."""
    decoded, _, _ = _RSC.decode(bytes(coded))
    return bytes(decoded)


def rs_max_payload(rs_output_size: int):
    """Pick the largest message length K such that rs_encode(K bytes) yields
    AT MOST rs_output_size bytes.  Returns (K, rs_actual_out).  Caller pads
    the conv-encoder input from rs_actual_out → rs_output_size with zeros.

    Example: rs_output_size=266 (16QAM 2/3, 4 syms) lands in the dead zone
    between 1-codeword (max 255) and 2-codewords (min 288).  Returns
    K=223, rs_actual_out=255, leaving 11 zero-pad bytes.
    """
    best_K, best_out = -1, -1
    for n_cw in range(1, rs_output_size // RS_NSYM + 1):
        K_min = (n_cw - 1) * RS_K_PER_CW + 1
        K_max = min(n_cw * RS_K_PER_CW, rs_output_size - RS_NSYM * n_cw)
        if K_max >= K_min and K_max > best_K:
            best_K, best_out = K_max, K_max + RS_NSYM * n_cw
    if best_K < 0:
        raise ValueError(f"rs_output_size={rs_output_size} too small for any RS codeword")
    return best_K, best_out


# ── Encoder ──────────────────────────────────────────────────────────────────

def conv_encode(data_bytes: bytes, rate: int) -> bytes:
    """Encode data_bytes. rate=0 → 1/2, rate=1 → 2/3. Returns coded bytes."""
    sr = 0  # 6-bit shift register, sr bit5 = newest

    coded_bits = []
    global_bit = 0

    for byte_val in data_bytes:
        for bit_pos in range(7, -1, -1):          # MSB first
            b = (byte_val >> bit_pos) & 1

            # G0 = b^sr[4]^sr[3]^sr[1]^sr[0]
            g0 = b ^ ((sr>>4)&1) ^ ((sr>>3)&1) ^ ((sr>>1)&1) ^ (sr&1)
            # G1 = b^sr[5]^sr[4]^sr[3]^sr[0]
            g1 = b ^ ((sr>>5)&1) ^ ((sr>>4)&1) ^ ((sr>>3)&1) ^ (sr&1)

            # Update shift register
            sr = ((b << 5) | (sr >> 1)) & 0x3F

            coded_bits.append(g0)
            if rate == 0 or (global_bit & 1) == 0:
                coded_bits.append(g1)              # G1 present
            # else: G1 punctured (rate 2/3, odd input bit)

            global_bit += 1

    # Pack coded bits into bytes (MSB first, zero-pad last byte if needed)
    n_bytes = (len(coded_bits) + 7) // 8
    out = bytearray(n_bytes)
    for i, bit in enumerate(coded_bits):
        if bit:
            out[i // 8] |= (1 << (7 - (i % 8)))
    return bytes(out)


# ── Decoder ──────────────────────────────────────────────────────────────────

def _branch_outputs(s: int, b: int):
    """Expected (g0, g1) for old state s encoding bit b."""
    g0 = b ^ ((s>>4)&1) ^ ((s>>3)&1) ^ ((s>>1)&1) ^ (s&1)
    g1 = b ^ ((s>>5)&1) ^ ((s>>4)&1) ^ ((s>>3)&1) ^ (s&1)
    return g0, g1


def viterbi_decode(coded_bytes: bytes, rate: int, n_data_bytes: int) -> bytes:
    """Decode coded_bytes. rate=0 → 1/2, rate=1 → 2/3.
    n_data_bytes: expected output size (also determines trellis length).
    Returns decoded bytes."""
    n_data_bits  = n_data_bytes * 8
    n_coded_bits = (n_data_bits * 2) if rate == 0 else (n_data_bits // 2 * 3)

    # Unpack coded bits
    coded_bits = []
    for b in coded_bytes:
        for p in range(7, -1, -1):
            coded_bits.append((b >> p) & 1)
    coded_bits = coded_bits[:n_coded_bits]

    # Depuncture: build trellis pairs (None = erasure)
    trellis = []   # list of (r0, r1) — r1=None if erased
    ci = 0
    for step in range(n_data_bits):
        r0 = coded_bits[ci]; ci += 1
        if rate == 0 or (step & 1) == 0:
            r1 = coded_bits[ci]; ci += 1
        else:
            r1 = None   # punctured position
        trellis.append((r0, r1))

    # Forward pass
    N = 64
    INF = 10**9
    pm = [INF] * N
    pm[0] = 0      # start state = 0
    decisions = []  # decisions[step][state] = 0 or 1

    for step in range(n_data_bits):
        r0, r1 = trellis[step]
        pm_new = [INF] * N
        dec_step = [0] * N

        for sp in range(N):
            b  = (sp >> 5) & 1            # decoded bit = MSB of new state
            p0 = (sp & 0x1F) << 1         # predecessor (old_state[0]=0)
            p1 = ((sp & 0x1F) << 1) | 1   # predecessor (old_state[0]=1)

            g0, g1 = _branch_outputs(p0, b)
            # Complement property: p1 outputs are (g0^1, g1^1)

            # Branch metric p0
            bm0 = 0
            if r0 is not None: bm0 += (r0 != g0)
            if r1 is not None: bm0 += (r1 != g1)

            # Branch metric p1 (complement)
            n_active = (r0 is not None) + (r1 is not None)
            bm1 = n_active - bm0

            cost0 = (pm[p0] + bm0) if pm[p0] < INF else INF
            cost1 = (pm[p1] + bm1) if pm[p1] < INF else INF

            if cost0 <= cost1:
                pm_new[sp] = cost0
                dec_step[sp] = 0
            else:
                pm_new[sp] = cost1
                dec_step[sp] = 1

        pm = pm_new
        decisions.append(dec_step)

    # Traceback from minimum-metric state
    best_state = pm.index(min(pm))
    decoded_bits = [0] * n_data_bits
    state = best_state

    for step in range(n_data_bits - 1, -1, -1):
        decoded_bits[step] = (state >> 5) & 1
        pred_idx = decisions[step][state]
        state = ((state & 0x1F) << 1) | pred_idx

    # Pack decoded bits into bytes (MSB first)
    out = bytearray(n_data_bytes)
    for byte_idx in range(n_data_bytes):
        for p in range(8):
            if decoded_bits[byte_idx * 8 + p]:
                out[byte_idx] |= (1 << (7 - p))
    return bytes(out)


# ── Soft-decision Viterbi (Euclidean / linear branch metric) ────────────────
#
# Sign convention for `soft_bits`:
#   r > 0  →  bit=0 likely (high confidence when |r| is large)
#   r < 0  →  bit=1 likely
#   r == 0 →  no information
#   r is None → punctured (erased) position; do not contribute to metric
#
# Branch metric (per coded bit, lower = better path):
#   bm = +r  if hypothesis bit g = 1
#   bm = -r  if hypothesis bit g = 0
# Equivalent to ML for AWGN with constant noise variance — the (r±1)² metric
# minus per-step constants reduces to ±r.
#
# The forward recursion / traceback structure is identical to the hard
# decoder; only the branch metric changes from Hamming to linear-soft.
def viterbi_decode_soft(soft_bits, rate: int, n_data_bytes: int,
                         clip_factor: float = 0.0) -> bytes:
    """Soft-decision Viterbi.  See module docstring above for sign convention.

    soft_bits     : iterable of floats (or None for punctured positions).
    rate          : 0 = rate-1/2, 1 = rate-2/3 (G1 punctured on odd input bits).
    n_data_bytes  : expected output payload size — also defines trellis length.
    clip_factor   : if > 0, clip soft values to ±(clip_factor × median(|soft|)).
                    A single SC with extreme magnitude (deep fade, CPE outlier,
                    or numerical overflow) can dominate the trellis search and
                    push all path metrics into one hypothesis — the so-called
                    "metric outlier" failure mode.  Median-based clipping is
                    robust against the outliers themselves (mean would be
                    biased by them).  Typical values: 5.0 conservative, 3.0
                    aggressive.  0.0 = disabled (matches old behavior).
    """
    n_data_bits  = n_data_bytes * 8
    soft = list(soft_bits)

    # ── Optional LLR clipping (median-based robust outlier rejection) ──
    if clip_factor > 0.0:
        # Median of |soft|, ignoring None (erasures from puncture pattern).
        finite_mags = [abs(v) for v in soft if v is not None]
        if finite_mags:
            import statistics as _stats
            thresh = clip_factor * _stats.median(finite_mags)
            if thresh > 0.0:
                soft = [
                    (max(-thresh, min(thresh, v)) if v is not None else None)
                    for v in soft
                ]

    # Build per-step (r0, r1) pairs.  For rate-2/3, every other r1 is None
    # (puncture pattern matches the encoder in conv_encode).
    trellis = []
    ci = 0
    for step in range(n_data_bits):
        r0 = soft[ci]; ci += 1
        if rate == 0 or (step & 1) == 0:
            r1 = soft[ci]; ci += 1
        else:
            r1 = None
        trellis.append((r0, r1))

    N        = 64
    INF      = float("inf")
    pm       = [INF] * N
    pm[0]    = 0.0
    decisions = []

    for step in range(n_data_bits):
        r0, r1 = trellis[step]
        pm_new   = [INF] * N
        dec_step = [0]   * N

        for sp in range(N):
            b  = (sp >> 5) & 1            # decoded bit = MSB of new state
            p0 = (sp & 0x1F) << 1         # predecessor (old_state[0]=0)
            p1 = ((sp & 0x1F) << 1) | 1   # predecessor (old_state[0]=1)

            g0, g1 = _branch_outputs(p0, b)
            # p1's outputs are the complement: (g0^1, g1^1).

            # Linear soft branch metric: bm += r if hypothesis bit = 1, else -r.
            # Equivalent to negative correlation (lower = better match).
            bm0 = 0.0
            if r0 is not None: bm0 += ( r0 if g0      == 1 else -r0)
            if r1 is not None: bm0 += ( r1 if g1      == 1 else -r1)
            bm1 = 0.0
            if r0 is not None: bm1 += ( r0 if (g0^1)  == 1 else -r0)
            if r1 is not None: bm1 += ( r1 if (g1^1)  == 1 else -r1)

            cost0 = (pm[p0] + bm0) if pm[p0] < INF else INF
            cost1 = (pm[p1] + bm1) if pm[p1] < INF else INF

            if cost0 <= cost1:
                pm_new[sp]   = cost0
                dec_step[sp] = 0
            else:
                pm_new[sp]   = cost1
                dec_step[sp] = 1

        pm = pm_new
        decisions.append(dec_step)

    # Traceback from minimum-metric terminal state.
    best_state   = pm.index(min(pm))
    decoded_bits = [0] * n_data_bits
    state        = best_state

    for step in range(n_data_bits - 1, -1, -1):
        decoded_bits[step] = (state >> 5) & 1
        pred_idx           = decisions[step][state]
        state              = ((state & 0x1F) << 1) | pred_idx

    out = bytearray(n_data_bytes)
    for byte_idx in range(n_data_bytes):
        for p in range(8):
            if decoded_bits[byte_idx * 8 + p]:
                out[byte_idx] |= (1 << (7 - p))
    return bytes(out)


# ── Header FEC: K=7 convolutional, rate-1/2 or rate-1/3 ─────────────────────
#
# K=7 conv code, NASA Voyager / 802.11a generator polynomials:
#   G0 = 0x5B (133 octal, 1011011) — same as data path
#   G1 = 0x79 (171 octal, 1111001) — same as data path
#   G2 = 0x75 (165 octal, 1110101) — added for rate-1/3
#
# Used to FEC-protect the OFDM BPSK header.  6 zero tail bits flush the
# trellis to state 0 for clean traceback.
#
# Sizing for the 200-data-SC header symbol (200 BPSK SCs available):
#   rate-1/2:  capacity = 100 input bits → ≤ 78 payload bits + 16 CRC + 6 tail
#   rate-1/3:  capacity =  66 input bits → ≤ 44 payload bits + 16 CRC + 6 tail
#
# Default rate-1/2 was chosen for forward compatibility with MAC growth — the
# header will eventually need ~70 bit payload (frame_type, src/dst addrs,
# seq_num, priority, etc).  Rate-1/3 is available for the smaller current
# header (10 payload bits) when extra coding gain matters more than capacity.
#
# Cost in HLS: small Viterbi (32-step trellis at today's 10-bit payload, up
# to ~92 steps at 70-bit payload), 64 states, no sliding-window — ~50-100 LUT.
# ────────────────────────────────────────────────────────────────────────────

def conv_encode_header(input_bits, rate: str = "1/2") -> list:
    """K=7 convolutional encoder for the OFDM header.

    input_bits : iterable of bits to encode (MSB first).  Typically this is
                 (payload_bits || CRC-16) — the caller composes them.
    rate       : "1/2" → (G0, G1) — 2 output bits per input
                 "1/3" → (G0, G1, G2) — 3 output bits per input

    Appends 6 zero tail bits before encoding to flush the trellis.
    Returns: list of coded bits, length = (len(input_bits) + 6) × {2 or 3}.
    """
    if rate not in ("1/2", "1/3"):
        raise ValueError(f"unsupported header FEC rate: {rate!r} (use '1/2' or '1/3')")
    bits = list(input_bits)

    sr = 0  # 6-bit shift register, bit 5 = newest
    coded = []
    for b in bits + [0] * 6:                     # +6 tail bits to flush
        b  = int(b) & 1
        # Compute all generators BEFORE updating sr (they depend on the
        # current state).
        g0 = b ^ ((sr>>4)&1) ^ ((sr>>3)&1) ^ ((sr>>1)&1) ^ (sr&1)
        g1 = b ^ ((sr>>5)&1) ^ ((sr>>4)&1) ^ ((sr>>3)&1) ^ (sr&1)
        coded.append(g0)
        coded.append(g1)
        if rate == "1/3":
            g2 = b ^ ((sr>>5)&1) ^ ((sr>>4)&1) ^ ((sr>>2)&1) ^ (sr&1)
            coded.append(g2)
        sr = ((b << 5) | (sr >> 1)) & 0x3F
    return coded


def viterbi_decode_header_soft(soft_bits, n_input_bits: int,
                                rate: str = "1/2",
                                clip_factor: float = 0.0) -> list:
    """Soft-decision Viterbi for the K=7 header conv code.

    soft_bits     : floats (positive → bit=0, negative → bit=1).  Length must
                    equal (n_input_bits + 6) × {2 or 3} depending on rate.
    n_input_bits  : number of input bits the encoder consumed (e.g. payload + CRC,
                    not including tail).  Decoder strips the 6 tail bits before
                    returning.
    rate          : "1/2" or "1/3" — must match the encoder.
    clip_factor   : if > 0, median-based outlier clipping.
    Returns       : list of n_input_bits decoded bits (MSB first).
    """
    if rate not in ("1/2", "1/3"):
        raise ValueError(f"unsupported header FEC rate: {rate!r}")
    bits_per_step = 2 if rate == "1/2" else 3
    n_steps       = n_input_bits + 6
    expected_n    = n_steps * bits_per_step
    if len(soft_bits) != expected_n:
        raise ValueError(
            f"expected {expected_n} soft bits for rate-{rate} on "
            f"{n_input_bits}-bit input, got {len(soft_bits)}")
    soft = list(soft_bits)

    # Optional LLR clipping (median-based, robust to outliers).
    if clip_factor > 0.0:
        import statistics as _stats
        thresh = clip_factor * _stats.median(abs(v) for v in soft)
        if thresh > 0.0:
            soft = [max(-thresh, min(thresh, v)) for v in soft]

    N         = 64
    INF       = float("inf")
    pm        = [INF] * N
    pm[0]     = 0.0          # encoder starts in state 0
    decisions = []

    for step in range(n_steps):
        r0 = soft[step * bits_per_step]
        r1 = soft[step * bits_per_step + 1]
        r2 = soft[step * bits_per_step + 2] if rate == "1/3" else None
        pm_new   = [INF] * N
        dec_step = [0]   * N

        for sp in range(N):
            b  = (sp >> 5) & 1            # decoded bit = MSB of new state
            p0 = (sp & 0x1F) << 1
            p1 = ((sp & 0x1F) << 1) | 1

            g0 = b ^ ((p0>>4)&1) ^ ((p0>>3)&1) ^ ((p0>>1)&1) ^ (p0&1)
            g1 = b ^ ((p0>>5)&1) ^ ((p0>>4)&1) ^ ((p0>>3)&1) ^ (p0&1)
            bm0 = (r0 if g0 == 1 else -r0) + (r1 if g1 == 1 else -r1)
            bm1 = (r0 if (g0^1) == 1 else -r0) + (r1 if (g1^1) == 1 else -r1)
            if rate == "1/3":
                g2 = b ^ ((p0>>5)&1) ^ ((p0>>4)&1) ^ ((p0>>2)&1) ^ (p0&1)
                bm0 += (r2 if g2 == 1 else -r2)
                bm1 += (r2 if (g2^1) == 1 else -r2)

            cost0 = (pm[p0] + bm0) if pm[p0] < INF else INF
            cost1 = (pm[p1] + bm1) if pm[p1] < INF else INF
            if cost0 <= cost1:
                pm_new[sp]   = cost0
                dec_step[sp] = 0
            else:
                pm_new[sp]   = cost1
                dec_step[sp] = 1

        pm = pm_new
        decisions.append(dec_step)

    # Traceback from state 0 (encoder appended 6 zero tail bits → final state = 0).
    state = 0
    decoded = [0] * n_steps
    for step in range(n_steps - 1, -1, -1):
        decoded[step] = (state >> 5) & 1
        pred_idx      = decisions[step][state]
        state         = ((state & 0x1F) << 1) | pred_idx

    return decoded[:n_input_bits]              # strip 6 tail bits


# Backwards-compat alias kept for any callers using the old name (now thin shim).
def conv_encode_header_rate13(payload_bits) -> list:
    """Deprecated — use conv_encode_header(input_bits, rate='1/3')."""
    return conv_encode_header(payload_bits, rate="1/3")


# ── Helpers ──────────────────────────────────────────────────────────────────

def count_bit_errors(a: bytes, b: bytes) -> int:
    errs = 0
    for x, y in zip(a, b):
        diff = x ^ y
        while diff:
            errs += diff & 1
            diff >>= 1
    return errs


def flip_bits(coded: bytes, n_coded_bits: int, pct: float, seed: int = 42) -> bytes:
    """Randomly flip pct% of the first n_coded_bits coded bits."""
    rng = np.random.default_rng(seed)
    n_flip = int(n_coded_bits * pct / 100)
    indices = rng.choice(n_coded_bits, size=n_flip, replace=False)

    arr = bytearray(coded)
    for idx in indices:
        arr[idx // 8] ^= (1 << (7 - (idx % 8)))
    return bytes(arr)


# ── Self-test ─────────────────────────────────────────────────────────────────

def run_tests():
    rng = np.random.default_rng(42)
    N = 200   # data bytes

    data = bytes(rng.integers(0, 256, N, dtype=np.uint8))

    print("=" * 50)
    print("  FEC Reference Self-Test")
    print("=" * 50)

    passes = 0

    for rate, rate_str in [(0, "1/2"), (1, "2/3")]:
        # ── Clean test ────────────────────────────────────────
        coded = conv_encode(data, rate)
        n_data_bits  = N * 8
        n_coded_bits = len(coded) * 8
        decoded = viterbi_decode(coded, rate, N)
        errs = count_bit_errors(data, decoded)

        label = f"Rate {rate_str} clean"
        if errs == 0:
            print(f"  [PASS]  {label}: BER=0 ({n_coded_bits} coded bits → {n_data_bits} data bits)")
            passes += 1
        else:
            print(f"  [FAIL]  {label}: {errs}/{n_data_bits} bit errors")

        # ── Noisy test (5% coded BER) ─────────────────────────
        noisy = flip_bits(coded, n_coded_bits, pct=5.0)
        decoded_noisy = viterbi_decode(noisy, rate, N)
        errs_noisy = count_bit_errors(data, decoded_noisy)
        ber_noisy = errs_noisy / n_data_bits

        label = f"Rate {rate_str} noisy (5% coded BER)"
        if errs_noisy < n_data_bits // 10:
            print(f"  [PASS]  {label}: {errs_noisy}/{n_data_bits} data bit errors (BER={ber_noisy:.2e})")
            passes += 1
        else:
            print(f"  [FAIL]  {label}: {errs_noisy}/{n_data_bits} bit errors (BER={ber_noisy:.2e})")

    print("-" * 50)
    print(f"  Result: {passes}/4 passed")
    print("=" * 50)
    return passes == 4


def run_sweep():
    """BER vs coded bit flip rate (simulates channel BER before FEC)."""
    rng = np.random.default_rng(42)
    N = 2000  # more bytes for better BER statistics

    data = bytes(rng.integers(0, 256, N, dtype=np.uint8))

    print("\nBER vs coded bit error rate (pre-FEC BER):")
    print(f"  {'Pre-FEC BER':>12} | {'Rate 1/2 BER':>14} | {'Rate 2/3 BER':>14}")
    print("  " + "-" * 46)

    for pct in [1, 2, 3, 5, 7, 10, 15]:
        row = f"  {pct:>11}% |"
        for rate in [0, 1]:
            coded = conv_encode(data, rate)
            n_coded_bits = N * 8 * (2 if rate == 0 else 3) // 2
            noisy  = flip_bits(coded, n_coded_bits, pct=float(pct), seed=42+pct)
            decoded = viterbi_decode(noisy, rate, N)
            errs = count_bit_errors(data, decoded)
            ber = errs / (N * 8)
            row += f" {ber:>14.2e} |"
        print(row)
    print()


if __name__ == "__main__":
    if "--sweep" in sys.argv:
        run_tests()
        run_sweep()
    else:
        ok = run_tests()
        sys.exit(0 if ok else 1)
