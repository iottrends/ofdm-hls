// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XINTERLEAVER_H
#define XINTERLEAVER_H

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
#include "xinterleaver_hw.h"

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
} XInterleaver_Config;
#endif

typedef struct {
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XInterleaver;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XInterleaver_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XInterleaver_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XInterleaver_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XInterleaver_ReadReg(BaseAddress, RegOffset) \
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
int XInterleaver_Initialize(XInterleaver *InstancePtr, UINTPTR BaseAddress);
XInterleaver_Config* XInterleaver_LookupConfig(UINTPTR BaseAddress);
#else
int XInterleaver_Initialize(XInterleaver *InstancePtr, u16 DeviceId);
XInterleaver_Config* XInterleaver_LookupConfig(u16 DeviceId);
#endif
int XInterleaver_CfgInitialize(XInterleaver *InstancePtr, XInterleaver_Config *ConfigPtr);
#else
int XInterleaver_Initialize(XInterleaver *InstancePtr, const char* InstanceName);
int XInterleaver_Release(XInterleaver *InstancePtr);
#endif

void XInterleaver_Start(XInterleaver *InstancePtr);
u32 XInterleaver_IsDone(XInterleaver *InstancePtr);
u32 XInterleaver_IsIdle(XInterleaver *InstancePtr);
u32 XInterleaver_IsReady(XInterleaver *InstancePtr);
void XInterleaver_EnableAutoRestart(XInterleaver *InstancePtr);
void XInterleaver_DisableAutoRestart(XInterleaver *InstancePtr);

void XInterleaver_Set_mod_r(XInterleaver *InstancePtr, u32 Data);
u32 XInterleaver_Get_mod_r(XInterleaver *InstancePtr);
void XInterleaver_Set_n_syms(XInterleaver *InstancePtr, u32 Data);
u32 XInterleaver_Get_n_syms(XInterleaver *InstancePtr);
void XInterleaver_Set_is_rx(XInterleaver *InstancePtr, u32 Data);
u32 XInterleaver_Get_is_rx(XInterleaver *InstancePtr);

void XInterleaver_InterruptGlobalEnable(XInterleaver *InstancePtr);
void XInterleaver_InterruptGlobalDisable(XInterleaver *InstancePtr);
void XInterleaver_InterruptEnable(XInterleaver *InstancePtr, u32 Mask);
void XInterleaver_InterruptDisable(XInterleaver *InstancePtr, u32 Mask);
void XInterleaver_InterruptClear(XInterleaver *InstancePtr, u32 Mask);
u32 XInterleaver_InterruptGetEnabled(XInterleaver *InstancePtr);
u32 XInterleaver_InterruptGetStatus(XInterleaver *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
