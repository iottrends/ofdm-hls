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
#include "xcfo_correct.h"

extern XCfo_correct_Config XCfo_correct_ConfigTable[];

#ifdef SDT
XCfo_correct_Config *XCfo_correct_LookupConfig(UINTPTR BaseAddress) {
	XCfo_correct_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XCfo_correct_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XCfo_correct_ConfigTable[Index].Ctrl_BaseAddress == BaseAddress) {
			ConfigPtr = &XCfo_correct_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XCfo_correct_Initialize(XCfo_correct *InstancePtr, UINTPTR BaseAddress) {
	XCfo_correct_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XCfo_correct_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XCfo_correct_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XCfo_correct_Config *XCfo_correct_LookupConfig(u16 DeviceId) {
	XCfo_correct_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XCFO_CORRECT_NUM_INSTANCES; Index++) {
		if (XCfo_correct_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XCfo_correct_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XCfo_correct_Initialize(XCfo_correct *InstancePtr, u16 DeviceId) {
	XCfo_correct_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XCfo_correct_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XCfo_correct_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

