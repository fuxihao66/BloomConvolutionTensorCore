#pragma once
#include "BloomTensorCoreExecuteRHI.h"

#include "CoreMinimal.h"

class BLOOMCONVOLUTIONTENSORCORE_API DispatchManager {
public:
	static void DispatchBloomConvTensorCore(FRDGBuilder& GraphBuilder,
		FRDGTextureRef ConvolvedKernel, FRDGTextureRef SourceTexture, FRDGTextureRef DestTexture,
		FRDGBufferRef PostFilterPara, FVector3f BrightGain,
		uint32 TextureWidth, uint32 TextureHeight, const FIntRect& ViewportSize);
//private:
//	static BloomTensorcoreExecuteRHI* m_RHIExtensions;
};

