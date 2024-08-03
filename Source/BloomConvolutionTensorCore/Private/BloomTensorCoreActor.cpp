#include "BloomTensorCoreActor.h"

#include "RenderUtils.h"

ABloomTensorCoreActor::ABloomTensorCoreActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

ABloomTensorCoreActor::~ABloomTensorCoreActor()
{
    
}

void ABloomTensorCoreActor::EndPlay(const EEndPlayReason::Type EndPlayReason) {
    Super::EndPlay(EndPlayReason);
}

void ABloomTensorCoreActor::Tick( float DeltaTime){
    AActor::Tick(DeltaTime);
    if (!BloomTensorCoreViewExtension && isEnabled) {
        BloomTensorCoreViewExtension = FSceneViewExtensions::NewExtension<FBloomTensorCoreViewExtension>();
        
        BloomTensorCoreViewExtension->ResetConvolutionProperty(Convolution);
        if (IConsoleVariable* CVarDebugCanvasVisible = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
        {
            CVarDebugCanvasVisible->Set(0);
        }
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
    if (BloomTensorCoreViewExtension)
        BloomTensorCoreViewExtension->ResetConvolutionProperty(Convolution);
}
void ABloomTensorCoreActor::BeginPlay(){

    Super::BeginPlay();
    //RegisterActorTickFunctions(true);
    // if (BloomConvolutionTexture == nullptr)
    // {
    //     GEngine->LoadDefaultBloomTexture();

    //     BloomConvolutionTexture = GEngine->DefaultBloomKernelTexture;
    // }
}
bool ABloomTensorCoreActor::ShouldTickIfViewportsOnly() const{
    return true;
}
