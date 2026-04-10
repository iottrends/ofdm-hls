// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XCONV_ENC_H
#define XCONV_ENC_H

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
#include "xconv_enc_hw.h"

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
} XConv_enc_Config;
#endif

typedef struct {
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XConv_enc;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XConv_enc_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XConv_enc_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XConv_enc_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XConv_enc_ReadReg(BaseAddress, RegOffset) \
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
int XConv_enc_Initialize(XConv_enc *InstancePtr, UINTPTR BaseAddress);
XConv_enc_Config* XConv_enc_LookupConfig(UINTPTR BaseAddress);
#else
int XConv_enc_Initialize(XConv_enc *InstancePtr, u16 DeviceId);
XConv_enc_Config* XConv_enc_LookupConfig(u16 DeviceId);
#endif
int XConv_enc_CfgInitialize(XConv_enc *InstancePtr, XConv_enc_Config *ConfigPtr);
#else
int XConv_enc_Initialize(XConv_enc *InstancePtr, const char* InstanceName);
int XConv_enc_Release(XConv_enc *InstancePtr);
#endif

void XConv_enc_Start(XConv_enc *InstancePtr);
u32 XConv_enc_IsDone(XConv_enc *InstancePtr);
u32 XConv_enc_IsIdle(XConv_enc *InstancePtr);
u32 XConv_enc_IsReady(XConv_enc *InstancePtr);
void XConv_enc_EnableAutoRestart(XConv_enc *InstancePtr);
void XConv_enc_DisableAutoRestart(XConv_enc *InstancePtr);

void XConv_enc_Set_rate(XConv_enc *InstancePtr, u32 Data);
u32 XConv_enc_Get_rate(XConv_enc *InstancePtr);
void XConv_enc_Set_n_data_bytes(XConv_enc *InstancePtr, u32 Data);
u32 XConv_enc_Get_n_data_bytes(XConv_enc *InstancePtr);

void XConv_enc_InterruptGlobalEnable(XConv_enc *InstancePtr);
void XConv_enc_InterruptGlobalDisable(XConv_enc *InstancePtr);
void XConv_enc_InterruptEnable(XConv_enc *InstancePtr, u32 Mask);
void XConv_enc_InterruptDisable(XConv_enc *InstancePtr, u32 Mask);
void XConv_enc_InterruptClear(XConv_enc *InstancePtr, u32 Mask);
u32 XConv_enc_InterruptGetEnabled(XConv_enc *InstancePtr);
u32 XConv_enc_InterruptGetStatus(XConv_enc *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
