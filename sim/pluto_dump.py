"""
pluto_dump.py — minimal: send 1000 random IQ samples through Pluto BIST,
capture, align by cross-correlation, dump TX and RX side-by-side to a text file.

Run: python3 sim/pluto_dump.py
Output: pluto_dump.txt  (4 columns: tx_i  tx_q  rx_i  rx_q)
"""
import time
import numpy as np
import adi

# ── TX pattern: 100 zeros + 800 random + 100 zeros (zero regions = oracle) ──
rng = np.random.default_rng(42)
sig = rng.integers(-8192, 8192, size=(800, 2)).astype(np.float32)
tx  = np.concatenate([
    np.zeros(100, dtype=np.complex64),
    (sig[:, 0] + 1j * sig[:, 1]).astype(np.complex64),
    np.zeros(100, dtype=np.complex64),
])
N = len(tx)

# ── Pluto: BIST loopback=1, 20 MSPS, 2.4 GHz ──
sdr = adi.Pluto(uri="ip:192.168.2.1")
sdr.sample_rate = 20_000_000
sdr.tx_lo = sdr.rx_lo = 2_400_000_000
sdr.tx_rf_bandwidth = sdr.rx_rf_bandwidth = 25_000_000
sdr.tx_hardwaregain_chan0 = 0.0
sdr.gain_control_mode_chan0 = "manual"
sdr.rx_hardwaregain_chan0 = 20.0
sdr.tx_cyclic_buffer = True
sdr.rx_buffer_size = 8000
sdr._ctrl.debug_attrs["loopback"].value = "1"

sdr.tx(tx)
time.sleep(0.05)
for _ in range(2): sdr.rx()         # drain
rx = np.asarray(sdr.rx(), dtype=np.complex64)
sdr.tx_destroy_buffer()

# ── Align RX to TX (where does tx[0] first appear in rx?) ──
corr = np.abs(np.correlate(rx, tx, mode="valid"))
offset = int(np.argmax(corr))
rx_aligned = rx[offset : offset + N]

# ── Dump 4-column side-by-side ──
with open("pluto_dump.txt", "w") as f:
    f.write("# tx_i  tx_q  rx_i  rx_q\n")
    for k in range(N):
        f.write(f"{tx[k].real:+8.1f}  {tx[k].imag:+8.1f}  "
                f"{rx_aligned[k].real:+10.2f}  {rx_aligned[k].imag:+10.2f}\n")

print(f"wrote pluto_dump.txt  ({N} rows, alignment offset = {offset})")
print(f"TX peak |x| = {np.max(np.abs(tx)):.0f}   RX peak |x| = {np.max(np.abs(rx_aligned)):.0f}")
print(f"first 5 rows:")
for k in range(5):
    print(f"  tx=({tx[k].real:+7.1f}, {tx[k].imag:+7.1f})  "
          f"rx=({rx_aligned[k].real:+8.2f}, {rx_aligned[k].imag:+8.2f})")
