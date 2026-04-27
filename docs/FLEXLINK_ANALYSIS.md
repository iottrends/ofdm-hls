# FlexLink PHY — Spec Analysis & `ofdm-hls` Alignment Study

**Source document:** `FlexLinkPhy202x_v11.docx` — "FlexLink Physical Layer Definitions", version 00.10, Apr 23, 2023. Author: Andreas Schwarzinger. Status: Preliminary.

**Purpose of this document:** assess the FlexLink PHY specification against the current `ofdm-hls` design, identify compliance gaps, and lay out realistic response strategies for the UAV datalink / IDEX use case.

---

## 1. What FlexLink Is

FlexLink is a **preliminary PHY + (formerly) MAC specification** for a packet-based TDD radio link. It deliberately borrows from **802.11 WLAN** (QAM mapping, signal field, ZC preambles) and **4G/5G LTE** (resource grid, demodulation reference symbols, rate matching, LDPC).

Target use case (inferred from the spec): mixed point-to-point and point-to-multipoint links where TX and RX terminals may be in motion, supporting both high reliability and high throughput. This profile matches tactical datalinks, UAV command-and-control + video, and similar mid-tier wireless applications.

The author (Schwarzinger) is known from RF/DSP textbook work; the spec reads like a one-person cleanroom design of "LTE-lite with WLAN ergonomics."

---

## 2. Mandatory PHY Parameters

Numbers below are for the **mandatory 20 MHz / 20 kHz subcarrier-spacing profile**. Optional bandwidth profiles (5 / 10 / 40 MHz) scale FFT size and sample rate proportionally.

| Parameter | Mandatory value | Notes |
|---|---|---|
| Bandwidth | 20 MHz | 5 / 10 / 40 MHz optional |
| Subcarrier spacing | 20 kHz | 40 / 80 / 160 kHz optional |
| FFT size | 1024 | |
| Sample rate (Fs) | 20.48 MHz | TCXO at Fs or integer multiple |
| Cyclic prefix | 4 µs (82 samples) | 1 / 2 / 8 µs optional |
| Active subcarriers | 901 (451 positive + 450 negative) | DC tone suppressed |
| Frequency stability | ≤ 0.5 ppm combined TX+RX | At 6 GHz → ≤ 3 kHz offset |
| QAM constellations | BPSK / QPSK / 16-QAM / 64-QAM | 802.11a mapping + scaling |
| Payload FEC | LDPC ½, ⅔, ¾ | 802.11n codes, block sizes 648 / 1296 / 1944 |
| Signal-field FEC | Convolutional ⅓ (K=7) + Polar 256-bit | Two layers |
| Rate matching | 1× / 1.5× / 2× / 3× / 4× / 6× / 8× / 16× | Bit-repetition, per-payload programmable |
| Antenna config | SISO mandatory; MISO / SIMO / 2×2 MIMO optional | SFBC for 2×2 |
| Preamble | AGC burst + PreambleA + PreambleB | Three-stage |
| AGC burst | 5 µs (102 samples), ZC `u=34`, `Nzc=887` | Wideband, low PAPR |
| PreambleA | 4-tone signal at ±640 kHz, ±1920 kHz | 512 or 5120 samples (offset-detect mode) |
| PreambleB | ZC `u=34`, `Nzc=331`, IFFT N=1024 | Timing sync, autocorrelation-based |
| Demod-reference symbols | Periodic (period programmable, 2–32 symbols) | Pilot spacing = 3 resource elements |
| Phase-noise reference signals | 8 tones (±90, ±180, ±270, ±360) every data symbol | BPSK +1 |
| Payload structure | Two fields (A + B), independent MCS | Differential reliability / throughput |

---

## 3. FlexLink vs `ofdm-hls` — Side-by-Side

