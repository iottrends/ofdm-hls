// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xsync_detect.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XSync_detect_CfgInitialize(XSync_detect *InstancePtr, XSync_detect_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Stat_BaseAddress = ConfigPtr->Stat_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XSync_detect_Set_pow_threshold(XSync_detect *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Stat_BaseAddress, XSYNC_DETECT_STAT_ADDR_POW_THRESHOLD_DATA, Data);
}

u32 XSync_detect_Get_pow_threshold(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Stat_BaseAddress, XSYNC_DETECT_STAT_ADDR_POW_THRESHOLD_DATA);
    return Data;
}

void XSync_detect_Set_stat_preamble_count(XSync_detect *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Stat_BaseAddress, XSYNC_DETECT_STAT_ADDR_STAT_PREAMBLE_COUNT_DATA, Data);
}

u32 XSync_detect_Get_stat_preamble_count(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Stat_BaseAddress, XSYNC_DETECT_STAT_ADDR_STAT_PREAMBLE_COUNT_DATA);
    return Data;
}

void XSync_detect_Set_stat_header_bad_count(XSync_detect *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Stat_BaseAddress, XSYNC_DETECT_STAT_ADDR_STAT_HEADER_BAD_COUNT_DATA, Data);
}

u32 XSync_detect_Get_stat_header_bad_count(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Stat_BaseAddress, XSYNC_DETECT_STAT_ADDR_STAT_HEADER_BAD_COUNT_DATA);
    return Data;
}

void XSync_detect_Set_stat_pow_env(XSync_detect *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Stat_BaseAddress, XSYNC_DETECT_STAT_ADDR_STAT_POW_ENV_DATA, Data);
}

u32 XSync_detect_Get_stat_pow_env(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Stat_BaseAddress, XSYNC_DETECT_STAT_ADDR_STAT_POW_ENV_DATA);
    return Data;
}

