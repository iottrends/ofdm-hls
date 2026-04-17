// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XTX_CHAIN_H
#define XTX_CHAIN_H

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
#include "xtx_chain_hw.h"

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
} XTx_chain_Config;
#endif

typedef struct {
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XTx_chain;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XTx_chain_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XTx_chain_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XTx_chain_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XTx_chain_ReadReg(BaseAddress, RegOffset) \
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
int XTx_chain_Initialize(XTx_chain *InstancePtr, UINTPTR BaseAddress);
XTx_chain_Config* XTx_chain_LookupConfig(UINTPTR BaseAddress);
#else
int XTx_chain_Initialize(XTx_chain *InstancePtr, u16 DeviceId);
XTx_chain_Config* XTx_chain_LookupConfig(u16 DeviceId);
#endif
int XTx_chain_CfgInitialize(XTx_chain *InstancePtr, XTx_chain_Config *ConfigPtr);
#else
int XTx_chain_Initialize(XTx_chain *InstancePtr, const char* InstanceName);
int XTx_chain_Release(XTx_chain *InstancePtr);
#endif

void XTx_chain_Start(XTx_chain *InstancePtr);
u32 XTx_chain_IsDone(XTx_chain *InstancePtr);
u32 XTx_chain_IsIdle(XTx_chain *InstancePtr);
u32 XTx_chain_IsReady(XTx_chain *InstancePtr);
void XTx_chain_EnableAutoRestart(XTx_chain *InstancePtr);
void XTx_chain_DisableAutoRestart(XTx_chain *InstancePtr);

void XTx_chain_Set_n_data_bytes(XTx_chain *InstancePtr, u32 Data);
u32 XTx_chain_Get_n_data_bytes(XTx_chain *InstancePtr);
void XTx_chain_Set_modcod(XTx_chain *InstancePtr, u32 Data);
u32 XTx_chain_Get_modcod(XTx_chain *InstancePtr);
void XTx_chain_Set_n_syms(XTx_chain *InstancePtr, u32 Data);
u32 XTx_chain_Get_n_syms(XTx_chain *InstancePtr);

void XTx_chain_InterruptGlobalEnable(XTx_chain *InstancePtr);
void XTx_chain_InterruptGlobalDisable(XTx_chain *InstancePtr);
void XTx_chain_InterruptEnable(XTx_chain *InstancePtr, u32 Mask);
void XTx_chain_InterruptDisable(XTx_chain *InstancePtr, u32 Mask);
void XTx_chain_InterruptClear(XTx_chain *InstancePtr, u32 Mask);
u32 XTx_chain_InterruptGetEnabled(XTx_chain *InstancePtr);
u32 XTx_chain_InterruptGetStatus(XTx_chain *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
