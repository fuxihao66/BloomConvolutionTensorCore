#pragma once

#include "Modules/ModuleManager.h"

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "Runtime/Launch/Resources/Version.h"

#include <map>

typedef enum BloomTensorcore_Result
{
    Result_Success = 0x0,

    Result_Fail = 0x1,
} BloomTensorcore_Result;

struct FBloomTensorcoreExecuteRHICreateArguments
{
	FString PluginBaseDir;
	FDynamicRHI* DynamicRHI = nullptr;
	FString UnrealEngineVersion;
	FString UnrealProjectID;
};


struct FBloomRHIExecuteArguments
{
	FRHITexture2D* SrcTexture = nullptr; 
	FRHITexture2D* KernelTexture = nullptr; 
	FRHITexture2D* Intermediate0 = nullptr; 
	FRHITexture2D* Intermediate1 = nullptr; 
	FRHIUnorderedAccessView* OutputTexture = nullptr; 
	FRHIUnorderedAccessView* Intermediate0_UAV = nullptr; 
	FRHIUnorderedAccessView* Intermediate1_UAV = nullptr; 

	uint32 GPUNode = 0;
	uint32 GPUVisibility = 0;
};
class BLOOMTENSORCOREEXECUTERHI_API BloomTensorcoreExecuteRHI
{
public:
	BloomTensorcoreExecuteRHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments);
	virtual BloomTensorcore_Result ExecuteBloomTensorcore(FRHICommandList& CmdList, const FBloomRHIExecuteArguments& InArguments) { return BloomTensorcore_Result::Result_Fail; }
	virtual ~BloomTensorcoreExecuteRHI();
private:

	FDynamicRHI* DynamicRHI = nullptr;
};

class IBloomTensorcoreExecuteRHIModule : public IModuleInterface{
public:
	virtual TUniquePtr<BloomTensorcoreExecuteRHI> CreateBloomTensorcoreExecuteRHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments) = 0;
};

class FBloomTensorcoreExecuteRHIModule final : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule();
	virtual void ShutdownModule();
};
