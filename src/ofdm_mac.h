// ============================================================
// ofdm_mac.h  —  Combined MAC / PHY Sequencer
//
// Replaces the LiteX OFDMLowerMAC gateware sequencer AND adds a thin
// datapath MAC (Ethernet-style header + CRC-32 FCS + address filter).
//
// Clock domain : 100 MHz (ofdm sys clock — same as PCIe byte stream
//                 after LiteX AsyncFIFO + width Converter).
//
// Host-facing  : 8-bit AXIS with TLAST, both directions.
// PHY-facing   : 8-bit plain byte streams; wired to tx_chain.data_in
//                 (TX) and to fec_rx.data_out via the 200→100 MHz AXIS
//                 clock converter `fec_cc2` (RX).
// PHY CSR path : HLS m_axi master → ctrl_xbar (AXI smartconnect) →
//                 s_axilite slaves on the 5 PHY HLS IPs.
//
// Execution model
// ───────────────
// Top function is ap_ctrl_hs and does at most ONE packet per ap_start:
//   - Non-blocking probe host_tx_in: if a byte is available, handle TX.
//   - Else non-blocking probe phy_rx_in: if available, handle RX.
//   - Else return idle (no work).
// A tiny LiteX gateware shim ties ap_start = ~ap_done so the block
// self-retriggers continuously.  Host never has to poke ap_start.
//
// Half-duplex: one packet at a time, TX or RX.  Good for MVP and matches
// the ctrl_xbar m_axi bandwidth anyway.  Upgrade path: split into two
// separate HLS IPs (ofdm_mac_tx / ofdm_mac_rx) with separate m_axi.
//
// Frame format (host ↔ MAC ↔ host, over-the-air PHY content)
// ──────────────────────────────────────────────────────────
//   [dst_mac(6)][src_mac(6)][len_be16(2)][payload(N)][CRC32_be(4)]
//
//   - len field carries PAYLOAD bytes (not including header or FCS).
//     That's a deliberate divergence from standard Ethernet's
//     ethertype — we use it for RX packet-boundary detection.
//   - FCS is standard Ethernet CRC-32 (poly 0x04C11DB7, init 0xFFFFFFFF,
//     final-XOR 0xFFFFFFFF), transmitted big-endian.
//
// Modcod convention (matches MAC CSR bits, matches LiteX spec)
//   modcod[1] : mod  — 0=QPSK, 1=16-QAM   (→ PHY block `mod_t`)
//   modcod[0] : rate — 0=rate 1/2, 1=rate 2/3  (→ PHY block `rate_t`)
// ============================================================
#pragma once

#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>
#include "ofdm_tx.h"    // modcod_t

#define MAC_HDR_LEN         14
#define MAC_FCS_LEN          4
#define MAC_OVERHEAD        (MAC_HDR_LEN + MAC_FCS_LEN)
#define MAC_MAX_PAYLOAD     4096
#define MAC_MAX_FRAME       (MAC_MAX_PAYLOAD + MAC_OVERHEAD)

// ── PHY block base addresses in m_axi ctrl_xbar space ───────
// Must match the address_segs set in vivado/create_ofdm_bd.tcl.
// 4 KB per block.  RX blocks are free-running and have no CSRs reachable
// by MAC — only tx_chain and ofdm_tx are programmed via m_axi.
#define PHY_BASE_TX_CHAIN   0x0000
#define PHY_BASE_OFDM_TX    0x1000

// ── Per-block s_axilite register offsets ────────────────────
// These are emitted by Vitis HLS when each block is synthesized with
// s_axilite control. After running each block's csynth once, grep
// the generated component.xml for the actual offsets and update the
// table below if Vitis allocated different slots.
// AP_CTRL is always 0x00 by convention (ap_start/ap_done).
#define PHY_AP_CTRL         0x00

// tx_chain: n_data_bytes, modcod(2b), n_syms
#define PHY_TX_CHAIN_N_DATA 0x10
#define PHY_TX_CHAIN_MODCOD 0x18
#define PHY_TX_CHAIN_N_SYMS 0x20

// ofdm_tx: modcod(2b), n_syms
#define PHY_OFDM_TX_MODCOD  0x10
#define PHY_OFDM_TX_N_SYMS  0x18

// sync_detect / ofdm_rx / fec_rx: no CSRs accessed by MAC.
// (sync_detect and ofdm_rx may have stats-only s_axilite banks for host diagnostics,
//  but MAC never writes to them.)

typedef ap_axiu<8,0,0,0> axis_byte_t;

void ofdm_mac(
    // Host DMA (LitePCIe) side
    hls::stream<axis_byte_t>& host_tx_in,
    hls::stream<axis_byte_t>& host_rx_out,

    // PHY byte streams
    hls::stream<ap_uint<8>>&  phy_tx_out,    // → tx_chain.data_in
    hls::stream<ap_uint<8>>&  phy_rx_in,     // ← fec_rx.data_out (via fec_cc2)

    // AXI4 master → ctrl_xbar → PHY s_axilite slaves (TX CSR programming only)
    volatile ap_uint<32>*     phy_csr,

    // RX PHY header-decode visibility (ap_none wires, driven by ofdm_rx):
    //   rx_modcod_in  : 2-bit modcod decoded from the air header
    //   rx_n_syms_in  : 8-bit symbol count decoded from the air header
    //   rx_header_err : sticky header-CRC-error flag (pulses high per bad header)
    // These are read by the MAC for stats + future FSM work (keepalive/ACK).
    modcod_t     rx_modcod_in,
    ap_uint<8>   rx_n_syms_in,
    ap_uint<1>   rx_header_err,

    // MAC own s_axilite
    ap_uint<48>  my_mac_addr,
    ap_uint<1>   promisc,
    ap_uint<2>   modcod,                  // TX modcod only (host-programmed)
    ap_uint<1>   mac_enable,
    ap_uint<32>& tx_pkt_count,
    ap_uint<32>& rx_pkt_count,
    ap_uint<32>& rx_drop_count,
    ap_uint<32>& rx_fcs_err_count,

    // Driver-visible RX header snapshots (last accepted packet)
    ap_uint<2>&  last_rx_modcod,
    ap_uint<8>&  last_rx_n_syms,
    ap_uint<32>& rx_hdr_err_count,

    // Interrupt strobes (ap_vld outputs — 1-cycle pulses)
    ap_uint<1>&  tx_done_pulse,
    ap_uint<1>&  rx_pkt_pulse
);
