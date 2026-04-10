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

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XOfdm_rx_Start(XOfdm_rx *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_AP_CTRL) & 0x80;
    XOfdm_rx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XOfdm_rx_IsDone(XOfdm_rx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XOfdm_rx_IsIdle(XOfdm_rx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XOfdm_rx_IsReady(XOfdm_rx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XOfdm_rx_EnableAutoRestart(XOfdm_rx *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_rx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_AP_CTRL, 0x80);
}

void XOfdm_rx_DisableAutoRestart(XOfdm_rx *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_rx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_AP_CTRL, 0);
}

u32 XOfdm_rx_Get_header_err(XOfdm_rx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_HEADER_ERR_DATA);
    return Data;
}

u32 XOfdm_rx_Get_header_err_vld(XOfdm_rx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_HEADER_ERR_CTRL);
    return Data & 0x1;
}

void XOfdm_rx_InterruptGlobalEnable(XOfdm_rx *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_rx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_GIE, 1);
}

void XOfdm_rx_InterruptGlobalDisable(XOfdm_rx *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_rx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_GIE, 0);
}

void XOfdm_rx_InterruptEnable(XOfdm_rx *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_IER);
    XOfdm_rx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_IER, Register | Mask);
}

void XOfdm_rx_InterruptDisable(XOfdm_rx *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_IER);
    XOfdm_rx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_IER, Register & (~Mask));
}

void XOfdm_rx_InterruptClear(XOfdm_rx *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_rx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_ISR, Mask);
}

u32 XOfdm_rx_InterruptGetEnabled(XOfdm_rx *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_IER);
}

u32 XOfdm_rx_InterruptGetStatus(XOfdm_rx *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XOfdm_rx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_RX_CTRL_ADDR_ISR);
}

