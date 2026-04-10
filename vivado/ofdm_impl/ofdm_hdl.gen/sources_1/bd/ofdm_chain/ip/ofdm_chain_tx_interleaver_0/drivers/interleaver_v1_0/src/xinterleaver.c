// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xinterleaver.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XInterleaver_CfgInitialize(XInterleaver *InstancePtr, XInterleaver_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XInterleaver_Start(XInterleaver *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_AP_CTRL) & 0x80;
    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XInterleaver_IsDone(XInterleaver *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XInterleaver_IsIdle(XInterleaver *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XInterleaver_IsReady(XInterleaver *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XInterleaver_EnableAutoRestart(XInterleaver *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_AP_CTRL, 0x80);
}

void XInterleaver_DisableAutoRestart(XInterleaver *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_AP_CTRL, 0);
}

void XInterleaver_Set_mod_r(XInterleaver *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_MOD_R_DATA, Data);
}

u32 XInterleaver_Get_mod_r(XInterleaver *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_MOD_R_DATA);
    return Data;
}

void XInterleaver_Set_n_syms(XInterleaver *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_N_SYMS_DATA, Data);
}

u32 XInterleaver_Get_n_syms(XInterleaver *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_N_SYMS_DATA);
    return Data;
}

void XInterleaver_Set_is_rx(XInterleaver *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_IS_RX_DATA, Data);
}

u32 XInterleaver_Get_is_rx(XInterleaver *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_IS_RX_DATA);
    return Data;
}

void XInterleaver_InterruptGlobalEnable(XInterleaver *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_GIE, 1);
}

void XInterleaver_InterruptGlobalDisable(XInterleaver *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_GIE, 0);
}

void XInterleaver_InterruptEnable(XInterleaver *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_IER);
    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_IER, Register | Mask);
}

void XInterleaver_InterruptDisable(XInterleaver *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_IER);
    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_IER, Register & (~Mask));
}

void XInterleaver_InterruptClear(XInterleaver *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XInterleaver_WriteReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_ISR, Mask);
}

u32 XInterleaver_InterruptGetEnabled(XInterleaver *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_IER);
}

u32 XInterleaver_InterruptGetStatus(XInterleaver *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XInterleaver_ReadReg(InstancePtr->Ctrl_BaseAddress, XINTERLEAVER_CTRL_ADDR_ISR);
}

