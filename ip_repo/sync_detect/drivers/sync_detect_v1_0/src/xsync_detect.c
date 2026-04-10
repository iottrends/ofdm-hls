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

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XSync_detect_Start(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_AP_CTRL) & 0x80;
    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XSync_detect_IsDone(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XSync_detect_IsIdle(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XSync_detect_IsReady(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XSync_detect_EnableAutoRestart(XSync_detect *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_AP_CTRL, 0x80);
}

void XSync_detect_DisableAutoRestart(XSync_detect *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_AP_CTRL, 0);
}

void XSync_detect_Set_n_syms(XSync_detect *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_N_SYMS_DATA, Data);
}

u32 XSync_detect_Get_n_syms(XSync_detect *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_N_SYMS_DATA);
    return Data;
}

void XSync_detect_InterruptGlobalEnable(XSync_detect *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_GIE, 1);
}

void XSync_detect_InterruptGlobalDisable(XSync_detect *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_GIE, 0);
}

void XSync_detect_InterruptEnable(XSync_detect *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_IER);
    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_IER, Register | Mask);
}

void XSync_detect_InterruptDisable(XSync_detect *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_IER);
    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_IER, Register & (~Mask));
}

void XSync_detect_InterruptClear(XSync_detect *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XSync_detect_WriteReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_ISR, Mask);
}

u32 XSync_detect_InterruptGetEnabled(XSync_detect *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_IER);
}

u32 XSync_detect_InterruptGetStatus(XSync_detect *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XSync_detect_ReadReg(InstancePtr->Ctrl_BaseAddress, XSYNC_DETECT_CTRL_ADDR_ISR);
}

