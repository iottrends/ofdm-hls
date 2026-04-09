# OFDM HLS Transceiver — Code Review

**Date**: 2026-04-08  
**Reviewer**: Claude Sonnet 4.6  
**Scope**: Full PHY review — correctness, hardware readiness, comparison with openwifi-sdr and GNU Radio OFDM, product perspective, Artix-7 50T resource fit, LiteX/AD9361 integration  
**Design target**: 40k OFDM symbols/sec, drop-in between LiteX TX/RX FIFOs and AD9361-HDL

---

## Design Summary

| Parameter | Value |
|---|---|
| FFT size | 256 points |
| Cyclic prefix | 32 samples (12.5% overhead) |
| Symbol duration | 288 samples (14.4 µs at 20 MSPS) |
| Data subcarriers | 200 |
| Pilot subcarriers | 6 × BPSK (+1), positions {50,75,100,154,179,204} |
| Guard/null subcarriers | 50 (DC, Nyquist, upper/lower bands) |
| Modulation | QPSK (2 bps/SC) or 16-QAM (4 bps/SC) |
| Preamble | 1 Zadoff-Chu symbol (root u=25, N=256) |
| FEC | K=7 convolutional, rate 1/2 or 2/3 (802.11a puncture) |
| Viterbi | Sliding-window hard-decision, WIN=96 |
| Fixed-point | ap_fixed<16,1> for samples (Q0.15) |
| Equalization | Preamble-based, multiply-only hot path (precomputed G_eq) |

---

## What Is Solid

- **DATAFLOW pipeline in TX** is well-structured: `unpack_bits → fill_freq_buffer → run_ifft → insert_cp_and_send` with correct ping-pong array handoff between stages. The `SYMBOL_LOOP` overlaps symbol N+1 packing with symbol N IFFT — this is the right approach.

- **ROM-indexed `DATA_SC_IDX`** achieving II=1 in `fill_freq_buffer` (`ofdm_tx.cpp:213`). Using a precomputed index ROM rather than runtime null/pilot checks is what enables the II=1 pipeline. Good.

- **Precomputed G_eq in channel estimator** (`ofdm_rx.cpp:207–218`). Avoiding 200 dividers in the equalization hot path by computing `conj(G)/|G|²` once per preamble and multiplying thereafter is correct resource reasoning. The `CH_EQ` loop intentionally has no PIPELINE pragma to reuse a single divider — well-commented.

- **ap_fixed<32,10> through demap** (`ofdm_rx.cpp:237–251`). Not truncating equalized values back to ap_fixed<16,1> before the decision slicer prevents silent wrap-around on noisy samples. Correct.

- **Sliding-window Viterbi** (~1 BRAM_18K for 192×64-bit circular buffer vs megabytes for full-trellis). Correct tradeoff for Artix-50T.

- **ZC preamble choice**: Constant envelope in time (zero PAPR), excellent autocorrelation properties — better than the repeated short sequences in 802.11a STF for a custom waveform.

- **Python reference models with BER sweep**: Cross-verifying HLS output against an independent floating-point model (seed=42, AWGN sweep 0–30 dB) is the right verification methodology. The `run_loopback.sh` integration test is good practice.

- **FFT scale_sch = 0xAA (÷4/stage = ÷256 total)** is the correct setting to prevent overflow in all Radix-4 butterfly stages for full-scale OFDM inputs. The comment in `ofdm_rx.cpp:112–118` explaining why 0x55 must not be used is accurate.

---

## Critical Bugs / Hardware-Killers

### 1. Timing sync will false-trigger immediately on real hardware

**File**: `sync_detect.cpp:72–79`

```cpp
ONSET: for (int t = 0; t < SEARCH_WIN && !onset_found; t++) {
    ap_fixed<32,4> energy = buf_i[t]*buf_i[t] + buf_q[t]*buf_q[t];
    if (energy > ap_fixed<32,4>(0)) {   // threshold = 0
        best_t = t; onset_found = true;
    }
}
```

