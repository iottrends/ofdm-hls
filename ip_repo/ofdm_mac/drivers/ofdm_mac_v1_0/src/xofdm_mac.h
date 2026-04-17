// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XOFDM_MAC_H
#define XOFDM_MAC_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xofdm_mac_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
typedef struct {
#ifdef SDT
    char *Name;
#else
    u16 DeviceId;
#endif
    u64 Control_BaseAddress;
    u64 Ctrl_BaseAddress;
} XOfdm_mac_Config;
#endif

typedef struct {
    u64 Control_BaseAddress;
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XOfdm_mac;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XOfdm_mac_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XOfdm_mac_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XOfdm_mac_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XOfdm_mac_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __linux__
#ifdef SDT
int XOfdm_mac_Initialize(XOfdm_mac *InstancePtr, UINTPTR BaseAddress);
XOfdm_mac_Config* XOfdm_mac_LookupConfig(UINTPTR BaseAddress);
#else
int XOfdm_mac_Initialize(XOfdm_mac *InstancePtr, u16 DeviceId);
XOfdm_mac_Config* XOfdm_mac_LookupConfig(u16 DeviceId);
#endif
int XOfdm_mac_CfgInitialize(XOfdm_mac *InstancePtr, XOfdm_mac_Config *ConfigPtr);
#else
int XOfdm_mac_Initialize(XOfdm_mac *InstancePtr, const char* InstanceName);
int XOfdm_mac_Release(XOfdm_mac *InstancePtr);
#endif

void XOfdm_mac_Start(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_IsDone(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_IsIdle(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_IsReady(XOfdm_mac *InstancePtr);
void XOfdm_mac_EnableAutoRestart(XOfdm_mac *InstancePtr);
void XOfdm_mac_DisableAutoRestart(XOfdm_mac *InstancePtr);

void XOfdm_mac_Set_phy_csr(XOfdm_mac *InstancePtr, u64 Data);
u64 XOfdm_mac_Get_phy_csr(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_my_mac_addr(XOfdm_mac *InstancePtr, u64 Data);
u64 XOfdm_mac_Get_my_mac_addr(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_promisc(XOfdm_mac *InstancePtr, u32 Data);
u32 XOfdm_mac_Get_promisc(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_modcod(XOfdm_mac *InstancePtr, u32 Data);
u32 XOfdm_mac_Get_modcod(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_mac_enable(XOfdm_mac *InstancePtr, u32 Data);
u32 XOfdm_mac_Get_mac_enable(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_tx_pkt_count_i(XOfdm_mac *InstancePtr, u32 Data);
u32 XOfdm_mac_Get_tx_pkt_count_i(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_tx_pkt_count_o(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_tx_pkt_count_o_vld(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_rx_pkt_count_i(XOfdm_mac *InstancePtr, u32 Data);
u32 XOfdm_mac_Get_rx_pkt_count_i(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_rx_pkt_count_o(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_rx_pkt_count_o_vld(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_rx_drop_count_i(XOfdm_mac *InstancePtr, u32 Data);
u32 XOfdm_mac_Get_rx_drop_count_i(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_rx_drop_count_o(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_rx_drop_count_o_vld(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_rx_fcs_err_count_i(XOfdm_mac *InstancePtr, u32 Data);
u32 XOfdm_mac_Get_rx_fcs_err_count_i(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_rx_fcs_err_count_o(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_rx_fcs_err_count_o_vld(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_last_rx_modcod(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_last_rx_modcod_vld(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_last_rx_n_syms(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_last_rx_n_syms_vld(XOfdm_mac *InstancePtr);
void XOfdm_mac_Set_rx_hdr_err_count_i(XOfdm_mac *InstancePtr, u32 Data);
u32 XOfdm_mac_Get_rx_hdr_err_count_i(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_rx_hdr_err_count_o(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_Get_rx_hdr_err_count_o_vld(XOfdm_mac *InstancePtr);

void XOfdm_mac_InterruptGlobalEnable(XOfdm_mac *InstancePtr);
void XOfdm_mac_InterruptGlobalDisable(XOfdm_mac *InstancePtr);
void XOfdm_mac_InterruptEnable(XOfdm_mac *InstancePtr, u32 Mask);
void XOfdm_mac_InterruptDisable(XOfdm_mac *InstancePtr, u32 Mask);
void XOfdm_mac_InterruptClear(XOfdm_mac *InstancePtr, u32 Mask);
u32 XOfdm_mac_InterruptGetEnabled(XOfdm_mac *InstancePtr);
u32 XOfdm_mac_InterruptGetStatus(XOfdm_mac *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
