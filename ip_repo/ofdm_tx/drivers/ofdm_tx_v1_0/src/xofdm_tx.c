// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xofdm_tx.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XOfdm_tx_CfgInitialize(XOfdm_tx *InstancePtr, XOfdm_tx_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XOfdm_tx_Start(XOfdm_tx *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_AP_CTRL) & 0x80;
    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XOfdm_tx_IsDone(XOfdm_tx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XOfdm_tx_IsIdle(XOfdm_tx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XOfdm_tx_IsReady(XOfdm_tx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XOfdm_tx_EnableAutoRestart(XOfdm_tx *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_AP_CTRL, 0x80);
}

void XOfdm_tx_DisableAutoRestart(XOfdm_tx *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_AP_CTRL, 0);
}

void XOfdm_tx_Set_mod_r(XOfdm_tx *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_MOD_R_DATA, Data);
}

u32 XOfdm_tx_Get_mod_r(XOfdm_tx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_MOD_R_DATA);
    return Data;
}

void XOfdm_tx_Set_n_syms(XOfdm_tx *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_N_SYMS_DATA, Data);
}

u32 XOfdm_tx_Get_n_syms(XOfdm_tx *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_N_SYMS_DATA);
    return Data;
}

void XOfdm_tx_InterruptGlobalEnable(XOfdm_tx *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_GIE, 1);
}

void XOfdm_tx_InterruptGlobalDisable(XOfdm_tx *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_GIE, 0);
}

void XOfdm_tx_InterruptEnable(XOfdm_tx *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_IER);
    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_IER, Register | Mask);
}

void XOfdm_tx_InterruptDisable(XOfdm_tx *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_IER);
    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_IER, Register & (~Mask));
}

void XOfdm_tx_InterruptClear(XOfdm_tx *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_tx_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_ISR, Mask);
}

u32 XOfdm_tx_InterruptGetEnabled(XOfdm_tx *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_IER);
}

u32 XOfdm_tx_InterruptGetStatus(XOfdm_tx *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XOfdm_tx_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_TX_CTRL_ADDR_ISR);
}

