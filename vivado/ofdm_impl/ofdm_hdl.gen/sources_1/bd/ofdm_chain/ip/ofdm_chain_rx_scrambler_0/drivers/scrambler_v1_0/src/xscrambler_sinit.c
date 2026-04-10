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
#include "xscrambler.h"

extern XScrambler_Config XScrambler_ConfigTable[];

#ifdef SDT
XScrambler_Config *XScrambler_LookupConfig(UINTPTR BaseAddress) {
	XScrambler_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XScrambler_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XScrambler_ConfigTable[Index].Ctrl_BaseAddress == BaseAddress) {
			ConfigPtr = &XScrambler_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XScrambler_Initialize(XScrambler *InstancePtr, UINTPTR BaseAddress) {
	XScrambler_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XScrambler_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XScrambler_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XScrambler_Config *XScrambler_LookupConfig(u16 DeviceId) {
	XScrambler_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XSCRAMBLER_NUM_INSTANCES; Index++) {
		if (XScrambler_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XScrambler_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XScrambler_Initialize(XScrambler *InstancePtr, u16 DeviceId) {
	XScrambler_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XScrambler_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XScrambler_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

