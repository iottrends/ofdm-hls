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
#include "xconv_enc.h"

extern XConv_enc_Config XConv_enc_ConfigTable[];

#ifdef SDT
XConv_enc_Config *XConv_enc_LookupConfig(UINTPTR BaseAddress) {
	XConv_enc_Config *ConfigPtr = NULL;

	int Index;

	for (Index = (u32)0x0; XConv_enc_ConfigTable[Index].Name != NULL; Index++) {
		if (!BaseAddress || XConv_enc_ConfigTable[Index].Ctrl_BaseAddress == BaseAddress) {
			ConfigPtr = &XConv_enc_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XConv_enc_Initialize(XConv_enc *InstancePtr, UINTPTR BaseAddress) {
	XConv_enc_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XConv_enc_LookupConfig(BaseAddress);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XConv_enc_CfgInitialize(InstancePtr, ConfigPtr);
}
#else
XConv_enc_Config *XConv_enc_LookupConfig(u16 DeviceId) {
	XConv_enc_Config *ConfigPtr = NULL;

	int Index;

	for (Index = 0; Index < XPAR_XCONV_ENC_NUM_INSTANCES; Index++) {
		if (XConv_enc_ConfigTable[Index].DeviceId == DeviceId) {
			ConfigPtr = &XConv_enc_ConfigTable[Index];
			break;
		}
	}

	return ConfigPtr;
}

int XConv_enc_Initialize(XConv_enc *InstancePtr, u16 DeviceId) {
	XConv_enc_Config *ConfigPtr;

	Xil_AssertNonvoid(InstancePtr != NULL);

	ConfigPtr = XConv_enc_LookupConfig(DeviceId);
	if (ConfigPtr == NULL) {
		InstancePtr->IsReady = 0;
		return (XST_DEVICE_NOT_FOUND);
	}

	return XConv_enc_CfgInitialize(InstancePtr, ConfigPtr);
}
#endif

#endif

