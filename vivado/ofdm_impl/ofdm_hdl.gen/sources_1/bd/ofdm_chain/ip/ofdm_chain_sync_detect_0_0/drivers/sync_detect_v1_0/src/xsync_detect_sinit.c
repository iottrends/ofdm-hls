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
#include "xsync_detect.h"

extern XSync_detect_Config XSync_detect_ConfigTable[];

#ifdef SDT
XSync_detect_Config *XSync_detect_LookupConfig(UINTPTR BaseAddress) {
	XSync_detect_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XSync_detect_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XSync_detect_ConfigTable[Index].Stat_BaseAddress == BaseAddress) {
			ConfigPtr = &XSync_detect_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XSync_detect_Initialize(XSync_detect *InstancePtr, UINTPTR BaseAddress) {
	XSync_detect_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XSync_detect_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XSync_detect_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XSync_detect_Config *XSync_detect_LookupConfig(u16 DeviceId) {
	XSync_detect_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XSYNC_DETECT_NUM_INSTANCES; Index++) {
		if (XSync_detect_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XSync_detect_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XSync_detect_Initialize(XSync_detect *InstancePtr, u16 DeviceId) {
	XSync_detect_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XSync_detect_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XSync_detect_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

