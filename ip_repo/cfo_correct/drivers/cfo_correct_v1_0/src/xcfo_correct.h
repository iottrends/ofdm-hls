// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XCFO_CORRECT_H
#define XCFO_CORRECT_H

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
#include "xcfo_correct_hw.h"

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
} XCfo_correct_Config;
#endif

typedef struct {
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XCfo_correct;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XCfo_correct_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XCfo_correct_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XCfo_correct_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XCfo_correct_ReadReg(BaseAddress, RegOffset) \
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
int XCfo_correct_Initialize(XCfo_correct *InstancePtr, UINTPTR BaseAddress);
XCfo_correct_Config* XCfo_correct_LookupConfig(UINTPTR BaseAddress);
#else
int XCfo_correct_Initialize(XCfo_correct *InstancePtr, u16 DeviceId);
XCfo_correct_Config* XCfo_correct_LookupConfig(u16 DeviceId);
#endif
int XCfo_correct_CfgInitialize(XCfo_correct *InstancePtr, XCfo_correct_Config *ConfigPtr);
#else
int XCfo_correct_Initialize(XCfo_correct *InstancePtr, const char* InstanceName);
int XCfo_correct_Release(XCfo_correct *InstancePtr);
#endif

void XCfo_correct_Start(XCfo_correct *InstancePtr);
u32 XCfo_correct_IsDone(XCfo_correct *InstancePtr);
u32 XCfo_correct_IsIdle(XCfo_correct *InstancePtr);
u32 XCfo_correct_IsReady(XCfo_correct *InstancePtr);
void XCfo_correct_EnableAutoRestart(XCfo_correct *InstancePtr);
void XCfo_correct_DisableAutoRestart(XCfo_correct *InstancePtr);

void XCfo_correct_Set_cfo_est(XCfo_correct *InstancePtr, u32 Data);
u32 XCfo_correct_Get_cfo_est(XCfo_correct *InstancePtr);
void XCfo_correct_Set_n_syms(XCfo_correct *InstancePtr, u32 Data);
u32 XCfo_correct_Get_n_syms(XCfo_correct *InstancePtr);

void XCfo_correct_InterruptGlobalEnable(XCfo_correct *InstancePtr);
void XCfo_correct_InterruptGlobalDisable(XCfo_correct *InstancePtr);
void XCfo_correct_InterruptEnable(XCfo_correct *InstancePtr, u32 Mask);
void XCfo_correct_InterruptDisable(XCfo_correct *InstancePtr, u32 Mask);
void XCfo_correct_InterruptClear(XCfo_correct *InstancePtr, u32 Mask);
u32 XCfo_correct_InterruptGetEnabled(XCfo_correct *InstancePtr);
u32 XCfo_correct_InterruptGetStatus(XCfo_correct *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
