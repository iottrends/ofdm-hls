# ==============================================================
# Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
# Tool Version Limit: 2025.11
# Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
# Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
# 
# ==============================================================
proc generate {drv_handle} {
    xdefine_include_file $drv_handle "xparameters.h" "XOfdm_rx" \
        "NUM_INSTANCES" \
        "DEVICE_ID" \
        "C_S_AXI_STAT_BASEADDR" \
        "C_S_AXI_STAT_HIGHADDR"

    xdefine_config_file $drv_handle "xofdm_rx_g.c" "XOfdm_rx" \
        "DEVICE_ID" \
        "C_S_AXI_STAT_BASEADDR"

    xdefine_canonical_xpars $drv_handle "xparameters.h" "XOfdm_rx" \
        "DEVICE_ID" \
        "C_S_AXI_STAT_BASEADDR" \
        "C_S_AXI_STAT_HIGHADDR"
}

