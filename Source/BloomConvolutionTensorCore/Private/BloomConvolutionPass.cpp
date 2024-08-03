#include "BloomConvolutionPass.h"

#include "BloomConvolutionTensorCore.h"
//#include "SceneTextureParameters.h"
#include "PixelShaderUtils.h"
#include "RawIndexBuffer.h"
#include "SceneUtils.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "MeshMaterialShader.h"
//#include "PostProcess/PostProcessing.h"

BEGIN_SHADER_PARAMETER_STRUCT(FTensorcoreBloomParameters, )
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SrcTexture)
SHADER_PARAMETER_RDG_TEXTURE(Texture2D, KernelSpectrum)
SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DstTexture)
SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Intermediate0)
SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, Intermediate1)
// RDG_TEXTURE_ACCESS(SceneColorOutput, ERHIAccess::UAVCompute)

END_SHADER_PARAMETER_STRUCT()

// COPY FROM UE
FRDGTextureRef DispatchManager::DispatchKernelConv(FRDGBuilder& GraphBuilder,
	FRDGTextureRef InputKernel, uint32 TextureWidth, uint32 TextureHeight, const FIntRect& ViewportSize) {

    return InputKernel;
}

FRDGTextureRef DispatchManager::DispatchBloomConvTensorCore(FRDGBuilder& GraphBuilder,
	FRDGTextureRef ConvolvedKernel, FRDGTextureRef SourceTexture, uint32 TextureWidth, uint32 TextureHeight, const FIntRect& ViewportSize){

    FRDGTextureDesc OutputTextureDesc = FRDGTextureDesc::Create2D({ (int)TextureWidth ,(int)TextureHeight }, PF_A16B16G16R16, FClearValueBinding::Black, TexCreate_UAV);
	auto OutputTexture = GraphBuilder.CreateTexture(OutputTextureDesc, TEXT("Bloom Output"));

    uint32 PaddedWidth = 1 << FMath::CeilToInt(FMath::Log2((float)TextureWidth));

    FRDGTextureDesc IntermediateTexture0Desc = FRDGTextureDesc::Create2D({ (int)PaddedWidth + 2 ,(int)TextureHeight }, PF_A32B32G32R32F, FClearValueBinding::Black, TexCreate_UAV);
	auto IntermediateTexture0 = GraphBuilder.CreateTexture(IntermediateTexture0Desc, TEXT("Bloom Intermediate0"));

    FRDGTextureDesc IntermediateTexture1Desc = FRDGTextureDesc::Create2D({ (int)PaddedWidth + 2 ,(int)TextureHeight }, PF_A32B32G32R32F, FClearValueBinding::Black, TexCreate_UAV);
	auto IntermediateTexture1 = GraphBuilder.CreateTexture(IntermediateTexture1Desc, TEXT("Bloom Intermediate1"));
    // init params


    FBloomConvolutionTensorCoreModule* LocalModuleRef = &FModuleManager::LoadModuleChecked<FBloomConvolutionTensorCoreModule>("BloomConvolutionTensorCore"); // load module
    BloomTensorcoreExecuteRHI* LocalRHIExtensions = LocalModuleRef->GetBloomTensorcoreExecuteRHIRef();

    // 
    FTensorcoreBloomParameters* PassParameters = GraphBuilder.AllocParameters<FTensorcoreBloomParameters>();
    PassParameters->SrcTexture = SourceTexture;
    PassParameters->KernelSpectrum = ConvolvedKernel;
    PassParameters->DstTexture = GraphBuilder.CreateUAV(OutputTexture);
    PassParameters->Intermediate0 = GraphBuilder.CreateUAV(IntermediateTexture0);
    PassParameters->Intermediate1 = GraphBuilder.CreateUAV(IntermediateTexture1);
	
    //m_RHIExtensions;

    GraphBuilder.AddPass(
        RDG_EVENT_NAME("Execute Tensorcore Bloom FFT"),
        PassParameters,
        ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::NeverCull | ERDGPassFlags::SkipRenderPass,
        [LocalRHIExtensions, PassParameters, SourceTexture, ConvolvedKernel, 
            OutputTexture, IntermediateTexture0, IntermediateTexture1, ViewportSize](FRHICommandListImmediate& RHICmdList)
    {
		PassParameters->SrcTexture->MarkResourceAsUsed();
		PassParameters->KernelSpectrum->MarkResourceAsUsed();
		PassParameters->DstTexture->MarkResourceAsUsed();
		PassParameters->Intermediate0->MarkResourceAsUsed();
		PassParameters->Intermediate1->MarkResourceAsUsed();

        FBloomRHIExecuteArguments Arguments;
        // Arguments.SrcTexture = PassParameters->SrcTexture->GetRHI();
        Arguments.SrcTexture = PassParameters->SrcTexture->GetRHI();
        Arguments.KernelTexture = PassParameters->KernelSpectrum->GetRHI();
        Arguments.OutputTexture = PassParameters->DstTexture->GetRHI();
        Arguments.Intermediate0 = IntermediateTexture0->GetRHI();
        Arguments.Intermediate0_UAV = PassParameters->Intermediate0->GetRHI();
        Arguments.Intermediate1 = IntermediateTexture1->GetRHI();
        Arguments.Intermediate1_UAV = PassParameters->Intermediate1->GetRHI();
        Arguments.ViewportWidth = ViewportSize.Width();
        Arguments.ViewportHeight = ViewportSize.Height();

        RHICmdList.EnqueueLambda(
            [LocalRHIExtensions, Arguments](FRHICommandListImmediate& Cmd) mutable
        {
            Arguments.GPUNode = Cmd.GetGPUMask().ToIndex();
            Arguments.GPUVisibility = Cmd.GetGPUMask().GetNative();

			LocalRHIExtensions->ExecuteBloomTensorcore(Cmd, Arguments);
        });
    });

	return OutputTexture;
            
}