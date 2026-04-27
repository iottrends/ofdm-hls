# Compact OFDM SDR — Product Brief

**OEM / System Integrator Reference**

---

## 1. Product Overview

A **complete OFDM software-defined radio modem** implemented on a single low-cost Artix-7 FPGA, delivered as a drop-in module with host PCIe interface and an industry-standard RF transceiver. The design targets embedded wireless links requiring configurable modulation, robust multipath performance, and small form factor — UAV datalinks, industrial telemetry, private wireless bridges, and custom waveform platforms.

**Form factor:** M.2 Key-M card · Single-board solution · PCIe Gen2 x2 host bus
**Core silicon:** Xilinx Artix-7 (XC7A50T) · Analog Devices AD9364 1R1T zero-IF transceiver
**Status:** Fully integrated, timing-closed, bitstream generated and validated

---

## 2. Key Features

| Capability                    | Value                                                       |
|-------------------------------|-------------------------------------------------------------|
| **Waveform**                  | OFDM (256-pt FFT, 200 data + 6 pilot + 50 guard SC)        |
| **Sample rate**               | 20 MSps (I/Q), 14.4 µs per OFDM symbol                      |
| **Modulations**               | QPSK, 16-QAM (Gray-coded, per-frame selectable)             |
| **FEC**                       | Convolutional K=7 rate-1/2 or rate-2/3 with Viterbi decoder |
| **Peak PHY throughput**       | **30.9 Mbps** (16-QAM, r=2/3)                               |
| **RF tuning range**           | 70 MHz – 6 GHz (AD9364 limits)                              |
| **RF bandwidth**              | Up to 56 MHz occupied (20 MSps instantaneous shown)         |
| **Channel estimation**        | Pilot-aided LS estimate, precomputed equalizer reuse        |
| **CFO tolerance**             | ±300 kHz corrected via preamble autocorrelation + NCO       |
| **Minimum decode latency**    | 52 µs (preamble + header to first data demand)              |
| **Per-symbol compute slack**  | 500 cycles (5 µs) — line-rate guaranteed                    |
| **Host interface**            | PCIe Gen2 x2, 2-channel DMA, MSI interrupts                 |
| **Control interface**         | AXI-Lite CSR (64 KB PHY space + RF transceiver SPI)         |
| **MAC layer**                 | On-chip L2 framing (header + FCS), autonomous PHY sequencing|

---

## 3. System Block Diagram

```
 ┌───────────────────────────────────────────────────────────────────────┐
 │                       HOST PC / EMBEDDED SoC                          │
 │                              ▲                                        │
 └──────────────────────────────│────────────────────────────────────────┘
                                │ PCIe Gen2 x2 (2 lanes, 1 GB/s peak)
 ┌──────────────────────────────┼────────────────────────────────────────┐
 │                  FPGA MODULE (Artix-7 XC7A50T)                        │
 │                              │                                        │
 │  ┌──────────────┐   ┌────────┴─────────┐   ┌──────────────────────┐   │
 │  │  LitePCIe    │◄──┤  LitePCIe DMA    ├──►│  ICAP / XADC / DNA   │   │
 │  │  Endpoint    │   │  64-bit, full-dup│   │  (maintenance)       │   │
 │  └──────────────┘   └──────┬───────────┘   └──────────────────────┘   │
 │                            │                                          │
 │  ┌─────────────────────────┴────────────────────────────────────┐     │
 │  │                    OFDM PHY CHAIN (100 MHz)                  │     │
 │  │                                                              │     │
 │  │   TX: host_data → mac → tx_chain → ofdm_tx → IFFT → RF_TX    │     │
 │  │                                                              │     │
 │  │   RX: RF_RX → sync_detect → ofdm_rx → FFT ─┐                 │     │
 │  │                                            ▼                 │     │
 │  │                                        fec_rx (Viterbi,      │     │
 │  │                                        200 MHz) → mac → host │     │
 │  │                                                              │     │
 │  │   Control: AXI-Lite CSR xbar (2 masters × 5 slaves)          │     │
 │  └──────────────────────────────────────────────────────────────┘     │
 │                            │                                          │
 │  ┌─────────────────────────┴────────────────────────────────────┐     │
 │  │   AD9364 PHY adapter:  LVDS IQ  +  SPI control               │     │
 │  └─────────────────────────┬────────────────────────────────────┘     │
 └────────────────────────────┼──────────────────────────────────────────┘
                              │ LVDS IQ @ 245.76 MHz · SPI
 ┌────────────────────────────┴──────────────────────────────────────────┐
 │             AD9364 Wideband Zero-IF Transceiver                       │
 │             70 MHz – 6 GHz · 200 kHz – 56 MHz BW · 1R1T               │
 └───────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                         RF Antenna
```

