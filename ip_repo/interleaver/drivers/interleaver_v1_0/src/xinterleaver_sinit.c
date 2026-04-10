// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2025.2 (64-bit)
// Tool Version Limit: 2025.11
// Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
// Copyright 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// 
// ==============================================================
#ifndef __linux__

#include "xstatus.h"
#ifdef SDT
#include "xparameters.h"
#endif
#include "xinterleaver.h"

extern XInterleaver_Config XInterleaver_ConfigTable[];

#ifdef SDT
XInterleaver_Config *XInterleaver_LookupConfig(UINTPTR BaseAddress) {
	XInterleaver_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XInterleaver_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XInterleaver_ConfigTable[Index].Ctrl_BaseAddress == BaseAddress) {
			ConfigPtr = &XInterleaver_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XInterleaver_Initialize(XInterleaver *InstancePtr, UINTPTR BaseAddress) {
	XInterleaver_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XInterleaver_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XInterleaver_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XInterleaver_Config *XInterleaver_LookupConfig(u16 DeviceId) {
	XInterleaver_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XINTERLEAVER_NUM_INSTANCES; Index++) {
		if (XInterleaver_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XInterleaver_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XInterleaver_Initialize(XInterleaver *InstancePtr, u16 DeviceId) {
	XInterleaver_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XInterleaver_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XInterleaver_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

