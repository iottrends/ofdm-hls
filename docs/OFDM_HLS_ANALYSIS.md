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

```cpp
// ofdm_tx.h
typedef ap_fixed<16, 1> sample_t;  // Q0.15: range [-1, +1), LSB = 2^-15

// ofdm_tx.cpp — run_ifft()
hls::fft<fft_cfg>(in, out, /*fwd_inv=*/false, /*scale_sch=*/0xAA);
//                          Radix-4, 4 stages: 10_10_10_10 → ÷4 each → ÷256 = matches numpy.ifft

// ofdm_rx.cpp — run_fft()
hls::fft<fft_cfg>(in, out, /*fwd_inv=*/true,  /*scale_sch=*/0xAA);
//                          Same Radix-4 ÷256 schedule → Y[k] = X[k]/256

// ofdm_tx.cpp / ofdm_rx.cpp — pilots
freq_buf[PILOT_IDX[p]] = csample_t(sample_t(0.999969), sample_t(0));
//                                  max Q0.15 value = 1 - 2^-15  (+1.0 wraps to -1.0)

// ofdm_reference.py — pilots (both TX and RX reference)
freq[p] = 0.999969 + 0j   # match ap_fixed<16,1> max

// ofdm_reference.py — IFFT/FFT
time_domain = np.fft.ifft(freq_domain)   # TX: ÷N, matches scale_sch=0xAA IFFT
freq_domain = np.fft.fft(time_domain)    # RX: unnormalized, Y[k]=X[k] directly
```

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

### Current resource usage (Artix-50T, xc7a50tcsg324-1, 10 ns clock)

| Resource | Available | TX | TX % | RX | RX % | Combined | Combined % |
|----------|-----------|-----|------|-----|------|----------|------------|
| LUT      | 32,600    | 16,387 | 50% | 19,022 | 58% | 35,409 | **109%** |
| FF       | 65,200    | 16,828 | 25% | 29,174 | 44% | 46,002 | 71% |
| DSP      | 120       | 18  | 15% | 69  | 57% | 87  | 73% |
| BRAM_18K | 150       | 28  | 18% | 31  | 20% | 59  | 39% |

TX timing: passes. RX timing: 7.124 ns, 2.876 ns slack ✓
TX and RX are separate IP blocks — individually they fit. Combined LUT is 9% over (TX on
drone end, RX on ground end → not co-located on same chip in typical deployment).

---

## 7. Roadmap — Remaining Work

### Phase 2 — RX robustness (needed before hardware)

#### 2a. Pilot-aided per-symbol channel tracking (TODO)
Current limitation: channel is estimated ONCE from the preamble ZC symbol and applied to
ALL subsequent data symbols unchanged. In a static channel (simulation) this is fine. In
a real channel (Doppler, phase drift, multipath variation over time) it will degrade.

**What's missing:**
- 6 pilot subcarriers (BPSK, index 50/75/100/154/179/204) are already inserted by the TX
  but the RX currently ignores them completely.
- Per-symbol: extract pilot subcarriers from FFT output → compute phase/amplitude error
  → interpolate correction across 200 data SCs → apply on top of G_eq.
- This gives per-symbol tracking without recomputing the full G_eq.

**Implementation plan:**
```
For each data symbol:
  Y_pilot[p] = fft_out[PILOT_IDX[p]]           // received pilot
  err[p]     = Y_pilot[p] * conj(G_eq_pilot[p])// phase/amplitude error per pilot
  interp_corr[k] = interpolate(err, DATA_SC_IDX[k]) // linear interp across data SCs
  X_hat[k]   = equalize_sc(Y[k], G_eq[k]) * interp_corr[k]
```

#### 2b. Schmidl-Cox timing synchronisation (TODO)
Current limitation: RX assumes the first sample is exactly the start of the preamble CP.
In a real system the packet arrives at an unknown time offset.

**What it does:** slides a correlator over the input stream, computes
`P[n] = Σ r[n+d] * conj(r[n+d+N/2])` over a half-symbol window. The plateau in |P[n]|²
marks the CP region; the peak marks the start of the preamble symbol body.

**HLS implementation sketch:**
```
sliding_correlator() → outputs timing_offset (ap_uint<16>)
Then: skip timing_offset samples before remove_cp_and_read()
```

#### 2c. CFO estimation and correction (TODO)
**What it does:** the phase of `P[n]` at the plateau gives the fractional CFO:
`CFO = angle(P[n]) / (π × N/2)`
Correct by multiplying each time-domain sample by `exp(-j×2π×CFO×n/N)` before FFT.

**HLS implementation:** CORDIC-based complex rotation per sample (hls::cordic available).

---

### Phase 3 — TX additions (needed before hardware)

#### 3a. FEC — Forward Error Correction (TODO)
**Where it fits:** before `unpack_bits()` in the TX, after `equalize_demap_pack()` in the RX.

**Options (in order of complexity):**
1. **Convolutional code (rate 1/2, constraint 7)** — same as 802.11a. Viterbi decoder on RX.
   Simple, well-understood, moderate BER gain (~5 dB coding gain at BER=1e-3).
2. **LDPC** — better performance, much more complex decoder. Defer to later phase.
3. **Reed-Solomon** — good for burst errors, less effective for AWGN. Not ideal here.

**Recommendation:** Start with rate-1/2 convolutional + Viterbi. This is sufficient for
the drone VTX use case and fits within remaining FPGA resources.

#### 3b. Scrambler / descrambler (TODO)
Prevents long runs of 0s or 1s that can cause DC bias in the OFDM spectrum.
Standard 802.11a scrambler: LFSR with polynomial x⁷ + x⁴ + 1.
Trivial to implement in HLS (~10 LUTs).

#### 3c. Puncturing (TODO)
Works with the convolutional encoder to produce rates 3/4, 2/3 from the base rate 1/2.
Increases throughput at the cost of coding gain. Implement after base FEC works.

---

### Phase 4 — Scale-up test (immediate next step)

**Run loopback scripts with 16-QAM and 255 data symbols** (not just 4):

Changes needed in testbenches and scripts:
- `ofdm_tx_tb.cpp`: `TB_N_SYMS = 255`, `TB_MOD = 1` (16-QAM)
- `ofdm_rx_tb.cpp`: same
- `ofdm_reference.py`: `N_SYMS = 255`, `MOD = 1`
- `run_loopback.sh` / `run_loopback_noisy.sh`: pass `--mod 1 --nsyms 255`

Expected:
- 255 × 100 bytes = 25,500 bytes per packet (16-QAM, 4 bps)
- Throughput: 255 × 200 × 4 bits / (256 × 255 × 1/20MHz) ≈ 55.6 Mbps
- HLS stream depth will increase: 256 × (255+1) = 65,536 samples in flight
- Verify HLS stream FIFOs are large enough (check `hls::stream` depth warnings)

---

### Summary of what's missing before hardware tape-out

| Item | Phase | Priority |
|------|-------|----------|
| 16-QAM + 255 symbol loopback test | 4 | **Now** |
| Pilot-aided per-symbol tracking | 2a | High |
| Schmidl-Cox timing sync | 2b | High — required for real hardware |
| CFO estimation/correction | 2c | High — required for real hardware |
| Convolutional FEC (rate 1/2) + Viterbi | 3a | Medium |
| Scrambler/descrambler | 3b | Low |
| Puncturing (rate 3/4) | 3c | Low |
| AXI-DMA integration + PL/PS interface | HW | After above |
| AD9364 bring-up and loopback on hardware | HW | Final |
