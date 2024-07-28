#include "BloomTensorcoreExecuteRHI.h"

#include "Misc/Paths.h"
#include "GenericPlatform/GenericPlatformFile.h"

DEFINE_LOG_CATEGORY_STATIC(LogBloomTensorcoreExecuteRHI, Log, All);

#define LOCTEXT_NAMESPACE "BloomTensorcoreExecuteRHI"

BloomTensorcoreExecuteRHI::BloomTensorcoreExecuteRHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments)
	: DynamicRHI(Arguments.DynamicRHI)
{
	
	UE_LOG(LogBloomTensorcoreExecuteRHI, Log, TEXT("BloomTensorcoreExecuteRHI DEBUG INFO"));
}

BloomTensorcoreExecuteRHI::~BloomTensorcoreExecuteRHI()
{
}

IMPLEMENT_MODULE(FBloomTensorcoreExecuteRHIModule, BloomTensorcoreExecuteRHI)

#undef LOCTEXT_NAMESPACE


void FBloomTensorcoreExecuteRHIModule::StartupModule()
{
}

void FBloomTensorcoreExecuteRHIModule::ShutdownModule()
{
}
