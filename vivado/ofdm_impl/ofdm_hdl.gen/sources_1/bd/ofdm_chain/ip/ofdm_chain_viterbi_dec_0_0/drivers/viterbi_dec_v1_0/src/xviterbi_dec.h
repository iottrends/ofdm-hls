// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XVITERBI_DEC_H
#define XVITERBI_DEC_H

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
#include "xviterbi_dec_hw.h"

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
} XViterbi_dec_Config;
#endif

typedef struct {
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XViterbi_dec;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XViterbi_dec_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XViterbi_dec_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XViterbi_dec_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XViterbi_dec_ReadReg(BaseAddress, RegOffset) \
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
int XViterbi_dec_Initialize(XViterbi_dec *InstancePtr, UINTPTR BaseAddress);
XViterbi_dec_Config* XViterbi_dec_LookupConfig(UINTPTR BaseAddress);
#else
int XViterbi_dec_Initialize(XViterbi_dec *InstancePtr, u16 DeviceId);
XViterbi_dec_Config* XViterbi_dec_LookupConfig(u16 DeviceId);
#endif
int XViterbi_dec_CfgInitialize(XViterbi_dec *InstancePtr, XViterbi_dec_Config *ConfigPtr);
#else
int XViterbi_dec_Initialize(XViterbi_dec *InstancePtr, const char* InstanceName);
int XViterbi_dec_Release(XViterbi_dec *InstancePtr);
#endif

void XViterbi_dec_Start(XViterbi_dec *InstancePtr);
u32 XViterbi_dec_IsDone(XViterbi_dec *InstancePtr);
u32 XViterbi_dec_IsIdle(XViterbi_dec *InstancePtr);
u32 XViterbi_dec_IsReady(XViterbi_dec *InstancePtr);
void XViterbi_dec_EnableAutoRestart(XViterbi_dec *InstancePtr);
void XViterbi_dec_DisableAutoRestart(XViterbi_dec *InstancePtr);

void XViterbi_dec_Set_rate(XViterbi_dec *InstancePtr, u32 Data);
u32 XViterbi_dec_Get_rate(XViterbi_dec *InstancePtr);
void XViterbi_dec_Set_n_data_bytes(XViterbi_dec *InstancePtr, u32 Data);
u32 XViterbi_dec_Get_n_data_bytes(XViterbi_dec *InstancePtr);

void XViterbi_dec_InterruptGlobalEnable(XViterbi_dec *InstancePtr);
void XViterbi_dec_InterruptGlobalDisable(XViterbi_dec *InstancePtr);
void XViterbi_dec_InterruptEnable(XViterbi_dec *InstancePtr, u32 Mask);
void XViterbi_dec_InterruptDisable(XViterbi_dec *InstancePtr, u32 Mask);
void XViterbi_dec_InterruptClear(XViterbi_dec *InstancePtr, u32 Mask);
u32 XViterbi_dec_InterruptGetEnabled(XViterbi_dec *InstancePtr);
u32 XViterbi_dec_InterruptGetStatus(XViterbi_dec *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