ADC thermal noise quantized through ap_fixed<16,1> is never exactly zero. The block will lock onto the first ADC noise sample, every call. The comment says "set to calibrated noise floor for hardware" but there is no mechanism to load that threshold — it is hardcoded to zero.

Additionally, the `&& !onset_found` conditional break prevents HLS from pipelining this loop. HLS synthesizes it as a sequential FSM — up to 576 sequential clock cycles for the search — and no `#pragma HLS PIPELINE` is applied.

**Required fix**: Replace energy onset with a normalized Schmidl-Cox delay-and-correlate:

```
M(d) = |P(d)|²     where P(d) = Σ_{k=0}^{N/2-1} r[d+k] · conj(r[d+k+N/2])
R(d) = Σ_{k=0}^{N/2-1} |r[d+k+N/2]|²
metric(d) = M(d) / R(d)²     (normalized, threshold-independent)
```

Or use a sliding cross-correlator against the known ZC sequence in the frequency domain (preamble FFT × conj(ZC_FFT) → peak magnitude = timing and integer CFO simultaneously). This is what openwifi's `sync_short` and GNU Radio's `ofdm_sync_sc_cfb` implement.

---

### 2. Per-symbol pilot tracking is absent — pilots inserted but never read at RX

**TX file**: `ofdm_tx.cpp:206–208` — 6 BPSK pilots inserted every data symbol  
**RX file**: `ofdm_rx.cpp:341–376` — only `DATA_SC_IDX` positions processed; pilot positions skipped entirely

The 6 pilots (indices 50, 75, 100, 154, 179, 204) are discarded at the RX. The channel estimate `G_eq` computed from the preamble is used unchanged for all data symbols.

With any residual CFO after correction, or oscillator phase noise, a **common phase error (CPE)** accumulates linearly with symbol index. At 20 MSPS, 256-pt FFT, one subcarrier = 78.125 kHz. With ±0.01 subcarrier residual CFO:

```
Accumulated phase on symbol 255 = 255 × 288 × 2π × 0.01 / 256 ≈ 0.71 rad ≈ 41°
```

16-QAM decision margins are ≈ 15°. Symbol 255 will have unacceptable BER regardless of channel SNR.

**Required fix**: After each data symbol FFT, read the 6 pilot subcarriers, compare against the known +1 reference, compute the average phase rotation:

```cpp
float phase_err = 0;
for (int p = 0; p < NUM_PILOT_SC; p++) {
    cgeq_t Yp = equalize_sc(fft_out[PILOT_IDX[p]], G_eq[PILOT_IDX[p]]);
    phase_err += hls::atan2f((float)Yp.imag(), (float)Yp.real());
}
phase_err /= NUM_PILOT_SC;
// Apply -phase_err rotation to all equalized symbols before demapping
```

This is what openwifi's `equalizer_update` and GNU Radio's `ofdm_frame_equalizer_vcvc` implement.

---

### 3. No in-band frame signaling — RX cannot be autonomous

**File**: `ofdm_rx.h:27`

```cpp
void ofdm_rx(hls::stream<iq_t>&, hls::stream<ap_uint<8>>&, mod_t mod, ap_uint<8> n_syms);
```

Both `mod` (modulation type) and `n_syms` (frame length) must be pre-loaded via AXI-Lite before the packet arrives. The receiver has no way to determine these from the received signal itself.

For a fixed point-to-point link with CPU coordination this is workable. For any dynamic link it is not. Minimum viable solution: a header symbol immediately after the preamble, always sent at BPSK rate 1/2, carrying:

| Bits | Field |
|---|---|
| [7:6] | MCS (00=QPSK r1/2, 01=QPSK r2/3, 10=16QAM r1/2, 11=16QAM r2/3) |
| [15:8] | n_syms (frame length) |
| [31:16] | Header CRC-16 |

The receiver decodes this first, then configures itself for the payload.

---

### 4. Integer CFO not estimated or corrected

**File**: `sync_detect.cpp:89–105`

