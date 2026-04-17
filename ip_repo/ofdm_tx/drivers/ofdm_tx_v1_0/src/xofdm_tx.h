// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XOFDM_TX_H
#define XOFDM_TX_H

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
#include "xofdm_tx_hw.h"

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
    u64 Ctrl_BaseAddress;
} XOfdm_tx_Config;
#endif

typedef struct {
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XOfdm_tx;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XOfdm_tx_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XOfdm_tx_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XOfdm_tx_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XOfdm_tx_ReadReg(BaseAddress, RegOffset) \
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
int XOfdm_tx_Initialize(XOfdm_tx *InstancePtr, UINTPTR BaseAddress);
XOfdm_tx_Config* XOfdm_tx_LookupConfig(UINTPTR BaseAddress);
#else
int XOfdm_tx_Initialize(XOfdm_tx *InstancePtr, u16 DeviceId);
XOfdm_tx_Config* XOfdm_tx_LookupConfig(u16 DeviceId);
#endif
int XOfdm_tx_CfgInitialize(XOfdm_tx *InstancePtr, XOfdm_tx_Config *ConfigPtr);
#else
int XOfdm_tx_Initialize(XOfdm_tx *InstancePtr, const char* InstanceName);
int XOfdm_tx_Release(XOfdm_tx *InstancePtr);
#endif

void XOfdm_tx_Start(XOfdm_tx *InstancePtr);
u32 XOfdm_tx_IsDone(XOfdm_tx *InstancePtr);
u32 XOfdm_tx_IsIdle(XOfdm_tx *InstancePtr);
u32 XOfdm_tx_IsReady(XOfdm_tx *InstancePtr);
void XOfdm_tx_EnableAutoRestart(XOfdm_tx *InstancePtr);
void XOfdm_tx_DisableAutoRestart(XOfdm_tx *InstancePtr);

void XOfdm_tx_Set_modcod(XOfdm_tx *InstancePtr, u32 Data);
u32 XOfdm_tx_Get_modcod(XOfdm_tx *InstancePtr);
void XOfdm_tx_Set_n_syms(XOfdm_tx *InstancePtr, u32 Data);
u32 XOfdm_tx_Get_n_syms(XOfdm_tx *InstancePtr);

void XOfdm_tx_InterruptGlobalEnable(XOfdm_tx *InstancePtr);
void XOfdm_tx_InterruptGlobalDisable(XOfdm_tx *InstancePtr);
void XOfdm_tx_InterruptEnable(XOfdm_tx *InstancePtr, u32 Mask);
void XOfdm_tx_InterruptDisable(XOfdm_tx *InstancePtr, u32 Mask);
void XOfdm_tx_InterruptClear(XOfdm_tx *InstancePtr, u32 Mask);
u32 XOfdm_tx_InterruptGetEnabled(XOfdm_tx *InstancePtr);
u32 XOfdm_tx_InterruptGetStatus(XOfdm_tx *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
