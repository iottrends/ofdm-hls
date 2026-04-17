// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef XOFDM_RX_H
#define XOFDM_RX_H

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
#include "xofdm_rx_hw.h"

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
} XOfdm_rx_Config;
#endif

typedef struct {
    u64 Stat_BaseAddress;
    u32 IsReady;
} XOfdm_rx;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XOfdm_rx_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XOfdm_rx_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XOfdm_rx_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XOfdm_rx_ReadReg(BaseAddress, RegOffset) \
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
int XOfdm_rx_Initialize(XOfdm_rx *InstancePtr, UINTPTR BaseAddress);
XOfdm_rx_Config* XOfdm_rx_LookupConfig(UINTPTR BaseAddress);
#else
int XOfdm_rx_Initialize(XOfdm_rx *InstancePtr, u16 DeviceId);
XOfdm_rx_Config* XOfdm_rx_LookupConfig(u16 DeviceId);
#endif
int XOfdm_rx_CfgInitialize(XOfdm_rx *InstancePtr, XOfdm_rx_Config *ConfigPtr);
#else
int XOfdm_rx_Initialize(XOfdm_rx *InstancePtr, const char* InstanceName);
int XOfdm_rx_Release(XOfdm_rx *InstancePtr);
#endif


u32 XOfdm_rx_Get_header_err(XOfdm_rx *InstancePtr);
u32 XOfdm_rx_Get_header_err_vld(XOfdm_rx *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