CP-based CFO estimation gives only **fractional** offset: `|ε_sc| ≤ 0.5 subcarrier`. At 20 MSPS / 256 points, one subcarrier = 78.125 kHz. An AD9361 at 2.4 GHz with ±25 ppm crystal gives ±60 kHz ≈ ±0.77 SC — outside the CP estimation range. The system will fail if the combined TX+RX crystal offset exceeds 0.5 subcarrier spacing.

Detection of integer CFO requires a frequency-domain cross-correlation of the preamble FFT against the known ZC spectrum. The peak location gives both fine timing and integer subcarrier offset simultaneously.

---

## Performance / Correctness Issues

### 5. Header comment vs. code mismatch — scale_sch

**File**: `ofdm_rx.h:11` vs `ofdm_rx.cpp:122`

The header states:
```
// FFT: forward, scale_sch=0x55 (Radix-4 ÷16).
// TX IFFT uses ÷256; FFT of those samples × (1/16) → Y[k] = X[k]/16.
```

The code uses `0xAA` (÷256), giving `Y[k] = X[k]/256` and `G_eq ≈ 256`. The code comment in `ofdm_rx.cpp:107–118` is correct. The header is wrong/stale. This will mislead anyone using the header as the specification.

**Fix**: Update `ofdm_rx.h:11` to:
```
// FFT: forward, scale_sch=0xAA (Radix-4 ÷256 total).
// TX IFFT also uses ÷256; round-trip: Y[k] = X[k]/256. G_eq ≈ 256.
```

---

### 6. CFO correction uses floating-point per-sample — expensive on Artix-50T

**File**: `cfo_correct.cpp:51–71`

```cpp
float phase = -(float)n * delta_phi;    // float multiply every sample
hls::sincosf(phase, &sin_p, &cos_p);   // 15-cycle latency float CORDIC
```

`hls::sincosf` maps to a single-precision floating-point CORDIC in synthesis (Vivado IP). On Artix-7 this consumes approximately 20–30 DSPs and significant LUT routing. Running at the full sample rate (20 MSPS → 1 sample/cycle at ~125 MHz system clock), this is the largest single resource consumer after the FFTs.

The comment at line 65 acknowledges: `"n×delta_phi can be implemented as an accumulator (NCO) for better area"` — this should be done before synthesis.

**Required fix**: Replace with a fixed-point phase accumulator and CORDIC:

```cpp
ap_fixed<32,2> phase_acc = 0;
ap_fixed<32,2> delta_phi_fixed = (ap_fixed<32,2>)(eff_cfo * 2.0f * M_PI_F / FFT_SIZE);

CORRECT: for (int n = 0; n < total_samples; n++) {
#pragma HLS PIPELINE II=1
    // Accumulate phase (no multiplication each cycle)
    phase_acc -= delta_phi_fixed;  // negative: undo CFO advance
    // Fixed-point CORDIC sin/cos (synthesizes to ~6 DSPs)
    // ... rotation ...
}
```

---

### 7. Hard-decision Viterbi — ~2 dB SNR penalty

**File**: `viterbi_dec.cpp` — Hamming distance metric throughout

Soft-decision Viterbi with log-likelihood ratios gives approximately 2 dB coding gain over hard-decision in AWGN, more in fading. For QPSK the LLR per bit is proportional to the equalized real/imaginary value — essentially free to compute:

```cpp
// Hard decision (current):
int bm = (bit ^ coded_bit);   // Hamming: 0 or 1

// Soft decision (improvement):
int16_t llr = (int16_t)((float)equalized_re * snr_est * SCALE);
int bm = abs(llr - coded_val);  // Euclidean: continuous
```

At 40k symbols/sec over real RF the 2 dB matters — it is equivalent to doubling TX power. GNU Radio and openwifi both use soft-decision.

---

### 8. `equalize_demap_pack` — no PIPELINE pragma on outer loop

**File**: `ofdm_rx.cpp:341–376`

```cpp
QPSK_PACK: for (int i = 0; i < NUM_DATA_SC / 4; i++) {
    // 4 complex multiplies + 4 sign comparisons — no #pragma HLS PIPELINE
```

