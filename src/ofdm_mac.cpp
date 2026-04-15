// ============================================================
// ofdm_mac.cpp  —  Combined MAC / PHY Sequencer
// See ofdm_mac.h for architecture overview.
// ============================================================
#include "ofdm_mac.h"

// ── CRC-32 (Ethernet, poly 0x04C11DB7) ──────────────────────
// Bitwise implementation, inner loop UNROLLED — HLS folds to
// one combinational XOR tree per byte (~32 LUT, II=1 per byte).
// Matches standard Ethernet FCS: init 0xFFFFFFFF, bit-reflect
// disabled (we reflect manually where needed), final-XOR
// 0xFFFFFFFF on the outgoing FCS word.
static ap_uint<32> crc32_byte(ap_uint<32> crc, ap_uint<8> b) {
#pragma HLS INLINE
    crc ^= ((ap_uint<32>)b) << 24;
    for (int i = 0; i < 8; i++) {
#pragma HLS UNROLL
        ap_uint<1> top = crc[31];
        crc = (crc << 1);
        if (top) crc ^= ap_uint<32>(0x04C11DB7);
    }
    return crc;
}

// ── Modcod → bytes per OFDM symbol (200 data SCs) ───────────
// Index = {rate, mod}:
//   0b00 QPSK 1/2  → 200 × 2 × 1/2 / 8 = 25
//   0b01 QPSK 2/3  → 33  (floor of 33.33)
//   0b10 16QAM 1/2 → 50
//   0b11 16QAM 2/3 → 66  (floor of 66.67)
static const int BYTES_PER_SYM_TBL[4] = {25, 33, 50, 66};

// ── PHY CSR helpers (m_axi) ─────────────────────────────────
static void csr_write(volatile ap_uint<32>* p, ap_uint<16> off, ap_uint<32> v) {
#pragma HLS INLINE
    p[off >> 2] = v;
}
static ap_uint<32> csr_read(volatile ap_uint<32>* p, ap_uint<16> off) {
#pragma HLS INLINE
    return p[off >> 2];
}

// Configure + start TX PHY chain.
// modcod (2-bit) is passed to tx_chain + ofdm_tx as-is; each block decodes
// internally into {mod, rate}.
static void tx_cfg_phy(
    volatile ap_uint<32>* p,
    ap_uint<16> n_data_bytes,
    ap_uint<8>  n_syms,
    ap_uint<2>  modcod
) {
    // tx_chain
    csr_write(p, PHY_BASE_TX_CHAIN + PHY_TX_CHAIN_N_DATA, n_data_bytes);
    csr_write(p, PHY_BASE_TX_CHAIN + PHY_TX_CHAIN_MODCOD, modcod);
    csr_write(p, PHY_BASE_TX_CHAIN + PHY_TX_CHAIN_N_SYMS, n_syms);
    // ofdm_tx
    csr_write(p, PHY_BASE_OFDM_TX + PHY_OFDM_TX_MODCOD,   modcod);
    csr_write(p, PHY_BASE_OFDM_TX + PHY_OFDM_TX_N_SYMS,   n_syms);
    // ap_start pulses
    csr_write(p, PHY_BASE_TX_CHAIN + PHY_AP_CTRL, 1);
    csr_write(p, PHY_BASE_OFDM_TX  + PHY_AP_CTRL, 1);
}

// Wait for ofdm_tx ap_done (blocking m_axi poll).
static void tx_wait_done(volatile ap_uint<32>* p) {
    ap_uint<32> s;
    do {
        s = csr_read(p, PHY_BASE_OFDM_TX + PHY_AP_CTRL);
    } while (!(s & 0x2));   // ap_done = bit 1
}

// NOTE: rx_arm_phy() has been removed.  The RX PHY chain is free-running
// (ap_ctrl_none) and air-driven.  sync_cfo gates on detected preambles;
// ofdm_rx / fec_rx self-advance on stream data.  MAC no longer touches
// any RX PHY CSR — the m_axi master only programs the TX path.

