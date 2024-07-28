#include "BloomTensorCoreActor.h"

#include "RenderUtils.h"

ABloomTensorCoreActor::ABloomTensorCoreActor()
{
    
}

ABloomTensorCoreActor::~ABloomTensorCoreActor()
{
    
}

void ABloomTensorCoreActor::EndPlay(const EEndPlayReason::Type EndPlayReason) {

}
void ABloomTensorCoreActor::Tick( float DeltaSeconds ){

    if (!BloomTensorCoreViewExtension && isEnabled)
        BloomTensorCoreViewExtension = FSceneViewExtensions::NewExtension<FBloomTensorCoreViewExtension>();
        if (IConsoleVariable* CVarDebugCanvasVisible = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
        {
            CVarDebugCanvasVisible->Set(0);
        }
    else if (BloomTensorCoreViewExtension && !isEnabled){
        BloomTensorCoreViewExtension = nullptr;

        if (IConsoleVariable* CVarDebugCanvasVisible = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
        {
            CVarDebugCanvasVisible->Set(5);
        }
    }
}

void ABloomTensorCoreActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) {
    BloomTensorCoreViewExtension.ResetConvolutionProperty(Convolution);
}
void ABloomTensorCoreActor::BeginPlay(){

    // if (BloomConvolutionTexture == nullptr)
    // {
    //     GEngine->LoadDefaultBloomTexture();

    //     BloomConvolutionTexture = GEngine->DefaultBloomKernelTexture;
    // }
}
bool ABloomTensorCoreActor::ShouldTickIfViewportsOnly() const{
    return true;
}