Without an explicit pipeline pragma, HLS may not pipeline this loop if it detects dependency through `bits_out.write()`. Each loop iteration doing 4 subcarriers is also harder to pipeline than 1. Recommended:

```cpp
QPSK_PACK: for (int i = 0; i < NUM_DATA_SC; i++) {
#pragma HLS PIPELINE II=1
    cgeq_t eq = equalize_sc(fft_out[DATA_SC_IDX[i]], G_eq[DATA_SC_IDX[i]]);
    // accumulate bits into byte, write on every 4th (QPSK) or 2nd (16QAM)
```

---

### 9. n_syms capped at 255 — check for your throughput target

**File**: `ofdm_tx.h:76`, `sync_detect.h:46`

`ap_uint<8>` limits frames to 255 data symbols. At 20 MSPS: 255 × 288 = 73,440 samples = 3.67 ms/frame → max 272 frames/sec. For 40k symbols/sec you need each frame to contain ≥ 148 symbols (40000/272). For smaller frames (e.g., n_syms=10 for low-latency), the preamble overhead is 9% and you get only 10 × 200 × 2 / 2 = 2000 payload bits/frame. Sizing n_syms correctly for the use case matters.

---

### 10. ap_fixed<16,1> cannot represent +1.0 — worked around, but fragile

**File**: `ofdm_tx.cpp:208`, `ofdm_tx.cpp:352`

```cpp
freq_buf[PILOT_IDX[p]] = csample_t(sample_t(0.999969), sample_t(0));  // +1.0 wraps
```

The workaround (0.999969 ≈ 1 − 2^−15) is correct but means the BPSK pilot power is 0.000062 below unity. More importantly, if any FFT butterfly accumulation reaches exactly the maximum positive value, it overflows to −1. Using `ap_fixed<16,2>` (Q1.14, range [−2, +2)) eliminates this risk with negligible precision cost for OFDM samples bounded well within ±1.

---

## Missing Functional Blocks (Product Perspective)

| Feature | Priority | Notes |
|---|---|---|
| Proper Schmidl-Cox timing sync | Critical | Hardware won't sync without this |
| Per-symbol pilot phase tracking | Critical | BER degrades after ~50 symbols |
| Frame header (length + MCS) | High | Receiver autonomy |
| Bit interleaver / deinterleaver | High | 3–5 dB loss in multipath without it |
| Data scrambler / descrambler | High | Spectral lines, FEC performance |
| Soft-decision Viterbi | Medium | ~2 dB coding gain |
| CRC / FCS | Medium | Silent packet corruption otherwise |
| Integer CFO correction | Medium | Required if crystals differ > 0.5 SC |
| Fixed-point NCO for CFO | Medium | Artix-50T DSP budget |
| 64-QAM | Low | Throughput ceiling with 16-QAM only |
| Rate adaptation (SNR → MCS) | Low | Fixed MCS regardless of link quality |
| AGC training sequence | Low | AD9361 AGC settling concern |
| TX bypass / loopback CSR | Low | Needed for board bring-up |

### Bit interleaver detail

Without interleaving, a frequency-selective null kills a contiguous run of coded bits feeding the Viterbi — exactly the burst pattern FEC handles worst. 802.11a interleaves coded bits across all subcarriers so a faded subcarrier causes spread single-bit errors rather than burst errors. Implementation is a simple 2D permutation:

```
// 802.11a-style: N_cbps = N_bpsc × N_data
// First permutation:  i → (N_cbps/16) × (i mod 16) + floor(i/16)
// Second permutation: j → s × floor(j/s) + (j + N_cbps - floor(16*j/N_cbps)) mod s
// s = max(N_bpsc/2, 1)
```

In a flat AWGN test (current testbench) there is no difference. Over any real multipath channel, omitting the interleaver costs 3–5 dB effective SNR.

---

## Comparison with Reference Implementations

### vs. openwifi-sdr (https://github.com/open-sra/openwifi)

openwifi implements full 802.11a/g/n PHY on Zynq-7000 and has been verified interoperable with commercial WiFi devices.

