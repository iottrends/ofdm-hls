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

## 8. Outstanding Issues

### 8a. Vivado standalone synthesis trims design to ~7 LUT

**Root cause:** All HLS blocks use `ap_ctrl_hs` handshake. In the standalone block design
there is no AXI-Lite master to assert `ap_start`, so all state machines stay idle. Vivado
correctly determines all outputs are constant and prunes the logic. The post-implementation
report (`vivado/utilization_post_impl_summary.rpt`) therefore shows 7 LUT / 8 FF, not ~34K.

`set_false_path -from [all_inputs] -to [all_outputs]` in the XDC prevents timing-driven
trimming of unplaced ports but does not prevent ap_ctrl_hs idle-state pruning.

**Real fix required:** LiteX integration via `docs/files/ofdm_subsystem.py`. LiteX
generates the AXI-Lite interconnect + CSR register map that drives `ap_start` for all
HLS blocks. Only then will Vivado see a fully driven design and report accurate utilization.

### 8b. synth_ip [get_ips *] error (non-fatal)

```
ERROR: [Vivado 12-3424] IPI cores may not be directly generated.
```

`get_ips *` returns BD sub-IP instances (xfft, xlconstant) which cannot be synthesized
with `synth_ip`. The line in `create_project.tcl` should be removed — `synth_design`
handles everything. Currently non-fatal (Vivado continues) but generates noise.

### 8c. LUT budget — may be fine after integrated synthesis

The ~34,425 LUT estimate is ~4% over the 32,600 available on XC7A50T. This is from
summed per-block standalone estimates. Integrated synthesis typically reduces 10–20%
through cross-boundary optimization and LUT combining. Realistic expectation: ~28K–30K.

If the integrated design is still over budget:
1. **viterbi_dec_0 (9,448 LUT)** — replace with Xilinx SD-FEC core, or reduce K from 7→5
2. **ofdm_rx_0 geq_t narrowing** — `ap_fixed<32,10>` → `ap_fixed<18,10>` saves ~4 DSP
3. **ofdm_rx_0 angle_to_phase_acc** — simplification saves ~6–8 DSP

### 8d. Remaining hardware integration steps

| Item | Status |
|------|--------|
| All 10 HLS IPs synthesized + exported | Done |
| Vivado block design + xfft connections | Done |
| LiteX integration (ofdm_subsystem.py) | **TODO — required for real bitstream** |
| AD9364 LVDS bring-up on hardware | TODO |
| Over-the-air loopback test (drone ↔ ground) | TODO |
