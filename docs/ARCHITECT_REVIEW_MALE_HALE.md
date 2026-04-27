# Senior Architect Review — `ofdm-hls` for MALE/HALE Tactical Datalink

**Reviewer framing:** L3Harris-class tactical-datalink architect.
**Stated target:** MALE/HALE UAV C2 + telemetry/video link, 10–45 Mbps.
**Date:** 2026-04-26.
**Repo state at review:** branch `rx-dsp-opt`, post-impl numbers in README dated through commit `d22832b`.

---

## TL;DR

`ofdm-hls` is a **competent, well-engineered hobbyist-class OFDM SDR**. The
HLS work is real (AP_SAT discipline, externalized xfft, sliding-window
sync_detect, free-running `ap_ctrl_none` RX triad, post-impl timing closed at
47 % LUT on a XC7A50T). That is not nothing.

But as a candidate **MALE/HALE tactical datalink**, the gap is not
incremental — it is structural. The platform, FEC, security, antenna, MAC, and
link-budget assumptions are 1–2 generations and one PCB tier away from
anything an L3Harris CDL/TCDL/Bandit-family product would ship. The project
is more accurately positioned as "tactical-style OFDM PHY for an SDR R&D
platform" than as a UAV datalink contender.

The review is split into:
1. Internal inconsistencies in the repo's own docs (fix first).
2. Architectural mismatches with MALE/HALE.
3. What's actually good.
4. Three honest options.
5. Near-term recommendations regardless of option.

---

## 1. Internal inconsistencies — fix before any external pitch

These are credibility-killers if a customer or evaluator opens two of your
docs at the same time.

| Claim | Where | Reality |
|---|---|---|
| **Peak throughput 30.9 Mbps** | `docs/PRODUCT_BRIEF.md` §4 | Computed for a 10-symbol frame (5 787 fps × 5 333 b). |
| **Peak net 4.17 Mbps** | `docs/ENGINEERING_SPEC_UAV_DATALINK.md` §11 | Computed for a 255-symbol frame at 3.67 ms. |
| **DSP48 67.5 %** vs **91.7 %** vs **78 %** | README §Resource, PRODUCT_BRIEF §4, ENGINEERING_SPEC §10 | Three different snapshots, no marker for which is current. |
| **"sub-millisecond PHY round-trip"** | PRODUCT_BRIEF §8 | Worst-case frame is 3.67 ms one-way; half-duplex; round-trip ≥ 7+ ms. False. |
| **"recovers 6 dB margin"** in latest commit, but `ber_sweep_summary.csv` shows **QPSK-1/2 @ 20 dB → BER 0.365** | `ber_sweep_summary.csv` line 2 | Either the sweep harness is broken or one of the BER tables is wrong. Pick one source of truth and reconcile. |
| **`sync_cfo` retired** per the spec | ENGINEERING_SPEC §5 still references `sync_cfo` table row, §9 still describes the old DATAFLOW wrap | Tombstone left half-cleaned. |

**Action:** before talking to anyone external, do one consolidation pass. One
throughput number per modcod, one resource-usage table dated and tied to a
commit, one frame-time budget. Two-document divergence in spec language is an
evaluator allergy.

---

## 2. Architectural gaps vs MALE/HALE tactical datalinks

MALE = Predator/Reaper/Heron-class, HALE = Global Hawk/Triton-class.
Reference systems are CDL/TCDL/IBDL/Bandit at 1.544 – 274 Mbps, Ku/Ka-band,
with TRANSEC/COMSEC, ACM, and gimbaled directional antennas. The **target**
in `README.md` is plausibly a UAV ISR datalink. The **design** is much closer
to wfb-ng (the README itself says so).

### 2.1 Hardware class

