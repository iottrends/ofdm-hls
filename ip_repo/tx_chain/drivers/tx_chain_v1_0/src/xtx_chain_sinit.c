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
#include "xtx_chain.h"

extern XTx_chain_Config XTx_chain_ConfigTable[];

#ifdef SDT
XTx_chain_Config *XTx_chain_LookupConfig(UINTPTR BaseAddress) {
	XTx_chain_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XTx_chain_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XTx_chain_ConfigTable[Index].Ctrl_BaseAddress == BaseAddress) {
			ConfigPtr = &XTx_chain_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XTx_chain_Initialize(XTx_chain *InstancePtr, UINTPTR BaseAddress) {
	XTx_chain_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XTx_chain_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XTx_chain_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XTx_chain_Config *XTx_chain_LookupConfig(u16 DeviceId) {
	XTx_chain_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XTX_CHAIN_NUM_INSTANCES; Index++) {
		if (XTx_chain_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XTx_chain_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XTx_chain_Initialize(XTx_chain *InstancePtr, u16 DeviceId) {
	XTx_chain_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XTx_chain_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XTx_chain_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

