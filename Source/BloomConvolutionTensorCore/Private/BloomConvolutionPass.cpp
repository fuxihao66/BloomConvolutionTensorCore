#include "BloomConvolutionPass.h"

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
FRDGBufferRef DispatchKernelConv(FRDGBuilder& GraphBuilder,
	FRDGTextureRef InputKernel, uint32 TextureWidth, uint32 TextureHeight) {

	
}

FRDGTextureRef DispatchBloomConvTensorCore(FRDGBuilder& GraphBuilder,
	FRDGTextureRef ConvolvedKernel, FRDGTextureRef SourceTexture){

    FRDGTextureDesc TexDesc = FRDGTextureDesc::CreateTextureDesc(); 
	auto OutputTexture = GraphBuilder.CreateTexture2D(TexDesc, TEXT("Bloom Output"));

    FRDGTextureDesc TexDesc = FRDGTextureDesc::CreateTextureDesc(); 
	auto IntermediateTexture0 = GraphBuilder.CreateTexture2D(TexDesc, TEXT("Bloom Intermediate0"));

    FRDGTextureDesc TexDesc = FRDGTextureDesc::CreateTextureDesc(); 
	auto IntermediateTexture1 = GraphBuilder.CreateTexture2D(TexDesc, TEXT("Bloom Intermediate1"));
    // init params


    FBloomConvolutionTensorCoreModule* LocalModuleRef = &FModuleManager::LoadModuleChecked<FBloomConvolutionTensorCoreModule>("BloomConvolutionTensorCore"); // load module
	m_RHIExtensions = LocalModuleRef->GetBloomTensorcoreExecuteRHIRef();

    // 
    FTensorcoreBloomParameters* PassParameters = GraphBuilder.AllocParameters<FTensorcoreBloomParameters>();
    PassParameters->SrcTexture = SourceTexture;
    PassParameters->KernelSpectrum = ConvolvedKernel;
    PassParameters->DstTexture = GraphBuilder.CreateUAV(OutputTexture);
    PassParameters->Intermediate0 = GraphBuilder.CreateUAV(IntermediateTexture0);
    PassParameters->Intermediate1 = GraphBuilder.CreateUAV(IntermediateTexture1);
	
    GraphBuilder.AddPass(
        RDG_EVENT_NAME("Execute Tensorcore Bloom FFT"),
        PassParameters,
        ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
        [LocalRHIExtensions = m_RHIExtensions, PassParameters, SourceTexture, ConvolvedKernel, 
            OutputTexture, IntermediateTexture0, IntermediateTexture1](FRHICommandListImmediate& RHICmdList)
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

        RHICmdList.EnqueueLambda(
            [LocalRHIExtensions, Arguments](FRHICommandListImmediate& Cmd) mutable
        {
            Arguments.GPUNode = Cmd.GetGPUMask().ToIndex();
            Arguments.GPUVisibility = Cmd.GetGPUMask().GetNative();

			LocalRHIExtensions->ExecuteBloomTensorcore(Cmd, Arguments);
        });
    });

	return true;
            
}