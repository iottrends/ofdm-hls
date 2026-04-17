// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
// control
// 0x00 : reserved
// 0x04 : reserved
// 0x08 : reserved
// 0x0c : reserved
// 0x10 : Data signal of phy_csr
//        bit 31~0 - phy_csr[31:0] (Read/Write)
// 0x14 : Data signal of phy_csr
//        bit 31~0 - phy_csr[63:32] (Read/Write)
// 0x18 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XOFDM_MAC_CONTROL_ADDR_PHY_CSR_DATA 0x10
#define XOFDM_MAC_CONTROL_BITS_PHY_CSR_DATA 64

// ctrl
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read/COR)
//        bit 7  - auto_restart (Read/Write)
//        bit 9  - interrupt (Read)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0 - enable ap_done interrupt (Read/Write)
//        bit 1 - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0 - ap_done (Read/TOW)
//        bit 1 - ap_ready (Read/TOW)
//        others - reserved
// 0x10 : Data signal of my_mac_addr
//        bit 31~0 - my_mac_addr[31:0] (Read/Write)
// 0x14 : Data signal of my_mac_addr
//        bit 15~0 - my_mac_addr[47:32] (Read/Write)
//        others   - reserved
// 0x18 : reserved
// 0x1c : Data signal of promisc
//        bit 0  - promisc[0] (Read/Write)
//        others - reserved
// 0x20 : reserved
// 0x24 : Data signal of modcod
//        bit 1~0 - modcod[1:0] (Read/Write)
//        others  - reserved
// 0x28 : reserved
// 0x2c : Data signal of mac_enable
//        bit 0  - mac_enable[0] (Read/Write)
//        others - reserved
// 0x30 : reserved
// 0x34 : Data signal of tx_pkt_count_i
//        bit 31~0 - tx_pkt_count_i[31:0] (Read/Write)
// 0x38 : reserved
// 0x3c : Data signal of tx_pkt_count_o
//        bit 31~0 - tx_pkt_count_o[31:0] (Read)
// 0x40 : Control signal of tx_pkt_count_o
//        bit 0  - tx_pkt_count_o_ap_vld (Read/COR)
//        others - reserved
// 0x44 : Data signal of rx_pkt_count_i
//        bit 31~0 - rx_pkt_count_i[31:0] (Read/Write)
// 0x48 : reserved
// 0x4c : Data signal of rx_pkt_count_o
//        bit 31~0 - rx_pkt_count_o[31:0] (Read)
// 0x50 : Control signal of rx_pkt_count_o
//        bit 0  - rx_pkt_count_o_ap_vld (Read/COR)
//        others - reserved
// 0x54 : Data signal of rx_drop_count_i
//        bit 31~0 - rx_drop_count_i[31:0] (Read/Write)
// 0x58 : reserved
// 0x5c : Data signal of rx_drop_count_o
//        bit 31~0 - rx_drop_count_o[31:0] (Read)
// 0x60 : Control signal of rx_drop_count_o
//        bit 0  - rx_drop_count_o_ap_vld (Read/COR)
//        others - reserved
// 0x64 : Data signal of rx_fcs_err_count_i
//        bit 31~0 - rx_fcs_err_count_i[31:0] (Read/Write)
// 0x68 : reserved
// 0x6c : Data signal of rx_fcs_err_count_o
//        bit 31~0 - rx_fcs_err_count_o[31:0] (Read)
// 0x70 : Control signal of rx_fcs_err_count_o
//        bit 0  - rx_fcs_err_count_o_ap_vld (Read/COR)
//        others - reserved
// 0x74 : Data signal of last_rx_modcod
//        bit 1~0 - last_rx_modcod[1:0] (Read)
//        others  - reserved
// 0x78 : Control signal of last_rx_modcod
//        bit 0  - last_rx_modcod_ap_vld (Read/COR)
//        others - reserved
// 0x84 : Data signal of last_rx_n_syms
//        bit 7~0 - last_rx_n_syms[7:0] (Read)
//        others  - reserved
// 0x88 : Control signal of last_rx_n_syms
//        bit 0  - last_rx_n_syms_ap_vld (Read/COR)
//        others - reserved
// 0x94 : Data signal of rx_hdr_err_count_i
//        bit 31~0 - rx_hdr_err_count_i[31:0] (Read/Write)
// 0x98 : reserved
// 0x9c : Data signal of rx_hdr_err_count_o
//        bit 31~0 - rx_hdr_err_count_o[31:0] (Read)
// 0xa0 : Control signal of rx_hdr_err_count_o
//        bit 0  - rx_hdr_err_count_o_ap_vld (Read/COR)
//        others - reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XOFDM_MAC_CTRL_ADDR_AP_CTRL                 0x00
#define XOFDM_MAC_CTRL_ADDR_GIE                     0x04
#define XOFDM_MAC_CTRL_ADDR_IER                     0x08
#define XOFDM_MAC_CTRL_ADDR_ISR                     0x0c
#define XOFDM_MAC_CTRL_ADDR_MY_MAC_ADDR_DATA        0x10
#define XOFDM_MAC_CTRL_BITS_MY_MAC_ADDR_DATA        48
#define XOFDM_MAC_CTRL_ADDR_PROMISC_DATA            0x1c
#define XOFDM_MAC_CTRL_BITS_PROMISC_DATA            1
#define XOFDM_MAC_CTRL_ADDR_MODCOD_DATA             0x24
#define XOFDM_MAC_CTRL_BITS_MODCOD_DATA             2
#define XOFDM_MAC_CTRL_ADDR_MAC_ENABLE_DATA         0x2c
#define XOFDM_MAC_CTRL_BITS_MAC_ENABLE_DATA         1
#define XOFDM_MAC_CTRL_ADDR_TX_PKT_COUNT_I_DATA     0x34
#define XOFDM_MAC_CTRL_BITS_TX_PKT_COUNT_I_DATA     32
#define XOFDM_MAC_CTRL_ADDR_TX_PKT_COUNT_O_DATA     0x3c
#define XOFDM_MAC_CTRL_BITS_TX_PKT_COUNT_O_DATA     32
#define XOFDM_MAC_CTRL_ADDR_TX_PKT_COUNT_O_CTRL     0x40
#define XOFDM_MAC_CTRL_ADDR_RX_PKT_COUNT_I_DATA     0x44
#define XOFDM_MAC_CTRL_BITS_RX_PKT_COUNT_I_DATA     32
#define XOFDM_MAC_CTRL_ADDR_RX_PKT_COUNT_O_DATA     0x4c
#define XOFDM_MAC_CTRL_BITS_RX_PKT_COUNT_O_DATA     32
#define XOFDM_MAC_CTRL_ADDR_RX_PKT_COUNT_O_CTRL     0x50
#define XOFDM_MAC_CTRL_ADDR_RX_DROP_COUNT_I_DATA    0x54
#define XOFDM_MAC_CTRL_BITS_RX_DROP_COUNT_I_DATA    32
#define XOFDM_MAC_CTRL_ADDR_RX_DROP_COUNT_O_DATA    0x5c
#define XOFDM_MAC_CTRL_BITS_RX_DROP_COUNT_O_DATA    32
#define XOFDM_MAC_CTRL_ADDR_RX_DROP_COUNT_O_CTRL    0x60
#define XOFDM_MAC_CTRL_ADDR_RX_FCS_ERR_COUNT_I_DATA 0x64
#define XOFDM_MAC_CTRL_BITS_RX_FCS_ERR_COUNT_I_DATA 32
#define XOFDM_MAC_CTRL_ADDR_RX_FCS_ERR_COUNT_O_DATA 0x6c
#define XOFDM_MAC_CTRL_BITS_RX_FCS_ERR_COUNT_O_DATA 32
#define XOFDM_MAC_CTRL_ADDR_RX_FCS_ERR_COUNT_O_CTRL 0x70
#define XOFDM_MAC_CTRL_ADDR_LAST_RX_MODCOD_DATA     0x74
#define XOFDM_MAC_CTRL_BITS_LAST_RX_MODCOD_DATA     2
#define XOFDM_MAC_CTRL_ADDR_LAST_RX_MODCOD_CTRL     0x78
#define XOFDM_MAC_CTRL_ADDR_LAST_RX_N_SYMS_DATA     0x84
#define XOFDM_MAC_CTRL_BITS_LAST_RX_N_SYMS_DATA     8
#define XOFDM_MAC_CTRL_ADDR_LAST_RX_N_SYMS_CTRL     0x88
#define XOFDM_MAC_CTRL_ADDR_RX_HDR_ERR_COUNT_I_DATA 0x94
#define XOFDM_MAC_CTRL_BITS_RX_HDR_ERR_COUNT_I_DATA 32
#define XOFDM_MAC_CTRL_ADDR_RX_HDR_ERR_COUNT_O_DATA 0x9c
#define XOFDM_MAC_CTRL_BITS_RX_HDR_ERR_COUNT_O_DATA 32
#define XOFDM_MAC_CTRL_ADDR_RX_HDR_ERR_COUNT_O_CTRL 0xa0

