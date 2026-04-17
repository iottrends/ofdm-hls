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
#include "xofdm_mac.h"

extern XOfdm_mac_Config XOfdm_mac_ConfigTable[];

#ifdef SDT
XOfdm_mac_Config *XOfdm_mac_LookupConfig(UINTPTR BaseAddress) {
	XOfdm_mac_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XOfdm_mac_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XOfdm_mac_ConfigTable[Index].Control_BaseAddress == BaseAddress) {
			ConfigPtr = &XOfdm_mac_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XOfdm_mac_Initialize(XOfdm_mac *InstancePtr, UINTPTR BaseAddress) {
	XOfdm_mac_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XOfdm_mac_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XOfdm_mac_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XOfdm_mac_Config *XOfdm_mac_LookupConfig(u16 DeviceId) {
	XOfdm_mac_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XOFDM_MAC_NUM_INSTANCES; Index++) {
		if (XOfdm_mac_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XOfdm_mac_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XOfdm_mac_Initialize(XOfdm_mac *InstancePtr, u16 DeviceId) {
	XOfdm_mac_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XOfdm_mac_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XOfdm_mac_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