| Feature | This design | openwifi |
|---|---|---|
| Preamble | 1 × ZC symbol | STF (10 × 16-sample) + LTF (2 × 64-sample) |
| AGC training | None (ZC only) | STF provides 160 samples for AGC settle |
| Channel estimation | 1 preamble symbol | Average of 2 LTF symbols (3 dB noise reduction) |
| CFO estimation | CP correlation, fractional ±0.5 SC | Coarse from STF repetition + fine from LTF correlation |
| Integer CFO | Not estimated | From LTF cross-correlation |
| Per-symbol tracking | None (pilots discarded) | 4 pilots per symbol at ±7, ±21 |
| FEC | Hard-decision Viterbi | Soft-decision Viterbi with LLR |
| Bit interleaving | None | Full 802.11a frequency-domain interleaver |
| Scrambler | None | 802.11a LFSR (x^7 + x^4 + 1) |
| Frame signaling | Out-of-band only | SIGNAL field: 24 coded bits with MCS + length + parity |
| MAC | None | Full 802.11 MAC: CSMA/CA, backoff, ACK, retransmit |

### vs. GNU Radio `gr-digital` OFDM

| Block | GNU Radio | This design |
|---|---|---|
| Timing sync | `ofdm_sync_sc_cfb`: normalized Schmidl-Cox `\|P\|/R` | Energy onset (threshold=0) |
| Channel estimation | `ofdm_chanest_vcvc`: pilot-based + freq-domain interpolation | Preamble once, no update |
| Per-symbol equalization | `ofdm_frame_equalizer_vcvc`: channel updated every symbol | Static G_eq for all symbols |
| Frame framing | `ofdm_header_payload_demux`: separate header/payload decode | None — mode set out-of-band |
| Integer CFO | Frequency-domain xcorr peak | Not estimated |

The key insight from both reference implementations: the pilots in each data symbol are not decorative. They exist precisely to correct the phase accumulation between the preamble and each data symbol.

---

## Artix-7 50T Resource Estimate

Artix-7 50T resources: **120 DSP48E1s**, **75 BRAM36s** (= 150 BRAM18Ks), **32,600 logic LUTs**.  
*(Note: the 50T has 120 DSPs, not 240. Verify your exact part number — XC7A50T has 120 DSP48E1s.)*

| Block | DSPs | BRAM18s | LUTs (est.) |
|---|---|---|---|
| TX IFFT 256pt (pipelined streaming) | 40–50 | 4 | ~2,500 |
| RX FFT 256pt (pipelined streaming) | 40–50 | 4 | ~2,500 |
| Viterbi K=7 sliding window | 0 | 1–2 | ~800 |
| Convolutional encoder K=7 | 0 | 0 | ~100 |
| CFO correction (float sincosf — current) | **20–30** | 0 | ~3,000 |
| CFO correction (fixed-point NCO — target) | 2–4 | 0 | ~400 |
| sync_detect buffer (864 samples) | 0 | 2 | ~200 |
| ZC LUT ROMs (2 × 200 × 16-bit) | 0 | 1 | ~200 |
| DATA_SC_IDX ROM | 0 | 0 | ~100 |
| **OFDM TX+RX total (float CFO)** | **~105–135** | **~12** | **~9,400** |
| **OFDM TX+RX total (fixed NCO)** | **~85–110** | **~12** | **~6,800** |
| LiteX baseline (your stated 10–12%) | ~12–15 | ~6 | ~3,900 |
| **Grand total (fixed NCO)** | **~100–125** | **~18** | **~10,700** |

At 120 DSPs total on 50T, the **float CFO path puts you over budget**. Switching to a fixed-point NCO brings the DSP count to 85–110 — feasible but tight. Replacing the float path is not optional for this target device. Get the synthesis report early.

---

## Integration into LiteX + AD9361-HDL

### Clock domain

LiteX DMA FIFOs typically run at system clock (100–125 MHz). AD9361-HDL runs at its own data clock (derived from AD9361 DATA_CLK, configurable 1.25 MSPS to 61.44 MSPS). You need proper CDC FIFOs at both the TX input and RX output of your OFDM block. LiteX's AD9361 HDL usually includes these; verify the FIFO depth covers at least one full OFDM symbol of latency.

