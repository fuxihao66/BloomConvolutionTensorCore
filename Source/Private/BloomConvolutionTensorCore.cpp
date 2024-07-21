#include "BloomConvolutionTensorCore.h"

#define LOCTEXT_NAMESPACE "FBloomConvolutionTensorCoreModule"

void FBloomConvolutionTensorCoreModule::StartupModule() 
{
    if (IConsoleVariable* CVarDebugCanvasVisible = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
    {
        CVarDebugCanvasVisible->Set(0);
    }
}

void FBloomConvolutionTensorCoreModule::ShutdownModule() 
{
    
}   
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBloomConvolutionTensorCoreModule, BloomConvolutionTensorCore)