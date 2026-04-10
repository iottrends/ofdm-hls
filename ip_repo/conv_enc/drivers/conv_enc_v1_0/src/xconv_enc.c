// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xconv_enc.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XConv_enc_CfgInitialize(XConv_enc *InstancePtr, XConv_enc_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XConv_enc_Start(XConv_enc *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_AP_CTRL) & 0x80;
    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XConv_enc_IsDone(XConv_enc *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XConv_enc_IsIdle(XConv_enc *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XConv_enc_IsReady(XConv_enc *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XConv_enc_EnableAutoRestart(XConv_enc *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_AP_CTRL, 0x80);
}

void XConv_enc_DisableAutoRestart(XConv_enc *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_AP_CTRL, 0);
}

void XConv_enc_Set_rate(XConv_enc *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_RATE_DATA, Data);
}

u32 XConv_enc_Get_rate(XConv_enc *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_RATE_DATA);
    return Data;
}

void XConv_enc_Set_n_data_bytes(XConv_enc *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_N_DATA_BYTES_DATA, Data);
}

u32 XConv_enc_Get_n_data_bytes(XConv_enc *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_N_DATA_BYTES_DATA);
    return Data;
}

void XConv_enc_InterruptGlobalEnable(XConv_enc *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_GIE, 1);
}

void XConv_enc_InterruptGlobalDisable(XConv_enc *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_GIE, 0);
}

void XConv_enc_InterruptEnable(XConv_enc *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_IER);
    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_IER, Register | Mask);
}

void XConv_enc_InterruptDisable(XConv_enc *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_IER);
    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_IER, Register & (~Mask));
}

void XConv_enc_InterruptClear(XConv_enc *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_enc_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_ISR, Mask);
}

u32 XConv_enc_InterruptGetEnabled(XConv_enc *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_IER);
}

u32 XConv_enc_InterruptGetStatus(XConv_enc *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XConv_enc_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_ENC_CTRL_ADDR_ISR);
}