// ── One TX packet: absorb payload, program PHY, stream ──────
static void do_tx(
    ap_uint<8>                 first_byte_seen,
    bool                       first_was_last,
    hls::stream<axis_byte_t>&  host_tx_in,
    hls::stream<ap_uint<8>>&   phy_tx_out,
    volatile ap_uint<32>*      phy_csr,
    ap_uint<48>                my_mac_addr,
    ap_uint<2>                 modcod,
    ap_uint<32>&               tx_pkt_count,
    ap_uint<1>&                tx_done_pulse
) {
    static ap_uint<8> buf[MAC_MAX_PAYLOAD];
#pragma HLS BIND_STORAGE variable=buf type=RAM_1P impl=BRAM

    // Absorb payload.  The first byte was already read by the caller.
    int plen = 0;
    buf[plen++] = first_byte_seen;
    bool saw_last = first_was_last;

    ABSORB: while (!saw_last && plen < MAC_MAX_PAYLOAD) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT max=4095
        axis_byte_t b = host_tx_in.read();
        buf[plen++] = b.data;
        if (b.last) saw_last = true;
    }

    // Compute frame geometry
    ap_uint<16> n_data_bytes = plen + MAC_OVERHEAD;
    int bps = BYTES_PER_SYM_TBL[modcod];
    ap_uint<8>  n_syms = (n_data_bytes + bps - 1) / bps;
    if (n_syms == 0) n_syms = 1;

    // Program PHY and pulse ap_start
    tx_cfg_phy(phy_csr, n_data_bytes, n_syms, modcod);

    // Build + stream header (dst=broadcast for MVP, src=my_mac_addr, len=plen).
    // TODO: per-link dst addressing from a destination CSR or in-band field.
    ap_uint<48> dst = 0xFFFFFFFFFFFFULL;
    ap_uint<48> src = my_mac_addr;
    ap_uint<16> len_be = (ap_uint<16>)plen;

    ap_uint<32> crc = 0xFFFFFFFF;

    HDR_DST: for (int i = 5; i >= 0; i--) {
#pragma HLS PIPELINE II=1
        ap_uint<8> b = (dst >> (i * 8)) & 0xFF;
        phy_tx_out.write(b);
        crc = crc32_byte(crc, b);
    }
    HDR_SRC: for (int i = 5; i >= 0; i--) {
#pragma HLS PIPELINE II=1
        ap_uint<8> b = (src >> (i * 8)) & 0xFF;
        phy_tx_out.write(b);
        crc = crc32_byte(crc, b);
    }
    HDR_LEN: for (int i = 1; i >= 0; i--) {
#pragma HLS PIPELINE II=1
        ap_uint<8> b = (len_be >> (i * 8)) & 0xFF;
        phy_tx_out.write(b);
        crc = crc32_byte(crc, b);
    }

    // Payload
    PAY: for (int i = 0; i < plen; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT max=4096
        ap_uint<8> b = buf[i];
        phy_tx_out.write(b);
        crc = crc32_byte(crc, b);
    }

    // FCS — final XOR with 0xFFFFFFFF, big-endian on the wire
    ap_uint<32> fcs = crc ^ ap_uint<32>(0xFFFFFFFF);
    FCS_TX: for (int i = 3; i >= 0; i--) {
#pragma HLS PIPELINE II=1
        phy_tx_out.write((fcs >> (i * 8)) & 0xFF);
    }

    // Wait for PHY to finish (ofdm_tx is the slowest)
    tx_wait_done(phy_csr);

    tx_pkt_count++;
    tx_done_pulse = 1;
}

// ── One RX packet ───────────────────────────────────────────
static void do_rx(
    ap_uint<8>                 first_byte,
    hls::stream<ap_uint<8>>&   phy_rx_in,
    hls::stream<axis_byte_t>&  host_rx_out,
    volatile ap_uint<32>*      phy_csr,
    ap_uint<48>                my_mac_addr,
    ap_uint<1>                 promisc,
    modcod_t                   rx_modcod_in,
    ap_uint<8>                 rx_n_syms_in,
    ap_uint<32>&               rx_pkt_count,
    ap_uint<32>&               rx_drop_count,
    ap_uint<32>&               rx_fcs_err_count,
    ap_uint<2>&                last_rx_modcod,
    ap_uint<8>&                last_rx_n_syms,
    ap_uint<1>&                rx_pkt_pulse
) {
    // No RX PHY rearm — chain is free-running.  MAC is pure byte datapath
    // from fec_rx onward.

    // Read 14-byte MAC header; first byte was peeked by caller.
    ap_uint<8> hdr[MAC_HDR_LEN];
    hdr[0] = first_byte;
    HDR_RD: for (int i = 1; i < MAC_HDR_LEN; i++) {
#pragma HLS PIPELINE II=1
        hdr[i] = phy_rx_in.read();
    }

    ap_uint<48> dst = 0;
    DST_PK: for (int i = 0; i < 6; i++) {
#pragma HLS UNROLL
        dst = (dst << 8) | hdr[i];
    }
    ap_uint<16> plen = ((ap_uint<16>)hdr[12] << 8) | hdr[13];
    if (plen > MAC_MAX_PAYLOAD) plen = MAC_MAX_PAYLOAD;

    // Address filter
    bool is_bcast = (dst == 0xFFFFFFFFFFFFULL);
    bool is_me    = (dst == my_mac_addr);
    bool accept   = promisc || is_bcast || is_me;

    // Seed CRC with the header bytes
    ap_uint<32> crc = 0xFFFFFFFF;
    CRC_HDR: for (int i = 0; i < MAC_HDR_LEN; i++) {
#pragma HLS PIPELINE II=1
        crc = crc32_byte(crc, hdr[i]);
    }

    // Payload — stream to host if accepted, CRC always
    PAY: for (int i = 0; i < plen; i++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT max=4096
        ap_uint<8> b = phy_rx_in.read();
        crc = crc32_byte(crc, b);
        if (accept) {
            axis_byte_t o;
            o.data = b;
            o.last = (i == (int)plen - 1) ? 1 : 0;
            o.keep = 1;
            o.strb = 1;
            o.user = 0;
            o.id   = 0;
            o.dest = 0;
            host_rx_out.write(o);
        }
    }

    // FCS (4 bytes big-endian)
    ap_uint<32> rx_fcs = 0;
    FCS_RD: for (int i = 0; i < 4; i++) {
#pragma HLS PIPELINE II=1
        rx_fcs = (rx_fcs << 8) | (ap_uint<32>)phy_rx_in.read();
    }
    ap_uint<32> calc_fcs = crc ^ ap_uint<32>(0xFFFFFFFF);
    bool fcs_ok = (calc_fcs == rx_fcs);

    if (!accept) {
        rx_drop_count++;
    } else if (!fcs_ok) {
        rx_fcs_err_count++;
    } else {
        rx_pkt_count++;
        rx_pkt_pulse = 1;
        // Snapshot PHY header info for driver visibility / future MAC FSMs
        // (keepalive, ACK timers, adaptive modcod tracking, etc.).
        last_rx_modcod = rx_modcod_in;
        last_rx_n_syms = rx_n_syms_in;
    }
}