---

## 4. Performance at a Glance

### Air-Interface Throughput

20 MSPS sample rate, 255-symbol frame (preamble + header + 255 data = 257 symbols, 3.7 ms):

| Modcod           | PHY (gross) | Net (after MAC + framing, −10 to −15%) |
|------------------|-------------|----------------------------------------|
| QPSK,   r = 1/2  | 13.8 Mbps   | 11.7–12.4 Mbps                         |
| QPSK,   r = 2/3  | 18.4 Mbps   | 15.6–16.5 Mbps                         |
| 16-QAM, r = 1/2  | 27.6 Mbps   | 23.4–24.8 Mbps                         |
| **16-QAM, r = 2/3** | **36.7 Mbps** | **31–33 Mbps**                     |

### Latency Budget

| Stage                            | Time     |
|----------------------------------|----------|
| Preamble + header arrival (air)  | 28.8 µs  |
| RX compute to "ready for payload"| 23.0 µs  |
| **Trigger → first data demand**  | **51.8 µs** |
| Per-data-symbol processing       | 5.0 µs compute / 14.4 µs air |
| Max frame duration (255 syms)    | 3.7 ms   |
| Half-duplex round-trip (worst)   | ≥ 7.4 ms |

### FPGA Resource Utilization (XC7A50T-2CSG325, full SoC)

Full LiteX SoC build (PCIe + AD9361 wrapper + OFDM PHY chain), routed and bitstream-generated.

| Resource          | Used   | Total  | %         |
|-------------------|--------|--------|-----------|
| Slice LUTs        | 25,913 | 32,600 | **79.5%** |
| Slice Registers   | 32,450 | 65,200 | 49.8%     |
| Block RAM Tile    | 67     | 75     | **89.3%** |
| DSP48E1           | 81     | 120    | **67.5%** |
| MMCM / PLL        | 1 / 1  | 5 / 5  | 20%       |
| PCIE_2_1 hard IP  | 1      | 1      | 100%      |

**Build result:** DRC 0 errors · Bitstream + SPI-flash image generated · Hardware-validated.
BRAM is the tightest resource (89.3%). Source: `litex/build/.../utilization_place.rpt`.

---

## 5. Interfaces

### 5.1 Host Interface (PCIe)

- **PCIe Gen2 x2** — integrated Xilinx 7-series hard block
- **64-bit DMA engines** — one reader, one writer, independent scatter-gather
- **4 MSI interrupts**:
  - `PCIE_DMA0_WRITER` — TX DMA completion
  - `PCIE_DMA0_READER` — RX DMA completion
  - `OFDM_MAC_TX_DONE` — TX frame transmitted
  - `OFDM_MAC_RX_PKT` — RX frame received
- **128 KB BAR0** — CSR register window (32 KB PHY + 96 KB LiteX backbone)
- **Linux driver** — auto-generated LitePCIe kernel module + userspace library

### 5.2 RF Interface (AD9364)

- **LVDS** data bus — 12-bit I/Q, separate TX/RX, 245.76 MHz DDR
- **SPI** — transceiver configuration (frequency, gain, filters, AGC)
- **TXnRX** — FDD mode (simultaneous TX and RX) — but PHY is half-duplex by default
- **Timing** — on-board IDELAYCTRL calibration, configurable delay taps

### 5.3 Control Plane (AXI-Lite CSR)

| Block       | Offset    | Registers exposed                                     |
|-------------|-----------|-------------------------------------------------------|
| tx_chain    | 0x0000    | Modcod select, payload length, enable                 |
| ofdm_tx     | 0x1000    | Pilot config, guard config                            |
| sync_detect | 0x2000    | Power threshold, preamble count, header-err count     |
| ofdm_rx     | 0x3000    | Header status, decoded modcod, symbol count           |
| ofdm_mac    | 0x4000    | MAC address filter, FCS enable, statistics            |

All PHY blocks are **MAC-autonomous**: the MAC writes modcod/length per frame via its own AXI master, so the host is not in the inner loop.

---

## 6. Hardware Platform

