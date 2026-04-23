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
// 0x10 : Data signal of pow_threshold
//        bit 23~0 - pow_threshold[23:0] (Read/Write)
//        others   - reserved
// 0x14 : reserved
// 0x18 : Data signal of stat_preamble_count
//        bit 31~0 - stat_preamble_count[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of stat_header_bad_count
//        bit 31~0 - stat_header_bad_count[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of stat_pow_env
//        bit 23~0 - stat_pow_env[23:0] (Read/Write)
//        others   - reserved
// 0x2c : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XSYNC_DETECT_STAT_ADDR_POW_THRESHOLD_DATA         0x10
#define XSYNC_DETECT_STAT_BITS_POW_THRESHOLD_DATA         24
#define XSYNC_DETECT_STAT_ADDR_STAT_PREAMBLE_COUNT_DATA   0x18
#define XSYNC_DETECT_STAT_BITS_STAT_PREAMBLE_COUNT_DATA   32
#define XSYNC_DETECT_STAT_ADDR_STAT_HEADER_BAD_COUNT_DATA 0x20
#define XSYNC_DETECT_STAT_BITS_STAT_HEADER_BAD_COUNT_DATA 32
#define XSYNC_DETECT_STAT_ADDR_STAT_POW_ENV_DATA          0x28
#define XSYNC_DETECT_STAT_BITS_STAT_POW_ENV_DATA          24