| Parameter | FlexLink (mandatory) | `ofdm-hls` today |
|---|---|---|
| Target FPGA | Not specified (implies Zynq-class) | XC7A50T (Artix-7 50T) |
| RFIC | Not specified | AD9364 (single) |
| Sample rate | 20.48 MHz | 20 MHz |
| FFT size | **1024** | **256** |
| Subcarrier spacing | 20 kHz | 78.125 kHz (20 MHz / 256) |
| Cyclic prefix | 82 samples (4 µs) | 32 samples (1.6 µs) |
| Active subcarriers | 901 | 200 data + 6 pilot |
| Modulation set | BPSK / QPSK / 16-QAM / 64-QAM | QPSK / 16-QAM (runtime-selectable) |
| Payload FEC | **LDPC ½, ⅔, ¾** | Conv. K=7, rate ½ or ⅔, **hard-decision Viterbi** |
| Signal-field FEC | Conv. ⅓ (K=7) + Polar 256 | CRC-16 only (no FEC on header) |
| Rate matching | 1× to 16× bit repetition | Fixed (set by FEC rate) |
| Preamble | AGC burst + PreambleA (4-tone) + PreambleB (ZC-331) | Single ZC (root 25) |
| Frequency-offset acquisition | FFT peak-pick on PreambleA 4 tones | Schmidl-Cox |
| Antenna config | SISO / MISO / SIMO / 2×2 MIMO (SFBC) | SISO |
| Per-symbol phase tracking | 8 dedicated phase-noise ref tones | 6 pilots (shared with data SCs) |
| Channel estimate refresh | Periodic demod-ref symbols (2–32 symbol cadence) | Preamble-only |
| Payload structure | Two fields A + B, independent MCS | Single payload |

---

## 4. Technical Assessment

### 4.1 What's clever in FlexLink

- **Two payload fields (A + B) with independent MCS.** Lets the radio mix high-reliability control traffic with lower-reliability bulk data in the same packet. Directly applicable to UAV: Payload A = flight control / telemetry at rate-½ QPSK; Payload B = video at rate-¾ 16-QAM.
- **Dedicated phase-noise reference signals** (8 tones at ±90, ±180, ±270, ±360) transmitted in every data OFDM symbol, separate from the demod-ref pilots. Correct architecture for UAV at high carrier frequencies where phase noise dominates channel variation. Our current `ofdm-hls` solves the same problem with 6 shared pilots — functional but less precise.
- **Periodic demod-ref symbols with programmable cadence** (period ∈ {2, 4, …, 32} symbols). Cleaner parametrization than a one-off mid-frame channel update. This is the same problem `ofdm-hls` item S3 (Priority Fix List in `OFDM_HLS_ANALYSIS.md` §9.5) tries to solve.
- **Rate matching by bit repetition** (1× / 1.5× / 2× / 3× / 4× / 6× / 8× / 16×). Nearly zero HLS cost — just a repeat loop after the FEC encoder — and buys 3–6 dB link margin in poor-SNR conditions.
- **4-tone PreambleA** (tones at ±640 kHz, ±1920 kHz) for frequency-offset acquisition via FFT peak-picking. Claimed −8 dB SNR detection capability. Different approach from Schmidl-Cox but with comparable or better performance.
- **Signal field protected by convolutional FEC (⅓ K=7) + polar 256-bit.** Our current header has only CRC-16 — one bit flip corrupts the whole frame. Adding FEC to the signal field is a robustness upgrade worth considering even outside FlexLink alignment.

### 4.2 What's heavy — FlexLink compliance cost on `ofdm-hls`

- **1024-point FFT at 20.48 MSPS** is ~4× the DSP48E1 budget of our current 256-point FFT at 20 MSPS. Does not fit XC7A50T. Would require XC7A100T minimum, realistically Zynq 7020 / 7030.
- **LDPC encoder/decoder** (802.11n codes, block sizes 648/1296/1944) — each LDPC decoder is typically 10–15K LUT on Artix-7 (vs our 9.4K LUT hard-decision Viterbi). Replacing our FEC with LDPC: **+20–30K LUT net** over current chain. Also does not fit 50T.
- **Polar 256-bit decoder** for signal field — additional codec, non-trivial LUT + BRAM cost.
- **SFBC 2×2 diversity** requires a second AD9364 (or 2Rx path) and significant RX combining logic (per-SC Alamouti decoder).
- **Dedicated AGC burst handling** — needs an analog AGC convergence loop. Currently we assume AD9364's internal AGC handles input scaling.
- **Dual-payload architecture** (A + B with independent MCS) requires re-plumbing the scrambler → FEC → interleaver → OFDM chain to run twice per packet with different parameters.

