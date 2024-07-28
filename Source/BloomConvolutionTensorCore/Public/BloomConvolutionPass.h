#pragma once
#include "CoreMinimal.h"

FRDGTextureRef DispatchKernelConv(FRDGBuilder& GraphBuilder,
	FRDGTextureRef InputKernel, uint32 TextureWidth, uint32 TextureHeight);

FRDGTextureRef DispatchBloomConvTensorCore(FRDGBuilder& GraphBuilder,
	FRDGTextureRef ConvolvedKernel, FRDGTextureRef SourceTexture, const FRDGTextureRef& OutputTexture);