// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xofdm_rx.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XOfdm_rx_CfgInitialize(XOfdm_rx *InstancePtr, XOfdm_rx_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Stat_BaseAddress = ConfigPtr->Stat_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

u32 XOfdm_rx_Get_header_err(XOfdm_rx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_rx_ReadReg(InstancePtr->Stat_BaseAddress, XOFDM_RX_STAT_ADDR_HEADER_ERR_DATA);
    return Data;
}

u32 XOfdm_rx_Get_header_err_vld(XOfdm_rx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_rx_ReadReg(InstancePtr->Stat_BaseAddress, XOFDM_RX_STAT_ADDR_HEADER_ERR_CTRL);
    return Data & 0x1;
}