// ── Top ─────────────────────────────────────────────────────
void ofdm_mac(
    hls::stream<axis_byte_t>& host_tx_in,
    hls::stream<axis_byte_t>& host_rx_out,
    hls::stream<ap_uint<8>>&  phy_tx_out,
    hls::stream<ap_uint<8>>&  phy_rx_in,
    volatile ap_uint<32>*     phy_csr,
    modcod_t     rx_modcod_in,
    ap_uint<8>   rx_n_syms_in,
    ap_uint<1>   rx_header_err,
    ap_uint<48>  my_mac_addr,
    ap_uint<1>   promisc,
    ap_uint<2>   modcod,
    ap_uint<1>   mac_enable,
    ap_uint<32>& tx_pkt_count,
    ap_uint<32>& rx_pkt_count,
    ap_uint<32>& rx_drop_count,
    ap_uint<32>& rx_fcs_err_count,
    ap_uint<2>&  last_rx_modcod,
    ap_uint<8>&  last_rx_n_syms,
    ap_uint<32>& rx_hdr_err_count,
    ap_uint<1>&  tx_done_pulse,
    ap_uint<1>&  rx_pkt_pulse
) {
    #pragma HLS INTERFACE axis      port=host_tx_in
    #pragma HLS INTERFACE axis      port=host_rx_out
    #pragma HLS INTERFACE axis      port=phy_tx_out
    #pragma HLS INTERFACE axis      port=phy_rx_in
    #pragma HLS INTERFACE m_axi     port=phy_csr offset=slave bundle=csr_master \
        depth=8192 max_read_burst_length=1 max_write_burst_length=1 \
        num_read_outstanding=1 num_write_outstanding=1
    // PHY→MAC ap_none wires (driven at BD level by ofdm_rx)
    #pragma HLS INTERFACE ap_none   port=rx_modcod_in
    #pragma HLS INTERFACE ap_none   port=rx_n_syms_in
    #pragma HLS INTERFACE ap_none   port=rx_header_err
    #pragma HLS INTERFACE s_axilite port=my_mac_addr       bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=promisc           bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=modcod            bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=mac_enable        bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=tx_pkt_count      bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=rx_pkt_count      bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=rx_drop_count     bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=rx_fcs_err_count  bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=last_rx_modcod    bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=last_rx_n_syms    bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=rx_hdr_err_count  bundle=ctrl
    #pragma HLS INTERFACE ap_vld    register port=tx_done_pulse
    #pragma HLS INTERFACE ap_vld    register port=rx_pkt_pulse
    #pragma HLS INTERFACE s_axilite port=return bundle=ctrl

    // Default pulses to 0; worker functions set them to 1 on completion.
    tx_done_pulse = 0;
    rx_pkt_pulse  = 0;

    if (!mac_enable) return;

    // Sticky header-error counter — increments every cycle rx_header_err is
    // high (ofdm_rx pulses it once per bad header).  Driver polls
    // rx_hdr_err_count for link-quality diagnostics.
    if (rx_header_err) rx_hdr_err_count++;

    // Round-robin, non-blocking: one packet max per ap_start.
    axis_byte_t tx_first;
    if (host_tx_in.read_nb(tx_first)) {
        do_tx(tx_first.data, tx_first.last != 0,
              host_tx_in, phy_tx_out, phy_csr,
              my_mac_addr, modcod, tx_pkt_count, tx_done_pulse);
        return;
    }

    ap_uint<8> rx_first;
    if (phy_rx_in.read_nb(rx_first)) {
        do_rx(rx_first, phy_rx_in, host_rx_out, phy_csr,
              my_mac_addr, promisc,
              rx_modcod_in, rx_n_syms_in,
              rx_pkt_count, rx_drop_count, rx_fcs_err_count,
              last_rx_modcod, last_rx_n_syms,
              rx_pkt_pulse);
        return;
    }

    // Idle — nothing to do this cycle.
}