### 4.3 Cost cliff

FlexLink mandatory profile is designed for **mid-tier SoC** class hardware (Zynq UltraScale+, XC7A200T, or 5G-modem-class ASICs). Full-spec compliance on XC7A50T is not feasible. The relevant cliff:

| FPGA class | 20 MHz / 1024-FFT | 5 MHz / 256-FFT | LDPC? | SFBC 2×2? |
|---|---|---|---|---|
| XC7A35T | No | Tight | No | No |
| **XC7A50T (current)** | **No** | **Yes (current)** | **No** | **No** |
| XC7A100T | Yes | Yes | Single LDPC | No |
| XC7A200T | Yes | Yes | Multi LDPC | Yes |
| Zynq 7020 / 7030 | Yes | Yes | Multi LDPC | Yes |
| Zynq US+ | Yes (easy) | Yes | Multi LDPC | Yes + margin |

---

## 5. Response Strategies

There are three coherent responses to FlexLink as a reference/target spec.

### 5.1 Option A — Adopt a FlexLink subset (5 MHz profile)

FlexLink's optional **5 MHz bandwidth profile** uses 256-FFT at 5.12 MSPS, 20 kHz subcarrier spacing, 20-sample CP. This is dimensionally very close to our current `ofdm-hls` — one sample-rate change away. Compliance scope:

- Move from 20 MSPS / 78 kHz SC spacing → 5.12 MSPS / 20 kHz SC spacing (mostly a TCXO + clocking change).
- Keep FFT at 256 (already done).
- Adopt FlexLink's pilot / reference signal structure.
- Need to justify keeping Viterbi over LDPC (simpler hardware trade-off) — or implement LDPC.
- Fits XC7A50T.

**Pros:** Minimal hardware change, smallest HLS delta.
**Cons:** Limited to 5 MHz BW — lower throughput. FEC substitution still contentious.

### 5.2 Option B — "FlexLink-inspired" (recommended)

Cherry-pick the high-value ideas from FlexLink without claiming compliance. Keep XC7A50T + AD9364 platform. Land the following in priority order:

| Feature | Effort | Impact | Maps to roadmap item |
|---|---|---|---|
| **Phase-noise reference signals** (8 dedicated tones per data symbol, BPSK +1) | 2 days HLS | +1–2 dB phase-noise rejection, cleaner per-symbol CPE | Supersedes S3 partially |
| **Rate matching** (bit repetition, 1× / 2× / 4× / 8× post-FEC) | 1 day HLS | +3–6 dB link margin in poor SNR | New |
| **Two-payload split** (A = control, B = video; independent MCS) | 3–5 days HLS | Better mission-critical / bulk-data separation | New |
| **Signal field FEC** (convolutional ⅓ K=7 on header bits) | 2 days HLS | Header robustness at low SNR | New |
| **Per-symbol pilot update / MMSE interpolation** | 2 days HLS | +2 dB at UAV speed | S3 |
| **Soft-decision LLR Viterbi** | 1 week HLS | +2–3 dB coding gain (still not LDPC, but closes part of the gap) | S2 |

Document the design as **"FlexLink-inspired, adapted for Artix-7 50T"** — honest positioning that acknowledges the reference without overclaiming.

**Pros:** Keeps validated hardware platform, incremental changes, each feature independently testable. Most robustness gains without the FFT / LDPC cliff.
**Cons:** Not a compliance pitch. Cannot claim interoperability with other FlexLink radios.

### 5.3 Option C — Full FlexLink compliance on bigger hardware

Move to **XC7A100T** (minimum) or **Zynq 7020/7030** (preferred for software side). Implement the mandatory 20 MHz / 1024-FFT / LDPC / SFBC configuration. Real engineering effort: 3–6 months full-time.

**Pros:** Interoperable FlexLink radio. Stronger IDEX/defense pitch if interop is a criterion. Full feature set.
**Cons:** Hardware platform change. ~3× the engineering effort. Abandons the 47%-LUT validated 50T baseline. FEC subsystem rewrite.

