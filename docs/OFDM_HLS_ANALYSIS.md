# OFDM TX HLS — Bug Analysis & FFT Normalization Notes

## 1. FFT/IFFT Normalization — The Core Problem

### Mathematical definitions

| Operation | Formula | Normalization |
|---|---|---|
| Forward DFT | `X[k] = Σ x[n] · e^(-j2πkn/N)` | None |
| Inverse DFT (numpy) | `x[n] = (1/N) · Σ X[k] · e^(+j2πkn/N)` | ÷N at IFFT |
| Inverse DFT (standard TX) | `x[n] = Σ X[k] · e^(+j2πkn/N)` | None |

### Xilinx HLS `hls::fft` IFFT behavior

The Xilinx FFT IP (PG109) does **NOT** apply `1/N` normalization. It computes the raw IDFT sum — identical to GNU Radio's FFTW backend (`FFTW_BACKWARD`, which is also unnormalized).

`numpy.fft.ifft` DOES divide by N. This is why HLS and Python produce different amplitudes for the same frequency-domain input — they implement different mathematical operations.

### GNU Radio convention

GNU Radio's OFDM TX chain does not normalize the IFFT. Power normalization is applied **in the frequency domain** via the KMOD constant (per IEEE 802.11a):

| Modulation | KMOD |
|---|---|
| BPSK | 1 |
| QPSK | 1/√2 |
| 16-QAM | 1/√10 |
| 64-QAM | 1/√42 |

KMOD normalizes constellation energy to 1 before the IFFT.

### `pipelined_streaming_io` is Radix-4, not Radix-2

This is the critical discovery. From `hls_fft.h`:

```cpp
unsigned tmp_bits = (arch == pipelined_streaming_io || arch == radix_4_burst_io)
                  ? ((max_nfft+1)>>1) * 2
                  : 2 * max_nfft;
```

For `pipelined_streaming_io` with `log2_transform_length=8` (N=256):
```
tmp_bits = ((8+1)>>1) * 2 = 4 * 2 = 8 bits   ← only 8 bits used
```

This means the architecture uses **4 Radix-4 stages** (4^4 = 256), with **2 bits per stage** = 8 bits total.
**Only the lower 8 bits of `scale_sch` matter.**

### `scale_sch` encoding (Radix-4, 2 bits per stage)

| 2-bit value | Scaling |
|---|---|
| `00` | ÷1 (no scale) |
| `01` | ÷2 |
| `10` | ÷4 |
| `11` | ÷8 |

### Empirical verification

| `scale_sch` | Lower 8 bits | Per stage | Total | HLS/numpy ratio | Measured |
|---|---|---|---|---|---|
| `0x2AB` | `0xAB` = `10_10_10_11` | ÷4,÷4,÷4,÷8 | ÷512 | 0.5× | ✓ |
| `0x5555` | `0x55` = `01_01_01_01` | ÷2,÷2,÷2,÷2 | ÷16 | 16× | ✓ (clipping) |
| `0xAA` | `0xAA` = `10_10_10_10` | ÷4,÷4,÷4,÷4 | ÷256 | **1×** | correct |
| `0x2AA` (default) | `0xAA` | same | ÷256 | **1×** | correct |

### Correct value

```cpp
scale_sch = 0xAA  =  10 10 10 10
                     ↑  ↑  ↑  ↑
                     stage 0..3: each ÷4 → total ÷4^4 = ÷256 = ÷N
```
This matches `numpy.fft.ifft` normalization exactly.

---

## 2. `ap_fixed<16,1>` Overflow Bug — Pilot and ZC Values

### The type

```cpp
typedef ap_fixed<16, 1> sample_t;
```