- **Artix-7 50T + AD9364 + 200 mW radio** is SDR-evaluation hardware. It is
  fine for an open-source hobby UAV link or a lab demo. It is not airborne
  MALE/HALE class. Reference points:
  - Real MALE/HALE radios are MIL-STD-810/461/704/1275 qualified, –40 to
    +85 °C, vibration-spec'd, often hermetic. Not M.2.
  - Reference SoC class for a tactical OFDM modem at 10 – 45 Mbps is
    **Zynq UltraScale+ ZU3EG / ZU4EV / ZU7EV** or **RFSoC** — primarily because
    of FEC + crypto + ARM control + secure boot + JTAG zeroize.
  - RF: AD9364 phase noise (–100 dBc/Hz @ 10 kHz typ at 2.4 GHz) is OK for
    SDR demos. At Ku/Ka-band MALE/HALE carrier frequencies you need a
    low-phase-noise local oscillator (TI LMX2594 or better) and likely an
    external up/down-converter — AD9364 is 70 MHz – 6 GHz only.
- **89.3 % BRAM and 67–91 % DSP utilization** means there is no room left on
  this part for: AES-256-GCM, soft-decision LLR Viterbi, LDPC, multi-tap
  equalizer, MIMO combiner, ARQ buffer, or TRANSEC keystream. You cannot land
  any of the missing tactical features on the current chip — that is **not
  an optimization problem, it is a chip-selection problem**.

**Action:** if MALE/HALE is real, the platform reference design has to move
to ZU+/RFSoC. The Artix-7 baseline becomes the "small-form-factor 5 MHz
tactical variant" or stays as the dev/sim platform. Be explicit about which.

### 2.2 FEC — generations behind

- K=7 hard-decision Viterbi at rate 1/2 / 2/3 with 802.11a puncture is 1990s
  technology. Coding gain is ≈ **5–6 dB short** of LDPC + soft decision and
  ≈ 3 dB short of soft-decision Viterbi alone.
- Reference systems (DVB-S2, CCSDS, 5G-NR, FlexLink, modern CDL waveforms)
  all use LDPC or turbo with soft demapping, plus an outer block code
  (BCH/Reed–Solomon) for residual error. No tactical 10–45 Mbps datalink
  shipped after ~2010 uses hard-decision K=7 Viterbi.
- `docs/FLEXLINK_ANALYSIS.md` already calls this out. The "soft-decision LLR
  upgrade" in §14 of the engineering spec is **not** a roadmap nice-to-have —
  it is the **minimum** to be taken seriously, and even then you are still
  ≈ 3 dB worse than LDPC.
- **Header has CRC-16 only, no FEC.** A single bit flip in the header voids
  the whole frame. Real tactical headers are FEC-protected (often a strong
  short block code) precisely because losing a frame for one bit is
  operationally unacceptable.

### 2.3 Security — entirely absent

This is the single biggest "is this a tactical link?" filter. Right now
`ofdm-hls` has:
- No TRANSEC (no FH, no preamble randomization, no spreading-code rotation).
- No COMSEC (no AES, no key management, no IV/HMAC framing).
- No LPI/LPD features (constant-envelope PAPR, fixed preamble root,
  non-frequency-hopped).
- No anti-replay (CRC-32 alone is not authentication).
- No zeroize, secure boot, or red/black separation.

For any DoD-relevant MALE/HALE pitch, the lack of even a stubbed-out crypto
path is disqualifying. For a non-DoD ISR / commercial UAV link, AES-256-GCM
at the MAC layer plus a basic key-derivation scheme is the ante.

### 2.4 Doppler/CFO and channel tracking

- **Preamble-only Schmidl-Cox CFO, ±0.5 SC ≈ ±39 kHz tolerance.** Fine for
  ground 2.4 GHz. At Ku 14 GHz, an aircraft at 200 m/s gives ≈ 9.3 kHz
  Doppler — inside your tolerance, but the residual after preamble
  compensation, plus the ±0.5 ppm TCXO drift, plus ionospheric Doppler shift
  on long links, eats the budget fast. There is no closed-loop tracker.
  CPE-only correction on 6 shared pilots is the bare minimum.
- **Channel estimate is one-shot, preamble only.** No time-domain tracking,
  no MMSE interpolation, no decision-directed update. For a moving aircraft
  the channel coherence time is sub-millisecond at Ku-band; the 3.67 ms max
  frame **is longer than the channel coherence time**. EVM will degrade
  across the frame on any non-trivial channel.
- **No PNRS-style dedicated phase-noise tracking tones.** Your own FlexLink
  analysis flagged this; for high-altitude long-link operation at C/X/Ku
  band, phase noise dominates fading, and 8 dedicated tones is not optional.

