// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xcfo_correct.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XCfo_correct_CfgInitialize(XCfo_correct *InstancePtr, XCfo_correct_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XCfo_correct_Start(XCfo_correct *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XCfo_correct_ReadReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_AP_CTRL) & 0x80;
    XCfo_correct_WriteReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XCfo_correct_IsDone(XCfo_correct *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XCfo_correct_ReadReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XCfo_correct_IsIdle(XCfo_correct *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XCfo_correct_ReadReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XCfo_correct_IsReady(XCfo_correct *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XCfo_correct_ReadReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XCfo_correct_EnableAutoRestart(XCfo_correct *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XCfo_correct_WriteReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_AP_CTRL, 0x80);
}

void XCfo_correct_DisableAutoRestart(XCfo_correct *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XCfo_correct_WriteReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_AP_CTRL, 0);
}

void XCfo_correct_InterruptGlobalEnable(XCfo_correct *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XCfo_correct_WriteReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_GIE, 1);
}

void XCfo_correct_InterruptGlobalDisable(XCfo_correct *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XCfo_correct_WriteReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_GIE, 0);
}

void XCfo_correct_InterruptEnable(XCfo_correct *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XCfo_correct_ReadReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_IER);
    XCfo_correct_WriteReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_IER, Register | Mask);
}

void XCfo_correct_InterruptDisable(XCfo_correct *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XCfo_correct_ReadReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_IER);
    XCfo_correct_WriteReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_IER, Register & (~Mask));
}

void XCfo_correct_InterruptClear(XCfo_correct *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XCfo_correct_WriteReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_ISR, Mask);
}

u32 XCfo_correct_InterruptGetEnabled(XCfo_correct *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XCfo_correct_ReadReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_IER);
}

u32 XCfo_correct_InterruptGetStatus(XCfo_correct *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XCfo_correct_ReadReg(InstancePtr->Ctrl_BaseAddress, XCFO_CORRECT_CTRL_ADDR_ISR);
}

