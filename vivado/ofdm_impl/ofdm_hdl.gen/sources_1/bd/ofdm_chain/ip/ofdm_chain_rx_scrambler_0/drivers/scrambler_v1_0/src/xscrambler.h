// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XSCRAMBLER_H
#define XSCRAMBLER_H

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
#include "xscrambler_hw.h"

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
} XScrambler_Config;
#endif

typedef struct {
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XScrambler;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XScrambler_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XScrambler_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XScrambler_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XScrambler_ReadReg(BaseAddress, RegOffset) \
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
int XScrambler_Initialize(XScrambler *InstancePtr, UINTPTR BaseAddress);
XScrambler_Config* XScrambler_LookupConfig(UINTPTR BaseAddress);
#else
int XScrambler_Initialize(XScrambler *InstancePtr, u16 DeviceId);
XScrambler_Config* XScrambler_LookupConfig(u16 DeviceId);
#endif
int XScrambler_CfgInitialize(XScrambler *InstancePtr, XScrambler_Config *ConfigPtr);
#else
int XScrambler_Initialize(XScrambler *InstancePtr, const char* InstanceName);
int XScrambler_Release(XScrambler *InstancePtr);
#endif

void XScrambler_Start(XScrambler *InstancePtr);
u32 XScrambler_IsDone(XScrambler *InstancePtr);
u32 XScrambler_IsIdle(XScrambler *InstancePtr);
u32 XScrambler_IsReady(XScrambler *InstancePtr);
void XScrambler_EnableAutoRestart(XScrambler *InstancePtr);
void XScrambler_DisableAutoRestart(XScrambler *InstancePtr);

void XScrambler_Set_n_bytes(XScrambler *InstancePtr, u32 Data);
u32 XScrambler_Get_n_bytes(XScrambler *InstancePtr);

void XScrambler_InterruptGlobalEnable(XScrambler *InstancePtr);
void XScrambler_InterruptGlobalDisable(XScrambler *InstancePtr);
void XScrambler_InterruptEnable(XScrambler *InstancePtr, u32 Mask);
void XScrambler_InterruptDisable(XScrambler *InstancePtr, u32 Mask);
void XScrambler_InterruptClear(XScrambler *InstancePtr, u32 Mask);
u32 XScrambler_InterruptGetEnabled(XScrambler *InstancePtr);
u32 XScrambler_InterruptGetStatus(XScrambler *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
