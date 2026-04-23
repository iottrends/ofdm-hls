// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
// stat
// 0x00 : reserved
// 0x04 : reserved
// 0x08 : reserved
// 0x0c : reserved
// 0x10 : Data signal of header_err
//        bit 0  - header_err[0] (Read)
//        others - reserved
// 0x14 : Control signal of header_err
//        bit 0  - header_err_ap_vld (Read/COR)
//        others - reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XOFDM_RX_STAT_ADDR_HEADER_ERR_DATA 0x10
#define XOFDM_RX_STAT_BITS_HEADER_ERR_DATA 1
#define XOFDM_RX_STAT_ADDR_HEADER_ERR_CTRL 0x14

