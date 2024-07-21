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

void ABloomTensorCoreActor::BeginPlay(){
    BloomTensorCoreViewExtension = FSceneViewExtensions::NewExtension<FBloomTensorCoreViewExtension>();

}
bool ABloomTensorCoreActor::ShouldTickIfViewportsOnly() const{
    return true;
}

void ABloomTensorCoreActor::Tick(float DeltaTime){
    
}
