#pragma once
#include "BloomTensorCoreExecuteRHI.h"

#include "CoreMinimal.h"

class BLOOMCONVOLUTIONTENSORCORE_API DispatchManager {
public:
	static FRDGTextureRef DispatchKernelConv(FRDGBuilder& GraphBuilder,
		FRDGTextureRef InputKernel, uint32 TextureWidth, uint32 TextureHeight, const FIntRect& ViewportSize);

	static FRDGTextureRef DispatchBloomConvTensorCore(FRDGBuilder& GraphBuilder,
		FRDGTextureRef ConvolvedKernel, FRDGTextureRef SourceTexture,
		uint32 TextureWidth, uint32 TextureHeight, const FIntRect& ViewportSize);
//private:
//	static BloomTensorcoreExecuteRHI* m_RHIExtensions;
};

