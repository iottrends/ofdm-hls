// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xscrambler.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XScrambler_CfgInitialize(XScrambler *InstancePtr, XScrambler_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XScrambler_Start(XScrambler *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_AP_CTRL) & 0x80;
    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XScrambler_IsDone(XScrambler *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XScrambler_IsIdle(XScrambler *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XScrambler_IsReady(XScrambler *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XScrambler_EnableAutoRestart(XScrambler *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_AP_CTRL, 0x80);
}

void XScrambler_DisableAutoRestart(XScrambler *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_AP_CTRL, 0);
}

void XScrambler_Set_n_bytes(XScrambler *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_N_BYTES_DATA, Data);
}

u32 XScrambler_Get_n_bytes(XScrambler *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_N_BYTES_DATA);
    return Data;
}

void XScrambler_InterruptGlobalEnable(XScrambler *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_GIE, 1);
}

void XScrambler_InterruptGlobalDisable(XScrambler *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_GIE, 0);
}

void XScrambler_InterruptEnable(XScrambler *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_IER);
    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_IER, Register | Mask);
}

void XScrambler_InterruptDisable(XScrambler *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_IER);
    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_IER, Register & (~Mask));
}

void XScrambler_InterruptClear(XScrambler *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XScrambler_WriteReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_ISR, Mask);
}

u32 XScrambler_InterruptGetEnabled(XScrambler *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_IER);
}

u32 XScrambler_InterruptGetStatus(XScrambler *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XScrambler_ReadReg(InstancePtr->Ctrl_BaseAddress, XSCRAMBLER_CTRL_ADDR_ISR);
}