### 2.5 MAC / link layer

- **Half-duplex, single in-flight frame, no ARQ, no QoS, no priority
  queues.** Tactical UAV traffic mixes deeply asymmetric flows: low-rate
  high-priority C2 (~64 kbps, hard latency bound), telemetry (~1 Mbps), HD
  video (5 – 25 Mbps, can drop frames), housekeeping. They need at minimum
  two priority classes and selective ARQ on the C2 class.
- **No frame-level rate adaptation loop.** Modcod is per-frame selectable but
  there is no closed-loop ACM controller measuring SNR/EVM/CRC-pass-rate and
  choosing the next modcod. This is the single most cost-effective
  robustness feature you can add and it is missing.
- **No fragmentation / reassembly above 4 KB.** Video burst frames will
  exceed this. Need IP-friendly fragmentation.
- **CRC-32 alone, no replay/auth.** See §2.3.

### 2.6 Antenna / RF / link budget

- **SISO, no diversity, no MIMO.** MALE/HALE air-side typically uses a
  stabilized parabolic or phased-array directional antenna with Az/El
  tracking. Ground-side is often dual-pol with diversity combining. Neither
  the hardware hooks nor the gateware exist.
- **Link-budget claim of "20 dB SNR margin at 200 mW, several-km LOS" is for
  ground 2.4 GHz hobby ranges.** A MALE LOS link runs 100 – 500 km, a HALE
  BLOS hop is satcom-relayed. Free-space path loss at 200 km / 14 GHz is
  ≈ 161 dB. With +30 dBi parabolic dish on each end, +30 dBm TX power, the
  receiver is below 16-QAM threshold without LDPC. The current PHY without
  6 dB+ extra coding gain just won't close the link at MALE distances.
- **No spectral mask / pulse shaping / windowing** — for any tactical
  regulatory environment (NTIA, MIL-STD-188-181/182), the waveform IP must
  own the spectral mask. This is non-negotiable downstream.
- **No frequency hopping, no spread spectrum.** You operate as a fixed
  15.6 MHz carrier in a 20 MHz channel. Tactical means contested spectrum,
  which means at minimum slow-FH on a hop set.

### 2.7 Range / geometry

- No ranging support, no time-of-flight measurement, no 1 PPS sync.
  MALE/HALE swarm or relay configurations need at least a PTP/1PPS hook.
  It's mentioned in the roadmap; it should be in the architecture.

---

## 3. What's genuinely good

This is real engineering. From a senior-architect lens, the work that lands:

- **Discipline around fixed-point overflow** — the AP_SAT vs AP_WRAP analysis
  in `src/ofdm_rx.cpp` (the `geq_t` typedef block) is exactly the kind of
  subtle Q-format reasoning that wins co-sim debugging. Most teams don't get
  this right until QA finds the BER cliff in the lab.
- **CORDIC equalizer with deliberate non-renormalization** (consumers
  compensate via sign-only compare or pre-scaled thresholds) — that is the
  right DSP-budget call, well documented at the call site.
- **External `xfft v9.1` swap saving ~16 K LUT** — the right engineering
  instinct: don't reimplement what is hardened. Same with the sync_detect
  78 % LUT reduction.
- **Free-running `ap_ctrl_none` RX triad with feedback wires** — clean and
  minimal, avoids the LiteX MAC scheduler being on the inner loop.
- **MAC/PHY split** — `ofdm_mac` running its own `m_axi` master to program TX
  CSRs, host out of the inner loop. Right.
- **Honest FlexLink gap analysis** — `docs/FLEXLINK_ANALYSIS.md` is more
  clear-eyed than most. The "Option B: FlexLink-inspired" framing is mature.
- **Post-impl timing closed at 47 % LUT, 89 % BRAM, 67 % DSP on a 50T** —
  closing the chain on the smallest-of-the-7-series-Artix family is a real
  result. It says the team can route. The same effort on a ZU3EG would have
  4 – 8× the headroom.
- **Real BER vs Python-Q15 reference within ±5 %** at the working modcod —
  numerical-equivalence discipline most HLS projects skip.

---

## 4. Realistic options

Three honest paths. Pick one — don't pretend to do all three.