| Component           | Part                       | Role                                    |
|---------------------|----------------------------|-----------------------------------------|
| FPGA                | Xilinx Artix-7 XC7A50T-2   | OFDM PHY + MAC + PCIe                   |
| RF transceiver      | Analog Devices AD9364      | Zero-IF, 70 MHz – 6 GHz, 1R1T           |
| TCXO                | 40 MHz, ±1 ppm             | PLL reference                           |
| External SRAM       | HyperRAM 8 MB              | Scratch / frame buffering               |
| Config flash        | SPI-QSPI 16 MB             | Bitstream storage                       |
| Form factor         | M.2 Key-M 2280             | Standard M.2 slot on host               |

**Typical power envelope:** 3–4 W (FPGA + RF + regulation), with thermal pad contact to M.2 host chassis.

---

## 7. Software Stack

```
┌───────────────────────────────────────────────────────────┐
│   User Application (C / Python / GNU Radio OOT block)     │
├───────────────────────────────────────────────────────────┤
│   LitePCIe userspace library (liblitepcie)                │
│     • open/read/write DMA endpoints                       │
│     • CSR read/write                                      │
│     • MSI event wait                                      │
├───────────────────────────────────────────────────────────┤
│   LitePCIe kernel driver (litepcie.ko)                    │
│     • DMA buffer management                               │
│     • MSI dispatch                                        │
├───────────────────────────────────────────────────────────┤
│   PCIe hardware (Gen2 x2, MSI)                            │
└───────────────────────────────────────────────────────────┘
```

**Driver target:** Linux kernel 5.15+ (tested), FreeBSD (untested, portable).
**API surface:** ~15 calls for open/tune/TX/RX/close. Example C program under 100 lines demonstrates a full TX↔RX loopback.

---

## 8. Target Applications

- **UAV command-and-control + HD video telemetry** — bidirectional half-duplex datalink. Worst-case frame is 3.7 ms one-way; round-trip ≥ 7.4 ms.
- **Industrial wireless backhaul** — replaces licensed microwave at sub-100 m ranges
- **Private wireless mesh** — unlicensed ISM or custom regulatory bands
- **Radio-astronomy / SIGINT front-ends** — 20 MSps I/Q capture with FPGA preprocessing
- **Custom waveform R&D** — HLS C++ source included for PHY modification

---

## 9. Integration & Delivery

### What is delivered

- **FPGA bitstream (.bit)** + **SPI-flash image (.bin)** — pre-built for XC7A50T
- **Host Linux driver** — source + pre-built .ko for common kernels
- **Userspace library + example apps** — C/Python samples for TX, RX, tune, loopback
- **HLS C++ source for all PHY blocks** — optional, under commercial license, for waveform customization
- **Vivado BD TCL + LiteX Python shell** — full regeneration flow for OEM customization
- **Documentation package**:
  - Engineering specification (full PHY/MAC detail)
  - Register map (auto-generated from RTL)
  - Board integration guide
  - Validation report

### Typical integration effort

| Task                                       | Effort   |
|--------------------------------------------|----------|
| M.2 slot bring-up on new host              | 1 day    |
| Kernel driver install + basic loopback     | ½ day    |
| Modcod / rate tuning for target link       | 1 week   |
| RF front-end adapter (SMA / U.FL / filter) | 1–2 weeks|
| Productization (EMI, enclosure, thermal)   | 4–6 weeks|

---

## 10. Roadmap Options

The following extensions are architecturally feasible and can be scoped per customer:

- **Dual-chain 2×2 MIMO** — AD9364 TX2/RX2 already routed; requires PHY duplication (fits on XC7A100T)
- **Adaptive modcod** — measure RSSI/EVM, auto-select modcod per frame
- **Higher-order modulation** — 64-QAM / 256-QAM requires ~10 % more DSPs; migration to XC7A75T
- **Encryption** — AES-128/256 inline at the MAC for payload confidentiality
- **Mesh / multi-hop routing** — upper-MAC extensions on the host side
- **Time-synchronized operation** — IEEE 1588 PTP hook via PCIe or external 1 PPS

---

## 11. Contact & Licensing

**Hardware IP:** Commercial license per unit / volume tiers
**Software IP (driver, library):** BSD-style open licence
**HLS source (PHY customization):** NDA + per-project license
**Support tier:** Direct engineering support, typical 24-hour response

*Contact the engineering team for bitstream evaluation, custom waveform development, or volume licensing.*

---

*Document version 1.0 — 2026-04-18. Based on built and hardware-validated design running on Artix-7 XC7A50T.*