### AXI-Stream TREADY / backpressure

The `iq_t` struct has only `last` (TLAST). HLS generates TVALID and TREADY automatically for `#pragma HLS INTERFACE axis` ports. However, the TX pipeline's `write()` calls assume the downstream never stalls. If the AD9361 FIFO asserts TREADY=0 (TX FIFO full), HLS will block the `write()` call correctly — but this back-propagates stalls up through the DATAFLOW stages. Validate with a backpressure testbench before hardware integration.

### AD9361 sample width

AD9361 native samples are 12-bit signed. LiteX's AD9361 HDL typically sign-extends to 16-bit (Q1.11 or Q1.15 depending on the driver version). Your `ap_fixed<16,1>` is Q0.15, which is a 16-bit value with a different fractional interpretation. Verify that the scaling matches — a mismatch of 1 bit causes a 6 dB power error. Check the `ad9361_phy` port definitions in your LiteX configuration.

### RX path buffering

`sync_detect` buffers `SYNC_BUF_SZ = 864` samples before producing any output (43.2 µs at 20 MSPS). The RX FIFO between AD9361-HDL and sync_detect must hold at least 864 + (n_syms_max + 1) × 288 samples without overflow. For n_syms=255: 864 + 256 × 288 = 74,592 samples ≈ 1.43 MB. Confirm your LiteX DMA FIFO is sized appropriately.

### TX/RX switching

The AD9361 needs `tx_enable`/`rx_enable` driven from the OFDM block based on whether a packet is being transmitted or received. Currently there is no such output signal. Minimum addition: a CSR register bit that the CPU sets before TX and clears after, with the OFDM block asserting `tx_busy` while transmitting.

### Board bring-up bypass path

There is no way to bypass the OFDM block for raw IQ pass-through. For initial hardware validation you need to verify the LiteX→FIFO→AD9361 chain before adding the OFDM layer. Add a `bypass` CSR register that routes the TX FIFO directly to AD9361-HDL and AD9361-HDL directly to the RX FIFO, completely bypassing the OFDM block.

---

## Priority Fix List

In order of impact for getting a working hardware link:

1. **Replace energy onset sync with normalized Schmidl-Cox or ZC cross-correlator** — the system will not sync in hardware without this
2. **Implement pilot phase tracking in `ofdm_rx`** — read the 6 pilots each symbol, correct CPE before demap
3. **Replace float CFO correction with fixed-point NCO** — Artix-50T DSP budget requires this
4. **Add bit interleaver/deinterleaver** — needed for any non-AWGN channel
5. **Add data scrambler/descrambler** — spectral compliance and FEC performance
6. **Add frame header symbol** — coded n_syms + mod at BPSK rate 1/2 for autonomous RX
7. **Fix `ofdm_rx.h` scale_sch comment** — minor but will confuse anyone reading the header
8. **Add `#pragma HLS PIPELINE II=1` to `equalize_demap_pack` loop** — verify synthesis achieves it
9. **Add CRC** — detect and discard corrupted packets silently passed by Viterbi miscorrection
10. **Add TX bypass CSR** — needed for board bring-up

---

## Throughput Reference

At 20 MSPS with FFT_SIZE=256, CP=32:

| MCS | Coded bits/sym | FEC rate | Payload bits/sym | Symbols/sec | Throughput |
|---|---|---|---|---|---|
| QPSK r1/2 | 400 | 1/2 | 200 | 40,000 | 8 Mbps |
| QPSK r2/3 | 400 | 2/3 | 267 | 40,000 | 10.7 Mbps |
| 16-QAM r1/2 | 800 | 1/2 | 400 | 40,000 | 16 Mbps |
| 16-QAM r2/3 | 800 | 2/3 | 533 | 40,000 | 21.3 Mbps |

40k symbols/sec assumes continuous transmission (no inter-frame gap, no sync overhead). With a preamble per frame and n_syms=255, effective symbol rate is 255/256 × max_rate — preamble overhead is 0.4%, negligible.
