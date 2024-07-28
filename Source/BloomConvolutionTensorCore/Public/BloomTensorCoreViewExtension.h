
#pragma once


#include "SceneViewExtension.h"

class FBloomTensorCoreViewExtension : public FSceneViewExtensionBase
{
public:
	FBloomTensorCoreViewExtension(const FAutoRegister& AutoRegister);

	//~ ISceneViewExtension interface
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	
	// FScreenPassTexture AfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs);
	FScreenPassTexture AfterCopySsrInput_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs);

	void ResetConvolutionProperty(FConvolutionBloomSettings& ConvolutionSettings);
private:
	FRDGTextureRef ConvolvedKernel = nullptr;
	FRDGTextureRef KernelImg = nullptr;
	bool ViewExtensionIsActive;
	bool isKernelReset = true;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Convolution Method")
	FConvolutionBloomSettings Convolution;
	/*int Width;
	int Height;*/
	FRDGTextureRef ApplyBloomConvolutionTensorCore_RenderThread(FRDGBuilder& GraphBuilder, FRDGTextureRef SourceTexture);

protected:
	FScreenPassTexture ApplyBloomConvolutionTensorCore(FRDGBuilder& GraphBuilder, const FPostProcessMaterialInputs& InOutInputs);
};

