#include "BloomConvolutionTensorCore.h"

#include "CoreMinimal.h"
#include "GeneralProjectSettings.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FBloomConvolutionTensorCoreModule"
DECLARE_LOG_CATEGORY_EXTERN(LogBloomConvolutionTensorCore, Verbose, All);
DEFINE_LOG_CATEGORY(LogBloomConvolutionTensorCore);
void FBloomConvolutionTensorCoreModule::StartupModule() 
{
    

    const FString RHIName = GDynamicRHI->GetName();
	const bool bIsDX12 = (RHIName == TEXT("D3D12"));

	if (bIsDX12)
	{
		const TCHAR* ODIRHIModuleName = TEXT("BloomTensorcoreExecuteD3D12RHI");

		const FString PluginBaseDir = IPluginManager::Get().FindPlugin(TEXT("BloomConvolutionTensorCore"))->GetBaseDir();

		FBloomTensorcoreExecuteRHICreateArguments Arguments;
		Arguments.PluginBaseDir = PluginBaseDir;
		Arguments.DynamicRHI = GDynamicRHI;

		Arguments.UnrealEngineVersion = FString::Printf(TEXT("%u.%u"), FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor());
		Arguments.UnrealProjectID = GetDefault<UGeneralProjectSettings>()->ProjectID.ToString();

		IBloomTensorcoreExecuteRHIModule* RHIModule = &FModuleManager::LoadModuleChecked<IBloomTensorcoreExecuteRHIModule>(ODIRHIModuleName);
		BloomTensorcoreExecuteRHIExtensions = RHIModule->CreateBloomTensorcoreExecuteRHI(Arguments);
	}
	else{
		UE_LOG(LogBloomConvolutionTensorCore, Log, TEXT("Plugin does not support %s RHI"), *RHIName);
	}
}
BloomTensorcoreExecuteRHI* FBloomConvolutionTensorCoreModule::GetBloomTensorcoreExecuteRHIRef(){
	return BloomTensorcoreExecuteRHIExtensions.Get();
}
void FBloomConvolutionTensorCoreModule::ShutdownModule() 
{
    BloomTensorcoreExecuteRHIExtensions.Reset();
}   
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBloomConvolutionTensorCoreModule, BloomConvolutionTensorCore)