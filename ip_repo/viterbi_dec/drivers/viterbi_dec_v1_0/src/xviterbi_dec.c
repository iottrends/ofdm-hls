// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xviterbi_dec.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XViterbi_dec_CfgInitialize(XViterbi_dec *InstancePtr, XViterbi_dec_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XViterbi_dec_Start(XViterbi_dec *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_AP_CTRL) & 0x80;
    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XViterbi_dec_IsDone(XViterbi_dec *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XViterbi_dec_IsIdle(XViterbi_dec *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XViterbi_dec_IsReady(XViterbi_dec *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XViterbi_dec_EnableAutoRestart(XViterbi_dec *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_AP_CTRL, 0x80);
}

void XViterbi_dec_DisableAutoRestart(XViterbi_dec *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_AP_CTRL, 0);
}

void XViterbi_dec_Set_rate(XViterbi_dec *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_RATE_DATA, Data);
}

u32 XViterbi_dec_Get_rate(XViterbi_dec *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_RATE_DATA);
    return Data;
}

void XViterbi_dec_Set_n_data_bytes(XViterbi_dec *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_N_DATA_BYTES_DATA, Data);
}

u32 XViterbi_dec_Get_n_data_bytes(XViterbi_dec *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_N_DATA_BYTES_DATA);
    return Data;
}

void XViterbi_dec_InterruptGlobalEnable(XViterbi_dec *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_GIE, 1);
}

void XViterbi_dec_InterruptGlobalDisable(XViterbi_dec *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_GIE, 0);
}

void XViterbi_dec_InterruptEnable(XViterbi_dec *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_IER);
    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_IER, Register | Mask);
}

void XViterbi_dec_InterruptDisable(XViterbi_dec *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_IER);
    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_IER, Register & (~Mask));
}

void XViterbi_dec_InterruptClear(XViterbi_dec *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XViterbi_dec_WriteReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_ISR, Mask);
}

u32 XViterbi_dec_InterruptGetEnabled(XViterbi_dec *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_IER);
}

u32 XViterbi_dec_InterruptGetStatus(XViterbi_dec *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XViterbi_dec_ReadReg(InstancePtr->Ctrl_BaseAddress, XVITERBI_DEC_CTRL_ADDR_ISR);
}

