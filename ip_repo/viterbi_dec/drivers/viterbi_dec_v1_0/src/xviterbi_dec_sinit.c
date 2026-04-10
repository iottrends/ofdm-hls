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
#include "xviterbi_dec.h"

extern XViterbi_dec_Config XViterbi_dec_ConfigTable[];

#ifdef SDT
XViterbi_dec_Config *XViterbi_dec_LookupConfig(UINTPTR BaseAddress) {
	XViterbi_dec_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XViterbi_dec_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XViterbi_dec_ConfigTable[Index].Ctrl_BaseAddress == BaseAddress) {
			ConfigPtr = &XViterbi_dec_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XViterbi_dec_Initialize(XViterbi_dec *InstancePtr, UINTPTR BaseAddress) {
	XViterbi_dec_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XViterbi_dec_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XViterbi_dec_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XViterbi_dec_Config *XViterbi_dec_LookupConfig(u16 DeviceId) {
	XViterbi_dec_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XVITERBI_DEC_NUM_INSTANCES; Index++) {
		if (XViterbi_dec_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XViterbi_dec_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XViterbi_dec_Initialize(XViterbi_dec *InstancePtr, u16 DeviceId) {
	XViterbi_dec_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XViterbi_dec_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XViterbi_dec_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

