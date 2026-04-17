// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
/***************************** Include Files *********************************/
#include "xofdm_mac.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XOfdm_mac_CfgInitialize(XOfdm_mac *InstancePtr, XOfdm_mac_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XOfdm_mac_Start(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_AP_CTRL) & 0x80;
    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XOfdm_mac_IsDone(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XOfdm_mac_IsIdle(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XOfdm_mac_IsReady(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XOfdm_mac_EnableAutoRestart(XOfdm_mac *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_AP_CTRL, 0x80);
}

void XOfdm_mac_DisableAutoRestart(XOfdm_mac *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_AP_CTRL, 0);
}

void XOfdm_mac_Set_phy_csr(XOfdm_mac *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Control_BaseAddress, XOFDM_MAC_CONTROL_ADDR_PHY_CSR_DATA, (u32)(Data));
    XOfdm_mac_WriteReg(InstancePtr->Control_BaseAddress, XOFDM_MAC_CONTROL_ADDR_PHY_CSR_DATA + 4, (u32)(Data >> 32));
}

u64 XOfdm_mac_Get_phy_csr(XOfdm_mac *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Control_BaseAddress, XOFDM_MAC_CONTROL_ADDR_PHY_CSR_DATA);
    Data += (u64)XOfdm_mac_ReadReg(InstancePtr->Control_BaseAddress, XOFDM_MAC_CONTROL_ADDR_PHY_CSR_DATA + 4) << 32;
    return Data;
}

void XOfdm_mac_Set_my_mac_addr(XOfdm_mac *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_MY_MAC_ADDR_DATA, (u32)(Data));
    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_MY_MAC_ADDR_DATA + 4, (u32)(Data >> 32));
}

u64 XOfdm_mac_Get_my_mac_addr(XOfdm_mac *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_MY_MAC_ADDR_DATA);
    Data += (u64)XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_MY_MAC_ADDR_DATA + 4) << 32;
    return Data;
}

void XOfdm_mac_Set_promisc(XOfdm_mac *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_PROMISC_DATA, Data);
}

u32 XOfdm_mac_Get_promisc(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_PROMISC_DATA);
    return Data;
}

void XOfdm_mac_Set_modcod(XOfdm_mac *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_MODCOD_DATA, Data);
}

u32 XOfdm_mac_Get_modcod(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_MODCOD_DATA);
    return Data;
}

void XOfdm_mac_Set_mac_enable(XOfdm_mac *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_MAC_ENABLE_DATA, Data);
}

u32 XOfdm_mac_Get_mac_enable(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_MAC_ENABLE_DATA);
    return Data;
}

void XOfdm_mac_Set_tx_pkt_count_i(XOfdm_mac *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_TX_PKT_COUNT_I_DATA, Data);
}

u32 XOfdm_mac_Get_tx_pkt_count_i(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_TX_PKT_COUNT_I_DATA);
    return Data;
}

u32 XOfdm_mac_Get_tx_pkt_count_o(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_TX_PKT_COUNT_O_DATA);
    return Data;
}

u32 XOfdm_mac_Get_tx_pkt_count_o_vld(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_TX_PKT_COUNT_O_CTRL);
    return Data & 0x1;
}

void XOfdm_mac_Set_rx_pkt_count_i(XOfdm_mac *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_PKT_COUNT_I_DATA, Data);
}

u32 XOfdm_mac_Get_rx_pkt_count_i(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_PKT_COUNT_I_DATA);
    return Data;
}

u32 XOfdm_mac_Get_rx_pkt_count_o(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_PKT_COUNT_O_DATA);
    return Data;
}

u32 XOfdm_mac_Get_rx_pkt_count_o_vld(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_PKT_COUNT_O_CTRL);
    return Data & 0x1;
}

void XOfdm_mac_Set_rx_drop_count_i(XOfdm_mac *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_DROP_COUNT_I_DATA, Data);
}

u32 XOfdm_mac_Get_rx_drop_count_i(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_DROP_COUNT_I_DATA);
    return Data;
}

u32 XOfdm_mac_Get_rx_drop_count_o(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_DROP_COUNT_O_DATA);
    return Data;
}

u32 XOfdm_mac_Get_rx_drop_count_o_vld(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_DROP_COUNT_O_CTRL);
    return Data & 0x1;
}

void XOfdm_mac_Set_rx_fcs_err_count_i(XOfdm_mac *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_FCS_ERR_COUNT_I_DATA, Data);
}

u32 XOfdm_mac_Get_rx_fcs_err_count_i(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_FCS_ERR_COUNT_I_DATA);
    return Data;
}

u32 XOfdm_mac_Get_rx_fcs_err_count_o(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_FCS_ERR_COUNT_O_DATA);
    return Data;
}

u32 XOfdm_mac_Get_rx_fcs_err_count_o_vld(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_FCS_ERR_COUNT_O_CTRL);
    return Data & 0x1;
}

u32 XOfdm_mac_Get_last_rx_modcod(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_LAST_RX_MODCOD_DATA);
    return Data;
}

u32 XOfdm_mac_Get_last_rx_modcod_vld(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_LAST_RX_MODCOD_CTRL);
    return Data & 0x1;
}

u32 XOfdm_mac_Get_last_rx_n_syms(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_LAST_RX_N_SYMS_DATA);
    return Data;
}

u32 XOfdm_mac_Get_last_rx_n_syms_vld(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_LAST_RX_N_SYMS_CTRL);
    return Data & 0x1;
}

void XOfdm_mac_Set_rx_hdr_err_count_i(XOfdm_mac *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_HDR_ERR_COUNT_I_DATA, Data);
}

u32 XOfdm_mac_Get_rx_hdr_err_count_i(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_HDR_ERR_COUNT_I_DATA);
    return Data;
}

u32 XOfdm_mac_Get_rx_hdr_err_count_o(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_HDR_ERR_COUNT_O_DATA);
    return Data;
}

u32 XOfdm_mac_Get_rx_hdr_err_count_o_vld(XOfdm_mac *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_RX_HDR_ERR_COUNT_O_CTRL);
    return Data & 0x1;
}

void XOfdm_mac_InterruptGlobalEnable(XOfdm_mac *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_GIE, 1);
}

void XOfdm_mac_InterruptGlobalDisable(XOfdm_mac *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_GIE, 0);
}

void XOfdm_mac_InterruptEnable(XOfdm_mac *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_IER);
    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_IER, Register | Mask);
}

void XOfdm_mac_InterruptDisable(XOfdm_mac *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_IER);
    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_IER, Register & (~Mask));
}

void XOfdm_mac_InterruptClear(XOfdm_mac *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XOfdm_mac_WriteReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_ISR, Mask);
}

u32 XOfdm_mac_InterruptGetEnabled(XOfdm_mac *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_IER);
}

u32 XOfdm_mac_InterruptGetStatus(XOfdm_mac *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XOfdm_mac_ReadReg(InstancePtr->Ctrl_BaseAddress, XOFDM_MAC_CTRL_ADDR_ISR);
}

