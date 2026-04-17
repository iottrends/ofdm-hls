// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XSYNC_DETECT_H
#define XSYNC_DETECT_H

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
#include "xsync_detect_hw.h"

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
    u64 Stat_BaseAddress;
} XSync_detect_Config;
#endif

typedef struct {
    u64 Stat_BaseAddress;
    u32 IsReady;
} XSync_detect;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XSync_detect_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XSync_detect_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XSync_detect_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XSync_detect_ReadReg(BaseAddress, RegOffset) \
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
int XSync_detect_Initialize(XSync_detect *InstancePtr, UINTPTR BaseAddress);
XSync_detect_Config* XSync_detect_LookupConfig(UINTPTR BaseAddress);
#else
int XSync_detect_Initialize(XSync_detect *InstancePtr, u16 DeviceId);
XSync_detect_Config* XSync_detect_LookupConfig(u16 DeviceId);
#endif
int XSync_detect_CfgInitialize(XSync_detect *InstancePtr, XSync_detect_Config *ConfigPtr);
#else
int XSync_detect_Initialize(XSync_detect *InstancePtr, const char* InstanceName);
int XSync_detect_Release(XSync_detect *InstancePtr);
#endif


void XSync_detect_Set_pow_threshold(XSync_detect *InstancePtr, u32 Data);
u32 XSync_detect_Get_pow_threshold(XSync_detect *InstancePtr);
void XSync_detect_Set_stat_preamble_count(XSync_detect *InstancePtr, u32 Data);
u32 XSync_detect_Get_stat_preamble_count(XSync_detect *InstancePtr);
void XSync_detect_Set_stat_header_bad_count(XSync_detect *InstancePtr, u32 Data);
u32 XSync_detect_Get_stat_header_bad_count(XSync_detect *InstancePtr);
void XSync_detect_Set_stat_pow_env(XSync_detect *InstancePtr, u32 Data);
u32 XSync_detect_Get_stat_pow_env(XSync_detect *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
