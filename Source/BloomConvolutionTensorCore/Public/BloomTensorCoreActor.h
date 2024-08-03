#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SceneViewExtension.h"

//#include "Windows/AllowWindowsPlatformTypes.h"
#include "BloomTensorCoreViewExtension.h"

#include "BloomTensorCoreActor.generated.h"

UCLASS()
class BLOOMCONVOLUTIONTENSORCORE_API ABloomTensorCoreActor : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(Interp, BlueprintReadWrite, Category = "Convolution Method")
	FConvolutionBloomSettings Convolution;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Convolution")
	bool isEnabled;
public:
    ABloomTensorCoreActor();
    ~ABloomTensorCoreActor(); 

protected:

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool ShouldTickIfViewportsOnly() const override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:
    virtual void Tick(float DeltaTime) override;

public:

    TSharedPtr<FBloomTensorCoreViewExtension, ESPMode::ThreadSafe> BloomTensorCoreViewExtension;

};