- Total bits: 16
- Integer bits (including sign): 1
- Format: `S.XXXXXXXXXXXXXXX` (sign + 15 fractional bits)
- Range: **[-1.0, +1.0)** — note: +1.0 is **NOT representable**
- Maximum positive value: `1 - 2^-15 = 0.999969482...`
- Default overflow mode: **wrap-around** (two's complement)

### What happens when you assign 1.0

```cpp
sample_t x = sample_t(1.0);
// +1.0 in Q0.15 = bit pattern 1_000000000000000
// MSB is the sign bit → interpreted as -1.0
// x == -1.0  ← WRONG!
```

### Where this bug hit us

**Bug 1 — Pilots (6 values corrupted):**
```cpp
// BEFORE (wrong):
freq_buf[PILOT_IDX[p]] = csample_t(sample_t(1.0), sample_t(0));
//                                  ^^^^^^^^^^^^^ wraps to -1.0!

// AFTER (correct):
freq_buf[PILOT_IDX[p]] = csample_t(sample_t(0.999969), sample_t(0));
```

**Bug 2 — ZC LUT (1 value corrupted):**
```
ZC_Q_LUT[99] = sample_t(1.000000)  ← wraps to -1.0
```
The ZC formula `exp(-jπu·k(k+1)/N)` can produce exactly ±1.0 for certain (u, k, N) combinations.

### Impact on EVM

Using Parseval's theorem: error in frequency domain → error in time domain:

```
Freq error:  -2.0 at each of 6 pilot positions (1.0 → -1.0, error = -2.0)
             -2.0 at ZC_Q_LUT[99]

RMS time-domain error from pilots alone:
  sum_n |err[n]|² = (1/N) × sum_k |freq_err[k]|²
                  = (1/256) × (6 × 4)  = 0.09375
  RMS = sqrt(0.09375 / 256) = 0.01912

After 0.5× HLS scale:  0.01912 × 0.5 = 0.00956

Signal RMS (HLS, 0x2AB scale) = 0.0281

EVM_pilots = 0.00956 / 0.0281 = 34%  ← measured: 34.xx%  ✓ exact match
```

This confirmed the pilot overflow was the dominant error source.

### Fix

```cpp
// In fill_freq_buffer() and send_preamble():
static const sample_t BPSK_ONE = sample_t(0.999969);  // 1 - 2^-15

// In gen_zc_lut.py — clamp to ap_fixed<16,1> range:
Q_MAX = 1.0 - 2**-15   # = 0.999969482421875
clamped = max(-1.0, min(Q_MAX, float(x)))
```

---

## 3. Debugging Timeline

| Step | Observation | Root Cause | Fix |
|---|---|---|---|
| First csim run | EVM = 55% | scale_sch=0x2AB → ÷64, numpy → ÷256 (4× mismatch) | Change scale_sch |
| Tried `* FFT_SIZE` in Python | EVM = 98% | Python now outputs raw IDFT, HLS still ÷64 — 512× apart | Reverted |
| Changed scale_sch to 0x5555 | EVM = 1309%, clipping | Pilot/ZC values = 1.0 overflow ap_fixed<16,1> → -1.0, IFFT produces garbage | Fix overflow |
| After pilot/ZC fix + 0x5555 | Expected: <1% EVM | Both bugs fixed | — |

---

## 4. Key Lessons

1. **Know your IFFT convention.** `numpy.fft.ifft` ÷N. Xilinx HLS FFT IP: no normalization. GNU Radio: no normalization. Pick one and be consistent.

2. **`ap_fixed<W,1>` cannot hold +1.0.** Range is `[-1, +1)`. Any value at or above 1.0 wraps to negative. Always clamp to `1 - 2^-(W-1)` for the positive boundary.

3. **`scale_sch = 0x5555` = normalized IFFT.** For 256-point, `01` at every stage = ÷2 × 8 stages = ÷256 = matches `numpy.fft.ifft`. This is the correct setting when comparing HLS C-sim against Python.

4. **Parseval's theorem is your debugger.** A wrong frequency-domain value translates directly to time-domain error with known power. Use it to verify which subcarrier(s) are wrong before running C-sim.

5. **C-sim is bit-accurate for `ap_fixed` types.** The Xilinx HLS FFT C model (`xilinx_ip_xfft_v9_0_bitacc_simulate`) faithfully simulates `scale_sch` and fixed-point overflow. Bugs in fixed-point types show up in C-sim exactly as they would in RTL.

6. **Don't force-match two implementations.** Multiplying Python output by 0.5 to match HLS is wrong — it hides the real discrepancy. Fix the root cause so both implementations independently produce the same result.

---

## 5. Final Correct Configuration

### Fixed-point types and pilot values (unchanged)

```cpp
// ofdm_tx.h
typedef ap_fixed<16, 1> sample_t;  // Q0.15: range [-1, +1), LSB = 2^-15

// ofdm_tx.cpp / ofdm_rx.cpp — pilots
freq_buf[PILOT_IDX[p]] = csample_t(sample_t(0.999969), sample_t(0));
//                                  max Q0.15 value = 1 - 2^-15  (+1.0 wraps to -1.0)

// ofdm_reference.py — pilots (both TX and RX reference)
freq[p] = 0.999969 + 0j   # match ap_fixed<16,1> max

// ofdm_reference.py — IFFT/FFT
time_domain = np.fft.ifft(freq_domain)   # TX: ÷N, matches scale_sch=0xAA IFFT
freq_domain = np.fft.fft(time_domain)    # RX: unnormalized, Y[k]=X[k] directly
```

### FFT implementation — hls::fft REPLACED by Xilinx xfft v9.1 IP

`hls::fft` was removed from both `ofdm_tx` and `ofdm_rx`. It is now an external
Xilinx xfft v9.1 block design IP connected via AXI-Stream ports.

**Reason:** `hls::fft` inlines 256-pt butterfly logic into HLS at synthesis time:
- ofdm_tx: +13,000 LUT, ofdm_rx: +7,000 LUT → total ~20,000 LUT from FFT alone
- Combined chain was ~49,000 LUT — 50% over the 32,600 available on XC7A50T
- xfft v9.1 256-pt pipelined_streaming_io: ~1,800 LUT + 4 DSP + 4 BRAM_18K per instance
- Net saving: **~15,000–17,000 LUT** (down to ~34K estimated)

### New HLS function signatures

```cpp
// ofdm_tx.h — added ifft_in/ifft_out ports
void ofdm_tx(
    hls::stream<ap_uint<8>> &bits_in,
    hls::stream<iq_t>        &iq_out,
    hls::stream<iq_t>        &ifft_in,   // to external xfft IFFT
    hls::stream<iq_t>        &ifft_out,  // from external xfft IFFT
    mod_t                     mod,
    ap_uint<8>                n_syms
);

// ofdm_rx.h — added fft_in/fft_out ports
void ofdm_rx(
    hls::stream<iq_t>       &iq_in,
    hls::stream<ap_uint<8>> &bits_out,
    hls::stream<iq_t>       &fft_in,    // to external xfft FFT
    hls::stream<iq_t>       &fft_out,   // from external xfft FFT
    ap_uint<1>              &header_err
);
```

### run_ifft / run_fft — streaming passthrough pattern

```cpp
// ofdm_tx.cpp — run_ifft sends to external IP and reads result back
static void run_ifft(csample_t in[FFT_SIZE], csample_t out[FFT_SIZE],
                     hls::stream<iq_t> &ifft_in, hls::stream<iq_t> &ifft_out) {
    #pragma HLS INLINE off
    IFFT_TX: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        iq_t s;
        s.i = in[i].real();  s.q = in[i].imag();
        s.last = (i == FFT_SIZE - 1) ? ap_uint<1>(1) : ap_uint<1>(0);
        ifft_in.write(s);
    }
    IFFT_RX: for (int i = 0; i < FFT_SIZE; i++) {
        #pragma HLS PIPELINE II=1
        iq_t s = ifft_out.read();
        out[i] = csample_t(s.i, s.q);
    }
}
// ofdm_rx.cpp — run_fft: identical pattern with fft_in/fft_out
```

### xfft v9.1 configuration

| Parameter | IFFT (ofdm_tx) | FFT (ofdm_rx) |
|---|---|---|
| transform_length | 256 | 256 |
| implementation_options | pipelined_streaming_io | pipelined_streaming_io |
| FWD_INV (bit 0) | 0 (IFFT) | 1 (FFT) |
| SCALE_SCH (bits 8:1) | 0xAA (÷256) | 0xAA (÷256) |
| Config word (decimal) | **340** | **341** |

Config word is driven by a 16-bit `xlconstant` with `s_axis_config_tvalid` tied to 1
permanently (so the config is latched on the first clock cycle).

### TDATA width mismatch (harmless)
- HLS `iq_t` → 48-bit TDATA: `{last[15:0-pad], q[15:0], i[15:0]}`
- xfft expects 32-bit TDATA: `{imag[15:0], real[15:0]}`
- Lower 32 bits map correctly; `last` field (bits 47:32) is dropped on input, zero on output
- HLS code counts 256 samples directly and does not use `fft_out.last` — safe
- Vivado issues a width-mismatch WARNING (not error); routes correctly

---

## 6. RX Chain Design

### Block diagram
```
IQ stream → remove_cp_and_read → run_fft(fwd, 0xAA) → estimate_channel → G[k]
                                                      ↓
                                  run_fft(fwd, 0xAA) → equalize_sc → demap → pack → bytes
```

### FFT normalization round-trip

| Stage | Operation | Scale | Output |
|---|---|---|---|
| TX IFFT | scale_sch=0xAA, fwd_inv=false | ÷256 | x[n] = X[k]/256 in time domain |
| RX FFT  | scale_sch=0xAA, fwd_inv=true  | ÷256 | Y[k] = X[k]/256 in freq domain |

Both divide by 256. The 1/256 factor is absorbed by the channel estimate G[k].

### Channel estimation

Preamble data SCs carry ZC[k] (|ZC[k]|=1). After round-trip:

```
Y_pre[k] = ZC[k] / 256

G[k] = Y_pre[k] * conj(ZC[k])
     = ZC[k]/256 * conj(ZC[k])
     = |ZC[k]|² / 256
     = 1/256  (real, positive — exact in ap_fixed<16,1>: 128 * 2^-15)
```

### Equalization

```
X_hat[k] = Y_dat[k] * conj(G[k]) / |G[k]|²

In ideal channel:
  Y_dat[k] = X_dat[k] / 256
  X_hat[k] = (X_dat/256) × (1/256) / (1/256)²  =  X_dat  ✓
```

Implemented with `ap_fixed<32,2>` intermediates. Division synthesizes as combinational divider
(correct for C-sim; replace with Newton-Raphson for timing-critical synthesis path).

### RX Synthesis — Before optimization (un-optimized equalize_sc with division)

| Resource | Used | Available | % |
|----------|------|-----------|---|
| LUT | 87,249 | 32,600 | **267% OVER** |
| FF | 121,672 | 65,200 | **186% OVER** |
| DSP | 169 | 120 | **140% OVER** |
| BRAM | 22 | 150 | 14% ✓ |

**Root cause:** HLS expanded 200 parallel `ap_fixed<32,2>` combinational dividers in the
`equalize_demap_pack` loop (each divider ≈ 60 LUT + 600 FF).

---

### RX Synthesis — After optimization (G_eq precomputation, multiply-only hot path)

| Resource | Used | Available | % |
|----------|------|-----------|---|
| LUT | 19,022 | 32,600 | **58%** ✓ |
| FF | 29,174 | 65,200 | **44%** ✓ |
| DSP | 69 | 120 | **57%** ✓ |
| BRAM_18K | 31 | 150 | **20%** ✓ |

**Timing: PASSES** — 7.124 ns estimated, 2.876 ns slack ✓

**Fix applied:** G_eq precomputation, scale_sch kept at 0xAA (÷256, safe for full-scale inputs):

```
estimate_channel():
  Phase 1 (PIPELINED):        G[d] = Y_pre[k] * conj(ZC[d])        (fast, multi-DSP)
  Phase 2 (NO PIPELINE → 1×): G_eq[k] = conj(G) / |G|²  ≈ 256    (1 divider reused)
  Type: cgeq_t = std::complex<ap_fixed<32,10>>  (range [-512,+512))

equalize_sc():
  Old: X_hat = Y * conj(G) / |G|²       (division per call, 200× in loop → overflow)
  New: X_hat = Y * G_eq  → returns cgeq_t  (multiply only, no cast to ap_fixed<16,1>)

qpsk_demap / qam16_demap:
  Now take cgeq_t — compare geq_t vs 0 or ±0.6325.
  Avoids overflow: noisy equalized value ≈ ±1.4 wraps in ap_fixed<16,1> but fits in
  ap_fixed<32,10>.
```

**scale_sch=0xAA is required** for the RX FFT. 0x55 (÷2/stage) causes intermediate
overflow for full-scale noisy inputs (2× growth per stage × 4 stages = 16× total).

**Verified BER vs SNR (QPSK, 4 symbols, 1600 bits):**

| SNR (dB) | Python BER | HLS BER | Notes |
|----------|-----------|---------|-------|
| clean    | 0         | 0       | Perfect decode |
| 20       | 0         | 0       | No errors |
| 10       | 2.50e-03  | 7.50e-03 | 3× fixed-point penalty |
| 6        | 6.06e-02  | 7.38e-02 | Gap narrows — noise dominates |

### HLS ap_uint shift truncation bug (FIXED)

**Bug:** `ap_uint<1> b1 = ...; return (b1 << 1) | b0;`
HLS keeps the result of `ap_uint<1> << 1` as `ap_uint<1>` — the bit shifts out → always 0.
Caused ~25% BER (I-component of every QPSK symbol decoded wrong).

**Fix:** Use bit-index assignment on a wider type:
```cpp
ap_uint<2> result;
result[1] = (sym.real() < sample_t(0)) ? 1 : 0;   // never loses the bit
result[0] = (sym.imag() < sample_t(0)) ? 1 : 0;
return result;
```
Same fix applied to 16-QAM `i_bits << 2` (ap_uint<2> shifted by 2 → 0).

### Verified loopback

```
tb_input_to_tx.bin → HLS TX C-sim → tb_tx_output_hls.txt
                                         ↓
                    HLS RX C-sim → tb_rx_decoded_hls.bin
                                         ↓
Python:  tb_rx_decoded_hls.bin == tb_input_to_tx.bin  →  BER = 0  ✓

Python reference decoder on tb_tx_output_hls.txt   →  BER = 0  ✓
```

### Test scripts

```bash
./run_loopback.sh                  # clean loopback: 5 steps, all PASS
./run_loopback_noisy.sh            # noisy loopback: default SNR=20 dB
./run_loopback_noisy.sh --snr 10   # marginal SNR test
```

### Resource usage — BEFORE xfft swap (hls::fft inline, Artix-50T)

| Resource | Available | TX | TX % | RX | RX % | Combined | Combined % |
|----------|-----------|-----|------|-----|------|----------|------------|
| LUT      | 32,600    | 16,387 | 50% | 19,022 | 58% | **~49,000** | **~150%** |
| FF       | 65,200    | 16,828 | 25% | 29,174 | 44% | ~46,002 | 71% |
| DSP      | 120       | 18  | 15% | 69  | 57% | 87  | 73% |
| BRAM_18K | 150       | 28  | 18% | 31  | 20% | 59  | 39% |

**hls::fft accounted for ~13K LUT in TX and ~7K LUT in RX — 50% over budget, not routable.**

### Resource usage — AFTER xfft swap (per-block HLS synthesis, 100 MHz)

These numbers are from individual HLS synthesis reports. Cross-boundary optimization during
full top-level implementation typically reduces LUT 10–20% further.

| Block            | LUT    | FF     | DSP | BRAM_18K |
|------------------|--------|--------|-----|----------|
| tx_scrambler     |   ~180 |   ~120 |   0 |    0     |
| tx_conv_enc      |   ~250 |   ~200 |   0 |    0     |
| tx_interleaver   |   ~600 |   ~400 |   0 |    2     |
| ofdm_tx_0        |  3,962 |  2,800 |  12 |    8     |
| sync_detect_0    | ~1,500 | ~1,000 |   4 |    4     |
| cfo_correct_0    | ~2,200 | ~1,500 |   8 |    0     |
| ofdm_rx_0        | 11,905 |  7,000 |  32 |   16     |
| rx_interleaver   |   ~600 |   ~400 |   0 |    2     |
| viterbi_dec_0    |  9,448 |  4,500 |   0 |    4     |
| rx_scrambler     |   ~180 |   ~120 |   0 |    0     |
| xfft × 2        | ~3,600 | ~2,400 |  12 |    8     |
| **TOTAL**        | **~34,425** | **~20,440** | **68** | **44** |

| Resource | Used (est.) | Available | Util% |
|----------|-------------|-----------|-------|
| LUT      | ~34,425     | 32,600    | ~106% |
| FF       | ~20,440     | 65,200    | ~31%  |
| DSP      | ~68         | 120       | ~57%  |
| BRAM_18K | ~44         | 150       | ~29%  |

The ~4% LUT overrun in per-block estimates is expected to resolve after integrated
implementation (cross-boundary merging, LUT combining). Real number requires LiteX
integration — see Section 8.

**Largest LUT consumer: viterbi_dec_0 at 9,448 LUT (27% of total).** If the integrated
design is still over budget, this is the primary reduction target.

---

## 7. Completed Work — Full Chain Status

All items from the original roadmap are now implemented as separate HLS IPs.

### Full TX+RX Chain (10 HLS IPs + 2 xfft IPs)

```
host_tx_in
  → tx_scrambler     (LFSR x⁷+x⁴+1 scrambling)
  → tx_conv_enc      (rate-1/2 convolutional encoder, K=7)
  → tx_interleaver   (frequency interleaver)
  → ofdm_tx_0        (subcarrier mapping, pilot insertion, CP, IFFT via xfft)
  → [xfft IFFT]
  → rf_tx_out

rf_rx_in
  → sync_detect_0    (ZC preamble correlator, timing sync)
  → cfo_correct_0    (CFO estimation from preamble phase, CORDIC rotation)
  → ofdm_rx_0        (CP removal, FFT via xfft, channel est, pilot CPE, equalize, demap)
  → [xfft FFT]
  → rx_interleaver   (frequency deinterleaver)
  → viterbi_dec_0    (rate-1/2 Viterbi decoder, K=7)
  → rx_scrambler     (descrambler)
  → host_rx_out
```

### IP Inventory

| IP | VLNV | Function | Status |
|----|------|----------|--------|
| tx_scrambler | hallycon.in:ofdm:scrambler:1.0 | TX LFSR scrambler | Done |
| tx_conv_enc | hallycon.in:ofdm:conv_enc:1.0 | Rate-1/2 conv encoder | Done |
| tx_interleaver | hallycon.in:ofdm:interleaver:1.0 | Freq interleaver | Done |
| ofdm_tx_0 | hallycon.in:ofdm:ofdm_tx:1.0 | OFDM modulator + IFFT I/F | Done |
| ofdm_tx_ifft | xilinx.com:ip:xfft:9.1 | 256-pt IFFT (external) | Done |
| sync_detect_0 | hallycon.in:ofdm:sync_detect:1.0 | ZC correlator + timing | Done |
| cfo_correct_0 | hallycon.in:ofdm:cfo_correct:1.0 | CFO correction | Done |
| ofdm_rx_0 | hallycon.in:ofdm:ofdm_rx:1.0 | OFDM demodulator + FFT I/F | Done |
| ofdm_rx_fft | xilinx.com:ip:xfft:9.1 | 256-pt FFT (external) | Done |
| rx_interleaver | hallycon.in:ofdm:interleaver:1.0 | Freq deinterleaver | Done |
| viterbi_dec_0 | hallycon.in:ofdm:viterbi_dec:1.0 | Rate-1/2 Viterbi decoder | Done |
| rx_scrambler | hallycon.in:ofdm:scrambler:1.0 | RX descrambler | Done |

---

## 8. Status & Outstanding Issues

### 8a. ✅ RESOLVED — Vivado post-implementation utilization (2026-04-11)

OOC (Out-of-Context) synthesis via `launch_runs synth_1 -jobs 1` correctly synthesizes
each IP block independently into a DCP, then stitches at top level. The trimming-to-7-LUT
problem is resolved. Full design is placed and routed.

**Actual post-implementation numbers (vivado/utilization_post_impl_summary.rpt):**

| Resource     | Used   | Available | Util%   |
|--------------|--------|-----------|---------|
| Slice LUTs   | 15,395 | 32,600    | **47%** |
| Slice FF     | 19,940 | 65,200    | **31%** |
| DSP48E1      | 94     | 120       | **78%** |
| BRAM Tile    | 26     | 75        | **35%** |
| BRAM_18K eq  | 52     | 150       | **35%** |
| Bonded IOB   | 82     | 150       | **55%** |

All 14 IPs instantiated and routed. No black boxes. Per-block HLS estimate was ~34K LUT;
integrated synthesis landed at 15,395 LUT (55% reduction from cross-boundary optimization).

**create_project.tcl memory settings (4 GB host):**
```tcl
set_param general.maxThreads 2   # caps internal threads
launch_runs synth_1 -jobs 1      # sequential OOC (was -jobs 8 → OOM)
launch_runs impl_1  -jobs 1
```
Note: `set_param synth.elaboration.rodinMoreOptions "rt::set_parameter conserveMemory 1"`
was tried and removed — `conserveMemory` is not a valid parameter in Vivado 2025.2.

### 8b. ✅ RESOLVED — synth_ip [get_ips *] error

Line removed from create_project.tcl. OOC synthesis via `launch_runs` handles all IP DCPs
automatically. No manual `synth_ip` call needed.

### 8c. ✅ RESOLVED — LUT budget

Per-block estimate of ~34,425 LUT (106%) was overly pessimistic. Actual integrated result:
15,395 LUT (47%). No reduction targets needed. DSP at 78% is the tightest resource.

### 8d. Remaining hardware integration steps

| Item | Status |
|------|--------|
| All 10 HLS IPs synthesized + exported | ✅ Done |
| Vivado block design + xfft connections | ✅ Done |
| Vivado OOC build — placed & routed | ✅ Done (2026-04-11) |
| Soft-decision Viterbi (S2) | TODO — next HLS task |
| Per-symbol pilot CFO tracking (S3) | TODO — ofdm_rx.cpp |
| PAPR clipping in ofdm_tx (S5) | TODO — ofdm_tx.cpp |
| PCIe + DMA + AD9361 HDL + mode mux | TODO — Vivado BD + LiteX |
| LiteX CSR integration (ofdm_subsystem.py) | TODO |
| xfft pipeline flush on boot (C6) | TODO — LiteX firmware |
| AD9364 IQ calibration SPI (S1) | TODO — LiteX BSP |
| Over-the-air loopback test (drone ↔ ground) | TODO |

---

## 9. Architect Review — Hardware Bring-Up Risk Assessment

**Context:** Artix-50T SDR boards ordered. Target: solid digital UAV link competitive with
commercial SDR products, approaching defence-grade reliability. This section identifies every
issue that will prevent hardware bring-up or degrade link quality, in priority order.

---

### 9.1 Executive Summary

The baseband DSP is mathematically correct and HLS-disciplined. The BER numbers hold. There
are **six issues that will cause failure on first hardware power-on** and **twelve issues
that cap performance below commercial-grade**. None require redesigning the DSP — they are
integration and protocol-layer problems that can be fixed before the boards arrive.

---

### 9.2 Critical — Will Fail on First Power-On

These must be fixed before any hardware bring-up attempt.

---

#### C1. `n_syms` Circular Dependency

`sync_detect` takes `n_syms` as an AXI-Lite input and uses it to compute
`total_output = (n_syms + 2) × 288` samples — it sets TLAST on the output stream at that
count. But `n_syms` is only extractable by `ofdm_rx` from the decoded header, which arrives
*after* sync_detect runs. The software cannot know `n_syms` before programming sync_detect
unless the transmitter signals it out-of-band.

**What breaks:** If `n_syms` is wrong, sync_detect sets TLAST at the wrong sample position.
`ofdm_rx` reads samples across the wrong frame boundary. The entire AXI-Stream chain
becomes permanently misaligned until a hard reset of all blocks.

**Fix:** Use a fixed constant `n_syms = 255` (maximum) in sync_detect — always output
`257 × 288 = 74,016` samples and set TLAST there. Let `ofdm_rx` handle shorter frames
internally using the header `n_syms` field. No n_syms feedback loop, no dependency.

---

#### C2. Guard Symbol Not Sent by TX

`sync_detect.h` documents that the search is designed around `SYNC_NL = 288` null samples
prepended before the ZC preamble:

```
// With one guard symbol prepended (SYNC_NL zeros), the preamble CP is
// at t = 288 and the first data CP boundary at t = 576 = SEARCH_WIN
```

`ofdm_tx` sends nothing before `send_preamble()` — it starts immediately with the ZC
symbol. In simulation the testbench prepends zeros externally, masking the issue. On
hardware with a live ADC stream, there are no pre-preamble zeros. If the receiver starts
searching while the channel is still settling or the previous frame's tail is still in the
FIFO, sync_detect finds a spurious metric peak in the noise and locks to the wrong offset.

**Fix:** Add `send_guard()` to `ofdm_tx` as the first thing in the top-level function:

```cpp
// Send SYNC_NL null samples so sync_detect search window is clean
GUARD_LOOP: for (int i = 0; i < SYNC_NL; i++) {
    #pragma HLS PIPELINE II=1
    iq_t s; s.i = 0; s.q = 0; s.last = 0;
    iq_out.write(s);
}
```

Three lines. Fixes cold-start sync for all future frames.

---

#### C3. Drain Loop Hangs Permanently on Corrupt Frame

`ofdm_rx.cpp:765`:

```cpp
DRAIN: do {
    #pragma HLS PIPELINE II=1
    iq_t s = iq_in.read();
    if (s.last) break;
} while (true);
```

This runs when `header_err = 1`. It waits for `s.last` from sync_detect. If `n_syms` was
wrong (C1), TLAST arrives at the wrong position. If the stream was interrupted or sync_detect
was reset, TLAST may never arrive. The HLS state machine stalls permanently. The AXI-Stream
bus deadlocks. The board requires a hard reset.

**Fix:** Add a sample counter as a hard timeout:

```cpp
ap_uint<17> drain_cnt = 0;
DRAIN: do {
    #pragma HLS PIPELINE II=1
    iq_t s = iq_in.read();
    if (s.last || ++drain_cnt > 74100) break;
} while (true);
```

Maximum legal frame is `(255 + 2) × 288 = 74,016` samples. The counter costs 17 FF and
zero LUT in the pipeline.

---

#### C4. CFO Handoff Has No Hardware Path + Missing ADC Input Buffer

**Revised understanding (supersedes the original 131K inter-block FIFO description):**

There are three distinct issues that must all be addressed together.

---

**C4a. cfo_est is routed through software — no hardware wire exists**

`sync_detect` exposes `cfo_est` as an AXI-Lite register. `cfo_correct` reads its own
separate AXI-Lite register. Nothing connects them in hardware. Per frame, software must:

1. Wait for `sync_detect ap_done` interrupt
2. AXI-Lite read `cfo_est` over PCIe (~2–5 µs typical, 50+ µs under OS jitter)
3. AXI-Lite write the value to `cfo_correct`
4. Assert `cfo_correct ap_start`

During steps 2–4, `sync_detect` is already streaming the 74,304-sample frame into the
inter-block FIFO at 100 MHz. The default auto-inserted FIFO is 512 words = 5 µs.
Under OS scheduling jitter (50 µs spike), 5,000 samples are dropped silently.
No error flag. No backpressure. Every frame corrupts.

**Fix (TCL — correct approach):**
Wire `cfo_est` directly in the block design. `sync_detect.cfo_est` (with `ap_vld`)
connects to `cfo_correct.cfo_est` through a register slice. `cfo_correct` sees the
value one clock after `sync_detect` finishes — no software round-trip, no FIFO depth
requirement between those two blocks.

```tcl
# Direct cfo_est wire: sync_detect → cfo_correct (1-clock propagation)
connect_bd_net \
    [get_bd_pins sync_detect_0/cfo_est] \
    [get_bd_pins cfo_correct_0/cfo_est]
```

With this wire in place, the inter-block FIFOs between `sync_detect → cfo_correct → ofdm_rx
→ rx_interleaver → viterbi_dec → rx_scrambler` can all stay at the default small depth —
they are pipeline buffers, not frame buffers.

---

**C4b. No input buffer before sync_detect — live ADC stream overflows**

While the RX chain is processing frame N (74,304 samples × 288 clocks/symbol), the AD9361
HDL is already streaming frame N+1. There is no buffer to absorb it. Frame N+1 is dropped.

At 100 MHz, one complete frame arrives in 74,304 / 100e6 = 0.74 ms. The full RX chain
(sync_detect + cfo_correct + ofdm_rx + Viterbi) takes roughly that same time to process
it. Back-to-back frames with no gap require double-buffering.

**Fix (TCL):**
Insert a 131,072-deep AXI-Stream FIFO before `sync_detect`. This holds 2× the maximum
frame size, giving the chain up to ~1.3 ms to start consuming each frame.

```tcl
# Input FIFO: absorbs live ADC stream while chain processes previous frame
# 131,072 × 32 bits = 4 BRAM_36 (2× max frame = 2 × 74,304 = 148,608 → round up)
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 adc_input_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {131072}] [get_bd_cells adc_input_fifo]
# Wire: AD9361_HDL iq_out → adc_input_fifo → sync_detect iq_in
```

BRAM cost: 4 BRAM_36. Current usage ~22/75 → 26/75 after this. Fits fine.

---

**C4c. No output buffer after rx_scrambler — DMA window is zero**

`rx_scrambler` outputs decoded bytes directly. If the host DMA is not ready the instant
the last byte arrives, bytes are dropped. There is no margin.

**Fix (TCL):**
Insert a 32,768-depth AXI-Stream FIFO after `rx_scrambler`. Maximum decoded payload per
frame is 25,500 bytes (16-QAM rate-2/3); 32 KB covers it completely.

```tcl
# Output FIFO: holds decoded bytes until host DMA reads them
# 32,768 bytes = 1 BRAM_36
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 rx_output_fifo
set_property -dict [list CONFIG.FIFO_DEPTH {32768}] [get_bd_cells rx_output_fifo]
# Wire: rx_scrambler data_out → rx_output_fifo → LiteX DMA engine
```

BRAM cost: 1 BRAM_36.

---

**Total BRAM cost for C4a+C4b+C4c: 5 BRAM_36 added (4 input + 1 output).**
Revised total: ~27/75 BRAM_36 (36%). PCIe is entirely on the output side — it reads
completed decoded bytes after the chain raises an interrupt. It has no involvement in
the inter-block stream.

---

#### C5. No Data Payload Integrity Check

Only the 10-bit frame header has CRC-16. Data symbols have no CRC, checksum, or integrity
marker. After Viterbi decode, corrupted bytes are silently written to the output FIFO and
passed to the host. For a UAV command/control link, a 1-bit error in "throttle value" or
"attitude setpoint" has no error recovery path. The link delivers wrong commands as valid.

The FEC + interleaver + scrambler reduce error rate but do not eliminate it — especially
at the SNR margins where the link operates at range.

**Decision: firmware/driver scope — no HLS changes.**

CRC belongs at the MAC layer, not the PHY. Implementing it in HLS would cost ~200 LUT,
require re-synthesis of both `ofdm_tx` and `ofdm_rx`, and add a new `data_err` port to
the block design. The same protection is achieved at zero FPGA cost by handling it in the
LiteX driver:

```
TX (LiteX firmware):
  payload[0..N-1]  = application data
  payload[N..N+3]  = crc32(payload[0..N-1])   # 4 bytes appended
  DMA → host_tx_in FIFO → HLS TX chain (treats CRC bytes as plain data)

RX (LiteX firmware):
  bytes = DMA read from host_rx_out FIFO
  if crc32(bytes[0..len-5]) != unpack_u32(bytes[len-4..len-1]):
      discard frame, signal upper layer (retransmit / NAK)
  else:
      deliver bytes[0..len-5] to application
```

This keeps the HLS chain as pure PHY (moves bits, no policy). Integrity, retransmit policy,
and sequence numbering are driver responsibilities. The CRC polynomial (CRC-32C or IEEE
802.3) and frame format can be changed without touching any HLS IP or re-running synthesis.

---

#### C6. xfft Pipeline State Undefined at Startup

The `xfft v9.1 pipelined_streaming_io` instance has no reset pin. After FPGA configuration
and `rst_n` deassertion, the xfft internal pipeline holds undefined values. `ofdm_tx` and
`ofdm_rx` assert `ap_start` immediately on the first software trigger. The first FFT output
frame will have up to 256 samples of garbage from the unflushed pipeline contaminating the
very first preamble and header symbols.

On the RX side, the first channel estimate will be computed from garbage FFT output,
producing a completely wrong `G_eq`. Every data symbol in that first frame will fail to
decode. This may look like a sporadic "first frame always fails" symptom that is easy to
dismiss but is actually deterministic.

**Fix:** In the LiteX init sequence, after deasserting reset, send one dummy 256-sample
burst through each xfft before enabling the HLS chain:

```python
# Flush xfft pipeline — send 256 zero samples, discard output
for _ in range(256):
    self.ofdm_tx_ifft_tdata.write(0)
    self.ofdm_tx_ifft_tvalid.write(1)
```

This takes 256 cycles (2.56 µs) and runs once on boot.

---

### 9.3 Serious — Will Degrade Link Quality on Hardware

These cause meaningful performance loss and should be fixed before any range testing.

---

#### S1. IQ Imbalance — EVM Floor Raised from −65 dB to ~−24 dB

The AD9364 has typical IQ imbalance of ±0.5 dB amplitude and ±2° phase at room temperature,
drifting further with temperature. This creates a conjugate image at −f for each subcarrier.
In a 200-SC OFDM system, the image raises the interference floor across the entire band:

```
ICI power ≈ (ε_A)² × signal_power
At 0.5 dB (ε_A ≈ 0.06): ICI floor ≈ −24 dB EVM
```

Your simulation EVM floor is −65 dB (quantization limited). IQ imbalance raises it to
−24 dB — a 41 dB degradation. 16-QAM requires EVM < −24 dB. You will be right at the
margin in a lab at room temperature, and below it over operating temperature.

**Fix:** Enable the AD9364 internal IQ correction calibration via SPI during init
(`RFDC_CAL_MODE = 1` in the AD9364 register map). This is a firmware task — no HLS changes.

---

#### S2. Hard-Decision Viterbi — 2–3 dB Left on the Table

The Viterbi decoder receives hard bits (0 or 1) from the demapper. Soft-decision Viterbi
uses the log-likelihood ratio of each bit — the signed distance from the decision boundary
in the equalized constellation. This gives approximately 2 dB additional coding gain at
`BER = 10⁻³` for the same K=7 code, with no additional bandwidth or power.

The equalized `cgeq_t` values in `equalize_demap_pack()` already contain the soft
information — they are simply hard-sliced and discarded before being passed to the Viterbi.

For a UAV operating at 15–18 dB link margin at max range, 2 dB translates to ~40% more
range for QPSK, or reduces the required transmit power by 37%.

**Fix:** Pass the real/imaginary parts of the equalized symbol directly to the Viterbi as
4-bit or 6-bit LLR values. Modify the demapper to emit soft bits and the Viterbi ACS to
use add-compare-select on LLR sums instead of hard Hamming distances.

---

#### S3. Preamble-Only Channel Estimation — Channel Changes Within Frame

`estimate_channel()` computes `G_eq[k]` once from the ZC preamble and holds it constant
for all data symbols. This assumes a static channel for the entire frame duration.

Frame duration at 20 MSPS: `(255 + 2) × 288 / 20e6 = 3.7 ms`

Channel coherence time at 5.8 GHz for a 120 km/h drone:
```
f_D = v × f_c / c = 33.3 m/s × 5.8e9 / 3e8 = 644 Hz
T_c ≈ 0.423 / 644 Hz ≈ 0.66 ms
```

The frame is **5.6× longer than the channel coherence time**. The channel changes
significantly within one frame at speed. The current CPE tracking compensates for phase
rotation only (one scalar correction per symbol). It does not track amplitude variation,
per-subcarrier fading evolution, or multipath spread changes.

**Fix:** After each data symbol's FFT, update `G_eq` using the 6 BPSK pilots:

```cpp
// For each data symbol, after run_fft():
for (int p = 0; p < NUM_PILOT_SC; p++) {
    G_pilot_new[p] = fft_buf[PILOT_IDX[p]] * conj_zc_pilot[p];
    // Blend: G_eq_pilot = alpha*G_pilot_new + (1-alpha)*G_eq_pilot
}
// Linear interpolate G_eq_pilot → G_eq[k] for all data SCs
```

The 6 pilot positions (spaced ~38 SCs apart) give sufficient spatial coverage for linear
interpolation across 200 data SCs. This is ~20 lines of HLS, ~50 LUT, 0 DSP.

---

#### S4. No Spectral Mask Compliance — Sidelobes at −13 dB

Rectangular-windowed OFDM produces sinc-shaped sidelobes. First sidelobe is −13 dB relative
to the in-band peak, located at approximately ±(bandwidth × 1.5) from center. At 20 MHz
occupied bandwidth, the −13 dB sidelobe falls at ±30 MHz — directly in the adjacent channel
for 20 MHz-spaced channel plans.

The AD9364 TX FIR can suppress this, but only if it is configured with appropriate
coefficients. By default it operates as a passthrough.

**Fix for hardware bring-up:** Load a raised-cosine or root-raised-cosine FIR coefficient
set into the AD9364 TX filter via SPI on init. No HLS changes. This is a one-time SPI
configuration in the LiteX BSP.

**Proper fix:** Apply a Hann or Nutall window to the time-domain symbol after CP insertion
in `ofdm_tx`. Reduces sidelobes to −32 dB at cost of ~3% inter-subcarrier energy spread
(negligible EVM impact). Cost: ~50 LUT in `ofdm_tx`.

---

#### S5. PAPR = 10 dB — Effective Transmit Power 9 dB Below Hardware Maximum

OFDM PAPR for 200 equal-amplitude subcarriers is approximately 9–10 dB (practical, after
natural cancellations). No PAPR reduction is applied. The AD9364 transmit chain must back
off by the PAPR to avoid clipping at the DAC and distortion at the amplifier stage.

At the Hallycon M2 board: the AD9364 maximum output is ~0 dBm. With PAPR = 9 dB, the
average output power is −9 dBm — 9 dB less than the hardware maximum. This directly
reduces link budget and maximum range by approximately 65% (×0.35 in linear distance terms).

**Fix:** Soft clip the time-domain IFFT output in `ofdm_tx` at a threshold of 4σ before
CP insertion. This clips approximately 0.1% of peaks, reduces PAPR from ~9 dB to ~6 dB,
and degrades EVM by less than 0.3 dB (well within 16-QAM tolerance of −24 dB).

```cpp
// After run_ifft(), before insert_cp():
CLIP: for (int i = 0; i < FFT_SIZE; i++) {
    #pragma HLS PIPELINE II=1
    const sample_t clip = sample_t(0.75);  // 4σ for typical OFDM distribution
    if (time_buf[i].real() >  clip) time_buf[i] = csample_t( clip, time_buf[i].imag());
    if (time_buf[i].real() < -clip) time_buf[i] = csample_t(-clip, time_buf[i].imag());
    if (time_buf[i].imag() >  clip) time_buf[i] = csample_t(time_buf[i].real(),  clip);
    if (time_buf[i].imag() < -clip) time_buf[i] = csample_t(time_buf[i].real(), -clip);
}
```

Cost: ~30 LUT. Gain: 3 dB average transmit power = 40% more range at same BER.

---

#### S6. AXI-Stream FIFO Depth Not Specified — Silent Sample Drops

The block design connects all chain stages via AXI-Stream. Vivado auto-inserts FIFOs of
default depth 512 words between each block. One frame = up to 74,016 words. With `ap_ctrl_hs`
blocks, each stage starts only after the host writes `ap_start`. If there is any inter-stage
startup latency (even one clock of software overhead), the upstream block outputs into a 512-
word FIFO that overflows in `512 / 100e6 = 5 µs`. At 100 MHz with 20 MSPS data, samples are
being written faster than the FIFO drains. Overflow means silent data loss — no error flag,
no backpressure stall.

**Fix:** In `create_project.tcl`, explicitly size each inter-block FIFO:

```tcl
create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 fifo_sc_to_cfo
set_property -dict [list CONFIG.FIFO_DEPTH {131072}] [get_bd_cells fifo_sc_to_cfo]

create_bd_cell -type ip -vlnv xilinx.com:ip:axis_data_fifo:2.0 fifo_cfo_to_rx
set_property -dict [list CONFIG.FIFO_DEPTH {131072}] [get_bd_cells fifo_cfo_to_rx]
```

Two FIFOs × 131,072 × 32-bit words = 1 MB total = 4 BRAM_36. Within the 75 BRAM_36
budget (currently using ~22).

---

### 9.4 Performance Gap vs Defence-Grade Systems

The following table shows where this design stands against L3-Harris, Collins JTRS-class,
and Silvus StreamCaster-class products. Items marked with a fix path can be closed without
redesigning the baseband.

| Feature | This design | Target grade | Gap | Fix path |
|---------|-------------|--------------|-----|----------|
| FEC | Hard-decision K=7 Viterbi | Soft LDPC r=1/2 | ~5 dB | Soft Viterbi (S2): +2 dB now |
| Channel tracking | Preamble-only + CPE | Per-symbol MMSE pilot interpolation | ~2 dB at speed | S3 — 20 lines HLS |
| PAPR | None | Clipping + filtering | 3 dB ERP | S5 — 30 LUT |
| IQ calibration | None | Hardware IQ correction | 1–2 dB EVM | S1 — SPI config |
| Spectral mask | Raw OFDM sidelobes −13 dB | −40 dB sidelobes | Regulatory | S4 — AD9364 FIR |
| AMC | Fixed MCS per session | Per-frame SNR-adaptive MCS | Link robustness | SNR output from ofdm_rx |
| ARQ | None | Selective repeat HARQ | Reliability | Software layer |
| Authentication | None | AES-256-GCM + ECDSA | Safety-critical | Software layer |
| Frequency agility | Fixed channel | Adaptive FHSS / DFS | Jamming resistance | Multi-channel plan |
| Diversity | Single antenna | 2×1 MRC at minimum | 3–6 dB SNR | Second AD9364 |
| Waveform signature | Fixed ZC root u=25 | Randomized root per session | Detectability | Configurable LUT |

**Achievable before first hardware test** (fixes S1–S5 plus C1–C6):

Closing these items produces a link that beats every hobby SDR system (OpenHD, wfb-ng,
DJI OcuSync 1) and approaches commercial UAV datalink products (DJI OcuSync 3 uses LDPC
+ 2×2 MIMO; this design gets within 3–4 dB without MIMO).

---

### 9.5 Priority Fix List

Items 1–8 must be completed before any hardware power-on. Items 9–13 before range testing.

| # | ID | Issue | Effort | Impact |
|---|----|----|---|---|
| 1 | C1 | Fix n_syms feedback — hard-code n_syms=255 in sync_detect | 1 hr HLS | Critical: prevents stream misalignment |
| 2 | C2 | Add 288-sample guard symbol to ofdm_tx send sequence | 30 min HLS | Critical: cold-start sync reliability |
| 3 | C3 | Add 74,100-sample timeout counter to drain loop | 30 min HLS | Critical: prevents permanent deadlock |
| 4 | C4a | Wire cfo_est directly: sync_detect → cfo_correct (BD TCL) | 30 min TCL | Critical: eliminates software round-trip |
| 5 | C4b | Add 131,072-depth input FIFO before sync_detect (4 BRAM_36) | 30 min TCL | Critical: double-buffers live ADC stream |
| 6 | C4c | Add 32,768-depth output FIFO after rx_scrambler (1 BRAM_36) | 15 min TCL | Critical: gives DMA a read window |
| 7 | C5 | CRC-32 in LiteX driver (TX append, RX check+strip) | 1 hr firmware | High: silent corruption detection, no HLS cost |
| 8 | C6 | Flush xfft pipelines in LiteX init (256 zero samples each) | 1 hr Python | Critical: first-frame channel estimate |
| 9 | S1 | Enable AD9364 IQ calibration in SPI init sequence | 2 hr firmware | High: +1–2 dB EVM |
| 10 | S5 | Add soft PAPR clipping at 4σ in ofdm_tx after IFFT | 2 hr HLS | High: +3 dB ERP, +40% range |
| 11 | S4 | Load raised-cosine coefficients into AD9364 TX FIR | 2 hr firmware | High: spectral compliance |
| 12 | S3 | Per-symbol pilot channel update in ofdm_rx data loop | 2 days HLS | High: +2 dB at UAV speed |
| 13 | S2 | Soft-decision LLR Viterbi decoder | 1 week HLS | High: +2–3 dB coding gain |

---

## 10. DSP Utilization Analysis — `ofdm_rx` (post-LiteX-integration)

### 10.1 The question

From the LiteX post-implementation report (`litex/build/hallycon_m2sdr_platform/gateware/hallycon_m2sdr_platform_utilization_place.rpt`):

| Resource | Used | Avail | % |
|---|---:|---:|---:|
| DSP48E1 | **110** | 120 | **91.7%** |

Per-block DSP breakdown (full SoC):

| Block | DSP |
|---|---:|
| `ofdm_rx` | **63** |
| `sync_detect` | 29 |
| `ofdm_rx_fft` (xfft v9.1) | 9 |
| `ofdm_tx_ifft` (xfft v9.1) | 9 |
| Everything else (MAC, FEC, tx_chain, PCIe) | 0 |
| **Total** | **110** |

`sync_detect` using 29 DSP is understood (integer cross-multiply + CORDIC atan2 + deliberate wide arithmetic for reliable correlation). The question is where `ofdm_rx` spends its **63 DSP**, given that FFT is already factored out into a separate IP and Viterbi uses zero DSP.

### 10.2 Hardware constraint: DSP48E1 operand width

DSP48E1 on Artix-7 is a hardware multiplier with fixed operand widths:

```
       A input: 25 bits (signed)
       B input: 18 bits (signed)
    ──────────────────────────
       product: 43 bits
```

Any scalar multiply whose operands exceed **25×18** must be decomposed into multiple DSP48E1 tiles by Vivado. For signed `ap_fixed<32,10>` × `ap_fixed<32,10>`:

- A = 32 bits → does not fit in 25-bit A port → split
- B = 32 bits → does not fit in 18-bit B port → split
- **Cost: 2 DSP48E1 per scalar multiply**

For a **complex multiply** `(a_re + j·a_im) × (b_re + j·b_im)` = `(a_re·b_re − a_im·b_im) + j·(a_re·b_im + a_im·b_re)`:

- 4 real multiplies × 2 DSP each = **8 DSP per complex multiply** at `<32,10>` precision

This is the single dominant cost driver for `ofdm_rx`.

### 10.3 Per-operation DSP breakdown

From `hallycon_m2sdr_platform_utilization_hierarchical_place.rpt`, filtered to `ofdm_rx` subtree:

| Block | DSP | Source | Operation |
|---|---:|---|---|
| `estimate_channel → CH_DATA` | 8 | `ofdm_rx.cpp:410-420` | 200-SC pipelined complex mul `G[d] = Y_pre · conj(ZC)` |
| `estimate_channel → mul_21s_40s` | 2 | `ofdm_rx.cpp:440-464` | One scalar mul in `inv · g_re` / `inv · g_im` reciprocal path |
| `estimate_channel` (hidden leaves) | 5 | `ofdm_rx.cpp:440-464` | HLS sequential divide / reciprocal internal mults — not broken out in report |
| `compute_pilot_cpe → CPE_PILOTS` | 8 | `ofdm_rx.cpp:539-544` | 6-iter pipelined `equalize_sc` complex mul `Y · G_eq` |
| `equalize_demap_pack → QPSK_PACK` | 16 | `ofdm_rx.cpp:645-656` | Per-SC `equalize_sc` (8) + `apply_cpe` (8) — two complex muls |
| `equalize_demap_pack → QAM16_PACK` | 4 | `ofdm_rx.cpp:660-672` | Same math as QPSK; HLS shared multipliers across mod-branch mutex |
| `mul_40s_67ns_106_5_1_U154` (top-level shared) | 8 | `ofdm_rx.cpp:167-169` | `angle × (2³²/2π)` wide scalar mul in `angle_to_phase_acc()` |
| Cross-hierarchy residual | 12 | — | DSPs registered at parent level after Vivado cross-module sharing |
| **Total** | **63** | | |

Equivalent view grouped by operation type:

| Operation (algebraic) | Where | DSP |
|---|---|---:|
| `Y_pre · conj(ZC)` complex mul | `estimate_channel` CH_DATA, preamble only | 8 |
| Reciprocal `1/|G|²` path (mul + divide internals) | `estimate_channel` CH_EQ + CH_PILOT | 7 |
| `Y · G_eq` complex mul, 6 pilots | `compute_pilot_cpe`, per symbol | 8 |
| `Y · G_eq` complex mul, 200 data SCs | `equalize_sc` in QPSK_PACK, per symbol | 8 |
| CPE rotation `x̂ · r_s` complex mul | `apply_cpe` in QPSK_PACK, per symbol | 8 |
| `angle × K` wide scalar mul | `angle_to_phase_acc()`, per symbol | 8 |
| QAM16 residual (shared with QPSK) | `equalize_demap_pack` QAM16_PACK | 4 |
| Cross-hierarchy / placement residual | Vivado post-placement | 12 |
| **Total** | | **63** |

**Root cause:** every complex multiply in the hot path runs in `ap_fixed<32, 10>`. This width is a direct consequence of `G_eq ≈ 256` (from the FFT ÷256 scale schedule 0xAA) needing ≥ 10 integer bits of range, combined with the decision to keep 22 fractional bits of precision. The wide precision pushes every scalar mul to 2 DSP and every complex mul to 8 DSP. Narrow input types (`ap_fixed<16,1>` I/Q from AD9361) are preserved losslessly and are *not* the driver — only the post-FFT equalizer arithmetic is.

### 10.4 Algebraic structure of the RX hot path

Naming:

| Symbol | Type | Meaning |
|---|---|---|
| `y[k]` | complex | FFT bin at subcarrier k (post-FFT, ÷256 scaled) |
| `z[d]` | complex | ZC reference at data SC d, `|z| = 1` |
| `g[d]` | complex | channel estimate = `y_pre[k] · conj(z[d])` |
| `geq[k]` | complex | equalizer = `conj(g) / |g|²` |
| `φ_s` | real | pilot CPE angle for data symbol s |
| `r_s` | complex | CPE rotator = `cos(φ_s) − j·sin(φ_s) = e^(−j·φ_s)` |
| `x̂[k]` | complex | equalized + CPE-corrected data |

Preamble (once per frame):

```
(A1)  g[d]   = y_pre[k] · conj(z[d])                                        — complex mul [8 DSP]
(A2)  geq[k] = conj(g[d]) / |g[d]|²                                         — divide + 2 real mul [~7 DSP]
```

Header + each data symbol s:

```
(B1)  φ_s     = atan2( Σ Im{ y[p]·geq[p] }, Σ Re{ y[p]·geq[p] } )           — 6 complex mul [8 DSP]
(B2)  ph_acc  = φ_s · (2³² / 2π)                                            — wide scalar mul [8 DSP]
(B3)  (cos_s, sin_s) = LUT(ph_acc[31:22])                                   — no DSP
(B4)  x̂[k]   = ( y[k] · geq[k] ) · r_s                                    — TWO complex mul per SC [16 DSP]
                 └─ equalize_sc ─┘ └─ apply_cpe ─┘
```

### 10.5 Optimization path

#### Opt-1 — CORDIC rotator replaces `apply_cpe` + `angle_to_phase_acc` (saves 16 DSP)

`apply_cpe(sym, cos, sin)` implements a 2-D rotation by `−φ_s`. This is the canonical use case for **CORDIC rotation mode** — shift-and-add only, zero multipliers. The existing `fixed_atan2_rx` (`ofdm_rx.cpp:114-158`) is CORDIC in *vectoring* mode; rotation mode is the same pipeline with the sign-decision driven by the residual angle instead of by `y`.

Replacing the multiplier chain with a CORDIC rotator collapses three blocks at once:

| Block | Before | After | Δ |
|---|---:|---:|---:|
| `apply_cpe` (4 real mul) | 8 DSP | 0 DSP | **−8** |
| `sincos_lut_rx` + LUT ROM | 0 DSP (1 BRAM) | removed | — |
| `angle_to_phase_acc` (`angle × K`) | 8 DSP | removed | **−8** |
| **Sub-total** | **16 DSP** | **0 DSP** | **−16** |

The CORDIC rotator takes `φ_s` (in `ap_fixed<32,4>` radians) directly — the whole `angle → phase_acc → LUT → cos/sin` chain disappears. Latency cost: ~16 extra cycles per symbol, well inside the per-symbol budget (288-sample symbol at 100 MHz = 2880 cycles).

**Risk:** low — identical algebra, existing CORDIC infrastructure in-tree, standard in WiFi/LTE reference implementations.

#### Opt-2 — Fold CPE rotation into `geq` via algebraic regrouping (saves ~4 DSP)

`x̂[k] = y[k] · geq[k] · r_s = y[k] · (geq[k] · r_s) = y[k] · geq_s[k]`

`r_s` is a **single scalar per symbol**, identical across all 200 SCs. Precompute `geq_s[k] = geq[k] · r_s` once per symbol in a sharing-friendly loop (high II), then use one complex mul in the hot path:

| Path | Before | After |
|---|---:|---:|
| `equalize_sc` hot path | 8 DSP (II=1) | 8 DSP (II=1, unchanged) |
| `apply_cpe` hot path | 8 DSP (II=1) | — |
| `geq_s` precompute loop | — | 4 DSP (II=4, HLS ALLOCATION) |
| **Sub-total** | **16 DSP** | **12 DSP** (**−4**) |

**Subsumed by Opt-1** — if Opt-1 is adopted, Opt-2 is not needed. List here only as a fallback if CORDIC rotator is rejected.

#### Opt-3 — Narrow `geq_t` from `ap_fixed<32,10>` → `ap_fixed<18,10>` (saves 10–20 DSP)

`ap_fixed<18, 10>` keeps the same ±512 range (required for `geq ≈ 256`) but reduces fractional bits from 22 to 7. Operand fits in DSP48E1's 18-bit B port, so each scalar multiply drops from **2 DSP → 1 DSP**.

Precision impact: relative error on `geq ≈ 256` is `1/2⁷ / 256 ≈ 0.003%`. Well below typical channel-estimation noise (dominated by AWGN at SNR < 30 dB). Must be validated with a BER sweep before commit.

Affects every complex mul in `estimate_channel`, `compute_pilot_cpe`, and `equalize_demap_pack`. Approximate saving: 10–20 DSP depending on how many paths Vivado narrows versus how many stay at the full `<32,10>` width for headroom.

**Risk:** medium — requires end-to-end BER validation across AWGN / phase-noise / multipath / combined channels at all mod/rate points. Reuse existing `run_ber_sweep.sh`.

#### Opt-4 — QPSK sign-only narrow-path (saves ~4 DSP, conditional)

`qpsk_demap` uses only `sign(Re{x̂})` and `sign(Im{x̂})`. The magnitude of `x̂` is irrelevant for QPSK. If `y` and `geq_s` are narrowed to `ap_fixed<16,1>` × `ap_fixed<16,1>` on the QPSK-only branch, each scalar mul drops to 1 DSP (fits 16×16 in one DSP48E1). 16-QAM still needs the wider path because `qam16_demap` threshold-compares against `±2/√10`, which requires magnitude.

**Risk:** medium — adds a mod-dependent path split. Only worth doing if Opt-3 is insufficient.

### 10.6 Summary and recommendation

| # | Optimization | DSP saved | Effort | Risk | Validation |
|---|---|---:|---|---|---|
| Opt-1 | CORDIC rotator replaces `apply_cpe` + `angle_to_phase_acc` | **−16** | 2 days HLS | low | C-sim EVM + BER sweep |
| Opt-2 | Fold `r_s` into `geq_s` (fallback if Opt-1 rejected) | −4 | 4 hr HLS | low | C-sim equivalence |
| Opt-3 | Narrow `geq_t` to `ap_fixed<18,10>` | −10 to −20 | 4 hr HLS + sweep | medium | Full BER sweep, all mod/rate/channel |
| Opt-4 | QPSK sign-only narrow path | −4 | 1 day HLS | medium | QPSK BER sweep |

**Recommended order:** Opt-1 first (highest-value single change, low risk, reuses existing CORDIC pattern). Then Opt-3 with BER validation. Skip Opt-2 if Opt-1 is adopted. Opt-4 only if DSP budget is still tight after Opt-1 + Opt-3.

**Projected DSP count for `ofdm_rx` after Opt-1 + Opt-3:** 63 − 16 − 15 ≈ **32 DSP** (≈50% reduction). Frees ~31 DSP at the full SoC level, bringing DSP utilization from 91.7% → ~66% on `xc7a50t`, restoring real headroom for future features (soft Viterbi LLR paths, per-symbol pilot update S3, diversity combining).

