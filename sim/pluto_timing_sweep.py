"""
pluto_timing_sweep.py — sweep modcod × frame size, time each phase, verify BER=0.

Modcods: 16QAM 1/2 + 16QAM 2/3.
Sizes: 9, 18, 33, 60, 120, 255 syms (multiples of 3 — required by rate-2/3
puncturing, which needs total_raw bytes to come out byte-aligned; rate-1/2
has no such restriction but we use the same sizes for apples-to-apples).
For each: TX prep, TX upload, settle, RX capture, align+scale, decode.
Reports total wall time + effective throughput.

TX payload is deterministic (seed=42 in ofdm_reference.generate) — fixed
content per frame size, repeatable for benchmarking.

Run: .venv/bin/python sim/pluto_timing_sweep.py
"""
import time
import numpy as np
import adi
import ofdm_reference as ofdm

SYM = ofdm.FFT_SIZE + ofdm.CP_LEN  # 288 samples per OFDM symbol


def run_one(sdr, n_syms, mod, fec_rate, peak=2**13):
    """One sweep point.  Returns (timings_dict, pass, bit_errors, total_bits, k_app)."""
    t = {}

    # ── TX prep: generate burst + DAC scale ──
    t0 = time.perf_counter()
    sig = ofdm.generate(mod=mod, n_syms=n_syms, fec_rate=fec_rate).astype(np.complex64)
    pad = 2 * SYM
    burst = np.concatenate([
        np.zeros(pad, dtype=np.complex64),
        sig,
        np.zeros(pad, dtype=np.complex64),
    ])
    m = np.max(np.abs(burst))
    tx_iq = burst * (peak / m) if m > 0 else burst
    t["tx_prep"] = time.perf_counter() - t0

    # Scale RX buffer to fit at least 2 cycles + slack.
    # Pluto locks the buffer size after first rx(); destroy any prior buffer
    # so the new size takes effect.
    try:
        sdr.rx_destroy_buffer()
    except Exception:
        pass
    sdr.rx_buffer_size = max(8000, len(burst) * 2 + 4000)

    # ── TX upload (sdr.tx pushes samples to Pluto, starts cyclic playback) ──
    t1 = time.perf_counter()
    sdr.tx(tx_iq)
    t["tx_upload"] = time.perf_counter() - t1

    # ── Settle: wait + drain stale buffers (AGC ramp, partial cycles) ──
    t2 = time.perf_counter()
    time.sleep(0.05)
    for _ in range(2):
        sdr.rx()
    t["settle"] = time.perf_counter() - t2

    # ── RX capture (single rx() — 200k+ samples drained over USB) ──
    t3 = time.perf_counter()
    rx = np.asarray(sdr.rx(), dtype=np.complex128)
    t["rx_capture"] = time.perf_counter() - t3
    sdr.tx_destroy_buffer()

    # ── Align (cross-corr) + descale (×16/peak, BIST) + slice ──
    t4 = time.perf_counter()
    template = tx_iq[:5000].astype(np.complex128)
    corr = np.abs(np.correlate(rx, template, mode="valid"))
    offset = int(np.argmax(corr))
    rx_one = rx[offset : offset + len(burst)]
    rx_scaled = rx_one * (16.0 / peak)
    rx_unpadded = rx_scaled[pad : pad + len(sig)]
    t["align_scale"] = time.perf_counter() - t4

    # ── Decode: Q15 + soft Viterbi + RS ──
    t5 = time.perf_counter()
    res = ofdm.decode_full(
        tx_file=rx_unpadded.astype(np.complex128),
        mod=mod, n_syms=n_syms, fec_rate=fec_rate,
        use_sync=True, use_q15=True, use_soft=True,
        use_reed_solomon=True, use_cfo_correct=True,
    )
    t["decode"] = time.perf_counter() - t5
    t["total"] = time.perf_counter() - t0

    if res is None or res.get("sync_fail"):
        return t, False, 0, 0, 0
    bit_err = res.get("bit_errors", 0)
    total_bits = res.get("total_bits", 0)
    ok = bool(res.get("header_pass")) and bit_err == 0
    return t, ok, bit_err, total_bits, total_bits // 8


def main():
    print(f"[setup] connecting to Pluto, configuring BIST loopback…")
    sdr = adi.Pluto(uri="ip:192.168.2.1")
    sdr.sample_rate = 20_000_000
    sdr.tx_lo = sdr.rx_lo = 2_400_000_000
    sdr.tx_rf_bandwidth = sdr.rx_rf_bandwidth = 25_000_000
    sdr.tx_hardwaregain_chan0 = 0.0
    sdr.gain_control_mode_chan0 = "manual"
    sdr.rx_hardwaregain_chan0 = 20.0
    sdr.tx_cyclic_buffer = True
    sdr._ctrl.debug_attrs["loopback"].value = "1"
    print(f"[setup] BIST loopback={sdr._ctrl.debug_attrs['loopback'].value}, 20 MSPS, 2.4 GHz")
    print()

    # Modcods to sweep — both at the same frame sizes (multiples of 3 so
    # rate-2/3 puncturing comes out byte-aligned).
    sizes = [9, 18, 33, 60, 120, 255]
    modcods = [
        ("16QAM-1/2", 1, 0),
        ("16QAM-2/3", 1, 1),
    ]

    hdr = (f"{'modcod':<10}  {'n_syms':>6}  {'k_app':>5}  {'PASS':>4}  "
           f"{'tx_prep':>8}  {'tx_up':>8}  {'settle':>7}  "
           f"{'rx_cap':>8}  {'align':>7}  {'decode':>8}  {'total':>8}  "
           f"{'wire_kbps':>10}")

    for label, mod, rate in modcods:
        print()
        print(f"=== {label} ===")
        print(hdr)
        print("-" * len(hdr))

        for n in sizes:
            try:
                t, ok, be, tb, ka = run_one(sdr, n, mod, rate)
            except Exception as e:
                print(f"{label:<10}  {n:>6}  --     FAIL  exception: {e}")
                continue
            wire_kbps = (tb / t["total"]) / 1000 if t["total"] > 0 else 0
            print(f"{label:<10}  {n:>6}  {ka:>5}  {('PASS' if ok else 'FAIL'):>4}  "
                  f"{t['tx_prep']*1000:>6.1f}ms  {t['tx_upload']*1000:>6.1f}ms  "
                  f"{t['settle']*1000:>5.1f}ms  {t['rx_capture']*1000:>6.1f}ms  "
                  f"{t['align_scale']*1000:>5.1f}ms  {t['decode']*1000:>6.1f}ms  "
                  f"{t['total']*1000:>6.1f}ms  {wire_kbps:>10.1f}")

    print()
    print("Notes:")
    print("  - TX payload is DETERMINISTIC (seed=42 in ofdm_reference.generate)")
    print("  - tx_prep    = generate OFDM burst + DAC peak scaling")
    print("  - tx_upload  = sdr.tx(): load buffer + start cyclic playback over USB")
    print("  - settle     = sleep(50ms) + drain 2 stale rx() buffers")
    print("  - rx_capture = single sdr.rx() blocking until N samples drained over USB")
    print("  - align      = cross-correlation alignment + ×16/8192 scale + slice")
    print("  - decode     = Q15 + soft Viterbi + RS(255,223) full RX chain in Python")
    print("  - wire_kbps  = decoded bits / total wall time (NOT PHY rate, includes wrapper overhead)")


if __name__ == "__main__":
    main()
