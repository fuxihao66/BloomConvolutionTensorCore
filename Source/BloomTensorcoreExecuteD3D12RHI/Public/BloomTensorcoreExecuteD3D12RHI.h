#pragma once

#include "Modules/ModuleManager.h"
#include "BloomTensorcoreExecuteRHI.h"
#define MAX_DESCRIPTOR_COUNT 16


class FBloomTensorcoreExecuteD3D12RHIModule final : public IBloomTensorcoreExecuteRHIModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule();
	virtual void ShutdownModule();

	/** IBloomTensorcoreExecuteRHIModule implementation */
	virtual TUniquePtr<BloomTensorcoreExecuteRHI> CreateBloomTensorcoreExecuteRHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments);
};