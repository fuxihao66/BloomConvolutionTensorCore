
#include "BloomTensorCoreViewExtension.h"

#include "Modules/ModuleManager.h"
#include "PostProcess/PostProcessMaterial.h"
#include "PostProcess/SceneRenderTargets.h"

// #include "MyNeuralNetwork.h"

#include "Math/PackedVector.h"

DEFINE_LOG_CATEGORY_STATIC(LogBloomTensorCore, Log, All);
// void RenderMyTest(FRHICommandList &RHICmdList, ERHIFeatureLevel::Type FeatureLevel, const FLinearColor &Color);

//------------------------------------------------------------------------------
FBloomTensorCoreViewExtension::FBloomTensorCoreViewExtension(const FAutoRegister &AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
	ViewExtensionIsActive = GDynamicRHI->GetName() == FString(TEXT("D3D12"));
}


//------------------------------------------------------------------------------
bool FBloomTensorCoreViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext &Context) const
{
	return ViewExtensionIsActive;
}


FRDGTextureRef FBloomTensorCoreViewExtension::ApplyBloomConvolutionTensorCore_RenderThread(
	FRDGBuilder &GraphBuilder,
	FRDGTextureRef SourceTexture)
{
	// if (SourceTexture == nullptr)
	// {
	// 	UE_LOG(LogBloomTensorCore, Warning, TEXT("Skipping null texture"));
	// 	return SourceTexture;
	// }

	// uint32 TextureWidth = SourceTexture->Desc.Extent.X;
	// uint32 TextureHeight = SourceTexture->Desc.Extent.Y;
	
	// uint32 BufferWidth = 224;
	// uint32 BufferHeight = 224;

	// auto SourceBuffer = DispatchTexture2Tensor(GraphBuilder, TextureWidth, TextureHeight, BufferWidth, BufferHeight, SourceTexture);
	// FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint16_t), BufferWidth * BufferHeight * 3); // half
	// auto OutputBuffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("OutputTensor"));

	// myNetwork->CreateModelAndUploadData(GraphBuilder);
	// myNetwork->ExecuteInference(GraphBuilder, 
	// 	std::map<std::string, FRDGBufferRef>{ {"input1", SourceBuffer} }, OutputBuffer);
	// return DispatchTensor2Texture(GraphBuilder, TextureWidth, TextureHeight, BufferWidth, BufferHeight, OutputBuffer, SourceTexture->Desc);
    return SourceTexture;
}

//------------------------------------------------------------------------------
void FBloomTensorCoreViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, FAfterPassCallbackDelegateArray &InOutPassCallbacks, bool bIsPassEnabled)
{
	
	if (!bIsPassEnabled)
	{
		return;
	}

	if (PassId == EPostProcessingPass::SSRInput) {
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FBloomTensorCoreViewExtension::AfterCopySsrInput_RenderThread));
	}
}

//------------------------------------------------------------------------------
FScreenPassTexture FBloomTensorCoreViewExtension::ApplyBloomConvolutionTensorCore(FRDGBuilder &GraphBuilder, const FPostProcessMaterialInputs &InOutInputs)
{
	FRDGTextureRef InputTexture = nullptr;
	FScreenPassTexture ReturnTexture;

	if (InOutInputs.OverrideOutput.IsValid())
	{
		InputTexture = InOutInputs.OverrideOutput.Texture;
		ReturnTexture = InOutInputs.OverrideOutput;
	}
	else
	{
		InputTexture = InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor].Texture;
		
		ReturnTexture = const_cast<FScreenPassTexture &>(InOutInputs.Textures[(uint32)EPostProcessMaterialInput::SceneColor]);
	}
	
	ReturnTexture.Texture = ApplyBloomConvolutionTensorCore_RenderThread(GraphBuilder, InputTexture);

	return ReturnTexture;
}

//------------------------------------------------------------------------------
FScreenPassTexture FBloomTensorCoreViewExtension::AfterCopySsrInput_RenderThread(FRDGBuilder &GraphBuilder, const FSceneView &View, const FPostProcessMaterialInputs &InOutInputs)
{
	RDG_EVENT_SCOPE(GraphBuilder, "Bloom_Convolution_TensorCore");

	return ApplyBloomConvolutionTensorCore(GraphBuilder, InOutInputs);
}


#undef LOCTEXT_NAMESPACE
