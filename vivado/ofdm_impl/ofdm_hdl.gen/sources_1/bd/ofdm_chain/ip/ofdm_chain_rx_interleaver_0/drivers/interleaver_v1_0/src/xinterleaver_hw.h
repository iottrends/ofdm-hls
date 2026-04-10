// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
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
// 0x10 : Data signal of mod_r
//        bit 0  - mod_r[0] (Read/Write)
//        others - reserved
// 0x14 : reserved
// 0x18 : Data signal of n_syms
//        bit 7~0 - n_syms[7:0] (Read/Write)
//        others  - reserved
// 0x1c : reserved
// 0x20 : Data signal of is_rx
//        bit 0  - is_rx[0] (Read/Write)
//        others - reserved
// 0x24 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XINTERLEAVER_CTRL_ADDR_AP_CTRL     0x00
#define XINTERLEAVER_CTRL_ADDR_GIE         0x04
#define XINTERLEAVER_CTRL_ADDR_IER         0x08
#define XINTERLEAVER_CTRL_ADDR_ISR         0x0c
#define XINTERLEAVER_CTRL_ADDR_MOD_R_DATA  0x10
#define XINTERLEAVER_CTRL_BITS_MOD_R_DATA  1
#define XINTERLEAVER_CTRL_ADDR_N_SYMS_DATA 0x18
#define XINTERLEAVER_CTRL_BITS_N_SYMS_DATA 8
#define XINTERLEAVER_CTRL_ADDR_IS_RX_DATA  0x20
#define XINTERLEAVER_CTRL_BITS_IS_RX_DATA  1