---

## 6. Recommendation

For the IDEX / UAV video-link pitch, **Option B (FlexLink-inspired)** is the right path:

1. Phase-noise reference signals → lands in ~2 days, closes the S3 gap with a cleaner design than "more pilots."
2. Rate matching → 1 day, highest ROI in link-margin terms.
3. Two-payload A/B split → 3–5 days, directly useful for mixed-criticality UAV traffic (flight control vs video).

Together these are roughly **1–2 weeks of HLS work** and close most of the meaningful performance gap with FlexLink without leaving the XC7A50T platform or rewriting the FEC subsystem.

Document the lineage honestly: "Design borrows the phase-noise reference signal structure, rate matching, and dual-payload concept from FlexLink (Schwarzinger 2023); implementation targets Artix-7 50T + AD9364 with hard-decision Viterbi FEC instead of LDPC, appropriate to the resource envelope."

If an IDEX evaluator later insists on full FlexLink compliance as a gating requirement, the path to Option C is clear — the 50T baseline is validated and the migration delta is scoped. But don't burn engineering time on a 1024-FFT port until that requirement is explicit.

---

## 7. Key FlexLink Concepts Worth Understanding Before Option B

### 7.1 Phase-noise reference signals (PNRS)

FlexLink places 8 BPSK `+1` tones at subcarriers `±90, ±180, ±270, ±360` in every data OFDM symbol. For a single TX antenna, these overwrite data resource elements. The receiver uses them — in addition to the periodic demod-ref symbols — to track phase noise and small frequency drifts *within* a data frame.

For `ofdm-hls` this is a small change: add 8 fixed tones in `ofdm_tx`, extract them on the RX side in `compute_pilot_cpe` (or a replacement function) with better averaging than the current 6-pilot approach.

### 7.2 Rate matching

After the FEC encoder produces a fixed-size encoded block (EBS), optional rate matching repeats every bit `C2 ∈ {1, 2, 4, 8}` times and optionally the even-indexed bits once more (for 1.5× / 3× / 6× factors). This inflates the encoded block before interleaving, giving the Viterbi / LDPC decoder more bit energy per information bit → better BER at low SNR.

Implementation: a trivial HLS loop after `conv_enc` that reads one bit and writes N bits to the output stream. ~50 LUT, no DSP.

### 7.3 Two-payload architecture (A + B)

Every FlexLink packet carries two independent payload fields, each with its own MCS (modulation + FEC rate + rate-matching factor). The signal field encodes both MCS descriptors. A zero `NTB2` indicates that payload B is absent.

For `ofdm-hls` this means running the TX scrambler → FEC → interleaver chain twice per packet (once per payload) with different parameters, and similarly on RX. Structurally: the control path (payload A config + payload B config) loads one set of runtime parameters before the first payload's OFDM symbols, then updates and loads again for the second payload.

### 7.4 Signal-field FEC

FlexLink protects the 90-bit signal field with a rate-⅓ K=7 convolutional encoder followed by rate matching to fill all BPSK/QPSK symbols in the signal-field OFDM symbols (up to 4 symbols, configurable). Polar 256-bit is also used (dual protection).

Our current 26-bit header has CRC-16 only — a single-bit error at the start of a frame causes full-frame rejection. Adding convolutional FEC here is a cheap robustness win even outside FlexLink alignment.

---

## 8. Open Questions

- **Who handed us this spec?** Context matters — customer reference, evaluation target, or competitive intel? Drives whether Option B's "inspired" framing is acceptable or whether Option C's compliance pitch is required.
- **What bandwidth does the UAV link actually need?** 20 MHz (mandatory FlexLink) vs 5 MHz (current `ofdm-hls` scale) is a 4× throughput difference; mission requirements decide.
- **LDPC or Viterbi** — if the requirement is "better FEC than hard-decision Viterbi," soft-decision Viterbi (S2 on the existing roadmap) closes most of the gap at ~10% of the LDPC integration cost.
- **MIMO / diversity** — FlexLink lists 2×2 as optional but many RF-noisy scenarios benefit from at minimum 2×1 MISO. A second AD9364 is a real BOM cost decision, not just an HLS decision.
