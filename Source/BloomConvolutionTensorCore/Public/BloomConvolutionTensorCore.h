#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "BloomTensorCoreExecuteRHI.h"
class FBloomConvolutionTensorCoreModule : public IModuleInterface
{
private:
    TUniquePtr<BloomTensorcoreExecuteRHI> BloomTensorcoreExecuteRHIExtensions;
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    BloomTensorcoreExecuteRHI* GetBloomTensorcoreExecuteRHIRef();

};