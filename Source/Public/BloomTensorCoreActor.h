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
    ABloomTensorCoreActor();
    ~ABloomTensorCoreActor(); 

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual bool ShouldTickIfViewportsOnly() const override;

public:
    virtual void Tick(float DeltaTime) override;

public:
    /*UPROPERTY(Category = "Bloom Tensor Core", EditAnywhere, BlueprintReadWrite)
    UTexture2D KernelImage;*/

    TSharedPtr<FBloomTensorCoreViewExtension, ESPMode::ThreadSafe> BloomTensorCoreViewExtension;

};