### Path A — Open-source UAV video link (the README's stated goal)
Stop calling this MALE/HALE. Position as **"open-source FPGA OFDM PHY for
hobby / Group-1-2 UAV links, wfb-ng successor, 2.4 / 5.8 GHz, line-of-sight,
1 – 10 km."** Drop the tactical framing. This is honest and it is where the
design actually fits. Land soft-decision Viterbi, ACM, AES-128 at the MAC,
and you have a great open-source product.

### Path B — "Tactical-style" SDR research platform
Position as **"R&D PHY for waveform development, configurable, HLS source
available."** This is the FlexLink-inspired option (≈ 2 weeks of work for
PNRS + rate matching + dual payload + signal-field FEC). Deliverable is a
research baseline, not a product. Honest about not being qualified hardware.

### Path C — Real MALE/HALE tactical datalink
Requires:
1. **Platform jump to Zynq UltraScale+ (ZU3EG min, ZU7EV preferred) +
   external low-phase-noise upconverter** for Ku/Ka.
2. **FEC rewrite to LDPC + soft demapping** (or turbo). 6 – 12 weeks of
   HLS + verification.
3. **AES-256-GCM at MAC, TRANSEC keystream, PRBS preamble randomization.**
4. **ACM closed loop** + **selective ARQ** + at least 2 priority classes.
5. **Decision-directed channel tracking** + dedicated phase-noise pilots +
   per-symbol CPE.
6. **Slow-FH or hop-set support** in `sync_detect`.
7. **Antenna control bus** (gimbal pointing or beam-steering CSRs).
8. **Spectral mask + windowing** in `ofdm_tx`.
9. **MIL-STD env qual, secure boot, zeroize** on the eventual production
   hardware.
10. **Documentation tier:** ICD, register map, test plan, link budget,
    regulatory filing.

That is **6 – 12 person-months minimum**, not weeks. It competes with mature
L3Harris / Collins / Curtiss-Wright product lines that have 20+ years of
waveform IP and TRANSEC clearance behind them. Path C is a small-team,
well-funded program — not a sprint.

---

## 5. Near-term recommendations (regardless of path)

Cheap and high-value — do them anyway:

1. **Reconcile the throughput/resource numbers across all three docs.** One
   commit-tied table of truth.
2. **Investigate the `ber_sweep_summary.csv` 36 % BER at 20 dB QPSK-1/2** —
   almost certainly a sweep-harness bug, but until it is explained it
   undermines the README BER table.
3. **Finish the `sync_cfo` retirement** — `docs/ENGINEERING_SPEC_UAV_DATALINK.md`
   §5 and §9 still have stale references.
4. **Soft-decision Viterbi (LLR demap)** — your roadmap S2. Single biggest
   BER-vs-effort lever you have.
5. **Header FEC** — even rate-1/3 K=7 conv on the 26-bit header is ~50 LUT
   and recovers 3+ dB on header CRC pass-rate.
6. **ACM closed loop in `ofdm_mac`** — measure CRC-pass-rate, pick next
   modcod. Pure software-tier feature, no DSP cost.
7. **Two priority classes in MAC + selective ARQ on the high-priority
   class** — the C2 / video traffic split needs this.
8. **Stub AES-256-GCM at the MAC even if disabled by default** — without it,
   the project is unpitchable to anyone in tactical / regulated.
9. **Document a real link budget** — TX power, antenna gains, FSPL at
   target range, NF, implementation loss, required SNR per modcod, margin.
   Right now there is no link budget anywhere in the docs.

---

## Bottom line

Code quality is good. DSP and HLS hygiene is good. The honesty in
`docs/FLEXLINK_ANALYSIS.md` already shows the team knows most of what is
above.

The work-product is a strong **open-source SDR OFDM PHY**. It is not, today,
a MALE/HALE tactical datalink, and the gap is not closable on Artix-7 50T +
AD9364 hardware. Either rescope the framing (Path A or B) or accept that
Path C is a multi-quarter program with a hardware redesign at its center.

If this came across an L3Harris internal review, the recommendation would
be: **excellent base IP for a research / training program; not a candidate
for a tactical product line without platform replatform + FEC rewrite +
crypto integration**.
