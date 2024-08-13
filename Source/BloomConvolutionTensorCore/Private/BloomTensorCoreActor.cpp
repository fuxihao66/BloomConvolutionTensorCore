#include "BloomTensorCoreActor.h"
#include "BloomConvolutionPass.h"
#include "RenderUtils.h"
#include "GPUFastFourierTransform.h"
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
	if (!isRegistered && isEnabled) {
		/*BloomTensorCoreViewExtension = FSceneViewExtensions::NewExtension<FBloomTensorCoreViewExtension>();

		BloomTensorCoreViewExtension->ResetConvolutionProperty(Convolution);
		if (IConsoleVariable* CVarDebugCanvasVisible = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
		{
			CVarDebugCanvasVisible->Set(0);
		}*/
		RegisterBloomFunc(DispatchManager::DispatchBloomConvTensorCore);
		isRegistered = true;
	}
	else if (isRegistered && !isEnabled){
		/*BloomTensorCoreViewExtension = nullptr;

		if (IConsoleVariable* CVarDebugCanvasVisible = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality")))
		{
			CVarDebugCanvasVisible->Set(5);
		}*/
		UnRegisterBloomFunc();
		isRegistered = false;
	}
}

void ABloomTensorCoreActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) {
	/*if (BloomTensorCoreViewExtension)
		BloomTensorCoreViewExtension->ResetConvolutionProperty(Convolution);*/
	/*if (isEnabled) {
		RegisterBloomFunc(DispatchManager::DispatchBloomConvTensorCore);
	}
	else {
		UnRegisterBloomFunc();
	}*/
}
void ABloomTensorCoreActor::BeginPlay(){

    Super::BeginPlay();
    RegisterActorTickFunctions(true);
    // if (BloomConvolutionTexture == nullptr)
    // {
    //     GEngine->LoadDefaultBloomTexture();

    //     BloomConvolutionTexture = GEngine->DefaultBloomKernelTexture;
    // }
}
bool ABloomTensorCoreActor::ShouldTickIfViewportsOnly() const{
    return true;
}
