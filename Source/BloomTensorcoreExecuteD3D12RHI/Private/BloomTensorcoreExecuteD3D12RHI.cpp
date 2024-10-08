#include "BloomTensorcoreExecuteD3D12RHI.h"
//#include "FFT_Debug.hlsl.h"
#include "FFT_TwoForOne_2048.hlsl.h"
#include "FFT_TwoForOne_1024.hlsl.h"
#include "FFT_ConvWithTexture_2048.hlsl.h"
#include "FFT_ConvWithTexture_1024.hlsl.h"
#include "Float16Compressor.h"
#include "D3D12RHIPrivate.h"
#include "D3D12Util.h"
#include "D3D12State.h"
#include "D3D12Texture.h"
#include "D3D12Resources.h"
#include "D3D12Viewport.h"
#include "D3D12ConstantBuffer.h"
#include "D3D12CommandContext.h"
#include "RHIValidationCommon.h"
#include "ID3D12DynamicRHI.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Interfaces/IPluginManager.h"

#include <cassert>
#include <fstream>

DEFINE_LOG_CATEGORY_STATIC(LogBloomTensorcoreExecuteD3D12RHI, Log, All);

#define LOCTEXT_NAMESPACE "FBloomTensorcoreExecuteD3D12RHIModule"
#define _Debug 0



class DescriptorHeapWrapper 
{
private:
	void Create(
		ID3D12Device* pDevice,
		const D3D12_DESCRIPTOR_HEAP_DESC* pDesc)
	{
		assert(pDesc != nullptr);

		m_desc = *pDesc;
		m_increment = pDevice->GetDescriptorHandleIncrementSize(pDesc->Type);

		if (pDesc->NumDescriptors == 0)
		{
			m_pHeap.Reset();
			m_hCPU.ptr = 0;
			m_hGPU.ptr = 0;
		}
		else
		{
			pDevice->CreateDescriptorHeap(
				pDesc,
				IID_PPV_ARGS(m_pHeap.ReleaseAndGetAddressOf()));

			m_hCPU = m_pHeap->GetCPUDescriptorHandleForHeapStart();

			if (pDesc->Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
				m_hGPU = m_pHeap->GetGPUDescriptorHandleForHeapStart();

		}
	}

public:
	DescriptorHeapWrapper(
		_In_ ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_TYPE type,
		D3D12_DESCRIPTOR_HEAP_FLAGS flags,
		size_t count) :
		m_desc{},
		m_hCPU{},
		m_hGPU{},
		m_increment(0)
	{
		if (count > UINT32_MAX)
			throw std::exception("Too many descriptors");

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Flags = flags;
		desc.NumDescriptors = static_cast<UINT>(count);
		desc.Type = type;
		Create(device, &desc);
	}
	ID3D12DescriptorHeap* Heap() {
		return m_pHeap.Get();
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(_In_ size_t index) const
	{
		assert(m_pHeap != nullptr);
		if (index >= m_desc.NumDescriptors)
		{
			throw std::out_of_range("D3DX12_CPU_DESCRIPTOR_HANDLE");
		}

		D3D12_CPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = static_cast<SIZE_T>(m_hCPU.ptr + UINT64(index) * UINT64(m_increment));
		return handle;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(_In_ size_t index) const
	{
		assert(m_pHeap != nullptr);
		if (index >= m_desc.NumDescriptors)
		{
			throw std::out_of_range("D3DX12_GPU_DESCRIPTOR_HANDLE");
		}
		assert(m_desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

		D3D12_GPU_DESCRIPTOR_HANDLE handle;
		handle.ptr = m_hGPU.ptr + UINT64(index) * UINT64(m_increment);
		return handle;
	}
private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    m_pHeap;
	D3D12_DESCRIPTOR_HEAP_DESC                      m_desc;
	D3D12_CPU_DESCRIPTOR_HANDLE                     m_hCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE                     m_hGPU;
	uint32_t                                        m_increment;
};
class FBloomTensorcoreExecuteD3D12RHI final : public BloomTensorcoreExecuteRHI
{
private:
	std::unique_ptr<DescriptorHeapWrapper>        	m_DescriptorHeap[3];
	std::unique_ptr<DescriptorHeapWrapper>        	m_CPUDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12RootSignature>		m_SM66RootSignature;
	//Microsoft::WRL::ComPtr<ID3D12PipelineState>		m_DebugTwoForOnePSO;

	Microsoft::WRL::ComPtr<ID3D12PipelineState>		m_TwoForOnePSO[2];
	Microsoft::WRL::ComPtr<ID3D12PipelineState>		m_ConvolveWithTexturePSO[2];
	//Microsoft::WRL::ComPtr<ID3D12PipelineState>		m_InverseTwoForOnePSO[2];
	Microsoft::WRL::ComPtr<ID3D12Resource>          m_Intermediate0;
	Microsoft::WRL::ComPtr<ID3D12Resource>          m_Intermediate1;

	Microsoft::WRL::ComPtr<ID3D12Resource>          m_FBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource>          m_FBuffer_Upload;
	Microsoft::WRL::ComPtr<ID3D12Resource>          m_FBuffer_Inverse;
	Microsoft::WRL::ComPtr<ID3D12Resource>          m_FBuffer_Inverse_Upload;
	uint16_t										FBufferData_1024[256 * 4];
	uint16_t										FBufferData_Inverse_1024[256 * 4];
	uint16_t										FBufferData_2048[256 * 4];
	uint16_t										FBufferData_Inverse_2048[256 * 4];
	UINT                                            m_currentDescriptorTopIndex;
public:
	FBloomTensorcoreExecuteD3D12RHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments);
	virtual BloomTensorcore_Result ExecuteBloomTensorcore(FRHICommandList& CmdList, const FBloomRHIExecuteArguments& InArguments) final;
	virtual ~FBloomTensorcoreExecuteD3D12RHI();
private:
	BloomTensorcore_Result CreateD3D12Resources();
	UINT FrameIndex = 0;
	bool initialized = false;
	UINT cachedScanLineLenght = 0;
	ID3D12Device* m_d3dDevice = nullptr;
	ID3D12DynamicRHI* D3D12RHI = nullptr;
};


FBloomTensorcoreExecuteD3D12RHI::FBloomTensorcoreExecuteD3D12RHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments)
	: BloomTensorcoreExecuteRHI(Arguments)
	, D3D12RHI(CastDynamicRHI<ID3D12DynamicRHI>(Arguments.DynamicRHI))
{
	//ID3D12Device* Direct3DDevice = D3D12RHI->GetAdapter().GetD3DDevice();
	ID3D12Device* Direct3DDevice = D3D12RHI->RHIGetDevice(0);

	ensure(D3D12RHI);
	ensure(Direct3DDevice);

	// const FString NGXLogDir = GetNGXLogDirectory();
	// IPlatformFile::GetPlatformPhysical().CreateDirectoryTree(*NGXLogDir);
	m_d3dDevice = Direct3DDevice;
	BloomTensorcore_Result ResultInit = CreateD3D12Resources();
	UE_LOG(LogBloomTensorcoreExecuteD3D12RHI, Log, TEXT("BloomTensorcoreExecute_D3D12_Init"));
	
}

FBloomTensorcoreExecuteD3D12RHI::~FBloomTensorcoreExecuteD3D12RHI()
{
	UE_LOG(LogBloomTensorcoreExecuteD3D12RHI, Log, TEXT("%s Enter"), ANSI_TO_TCHAR(__FUNCTION__));
	
	UE_LOG(LogBloomTensorcoreExecuteD3D12RHI, Log, TEXT("%s Leave"), ANSI_TO_TCHAR(__FUNCTION__));
}
namespace DescriptorOffsetMapping {
	enum DescriptorOffset {
		SrcTexture,
		KernelTexture,
		Intermediate0,
		Intermediate1,
		OutputUav,
		Intermediate0Uav,
		Intermediate1Uav,
		PostFilterPara,
		FBuffer,
		FBufferInverse,
		DescriptorMax
	};
}

BloomTensorcore_Result FBloomTensorcoreExecuteD3D12RHI::CreateD3D12Resources(){

	FString BaseDir = IPluginManager::Get().FindPlugin("BloomConvolutionTensorCore")->GetBaseDir();
	FString FMatrixDir = FPaths::Combine(*BaseDir, TEXT("Content/"));

	// initialize once
	for (int i = 0; i < 3; i++)
		m_DescriptorHeap[i] = std::make_unique<DescriptorHeapWrapper>(
			m_d3dDevice,
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			MAX_DESCRIPTOR_COUNT);

	m_CPUDescriptorHeap = std::make_unique<DescriptorHeapWrapper>(
		m_d3dDevice,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		MAX_DESCRIPTOR_COUNT);

	m_currentDescriptorTopIndex = 0;
	

	// root signature
	{
		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

		// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

		CD3DX12_ROOT_PARAMETER1 rootParameters[1];

		if (FAILED(m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}

		rootParameters[0].InitAsConstants(32, 0, 0);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		FAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
		FAILED(m_d3dDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_SM66RootSignature)));
	}
	//{
	//	D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
	//	PSODesc.CS = { g_DebugShader_CS, sizeof(g_DebugShader_CS) };// include by header
	//	PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	//	PSODesc.NodeMask = 0;
	//	PSODesc.pRootSignature = m_SM66RootSignature.Get();
	//	FAILED(m_d3dDevice->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_DebugTwoForOnePSO)));
	//}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
		PSODesc.CS = { g_TwoForOneShader_1024_CS, sizeof(g_TwoForOneShader_1024_CS) };// include by header
		PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		PSODesc.NodeMask = 0;
		PSODesc.pRootSignature = m_SM66RootSignature.Get();
		FAILED(m_d3dDevice->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_TwoForOnePSO[0])));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
		PSODesc.CS = { g_TwoForOneShader_2048_CS, sizeof(g_TwoForOneShader_2048_CS) };// include by header
		PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		PSODesc.NodeMask = 0;
		PSODesc.pRootSignature = m_SM66RootSignature.Get();
		FAILED(m_d3dDevice->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_TwoForOnePSO[1])));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
		PSODesc.CS = { g_ConvWithTextureShader_1024_CS, sizeof(g_ConvWithTextureShader_1024_CS) };// include by header
		PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		PSODesc.NodeMask = 0;
		PSODesc.pRootSignature = m_SM66RootSignature.Get();
		FAILED(m_d3dDevice->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_ConvolveWithTexturePSO[0])));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
		PSODesc.CS = { g_ConvWithTextureShader_2048_CS, sizeof(g_ConvWithTextureShader_2048_CS) };// include by header
		PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		PSODesc.NodeMask = 0;
		PSODesc.pRootSignature = m_SM66RootSignature.Get();
		FAILED(m_d3dDevice->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_ConvolveWithTexturePSO[1])));
	}
	{
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 2050, 2050, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		auto DefaultProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		m_d3dDevice->CreateCommittedResource(&DefaultProperty,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(m_Intermediate0.ReleaseAndGetAddressOf()));
		m_d3dDevice->CreateCommittedResource(&DefaultProperty,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			nullptr,
			IID_PPV_ARGS(m_Intermediate1.ReleaseAndGetAddressOf()));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.MostDetailedMip = 0;

		m_d3dDevice->CreateShaderResourceView(m_Intermediate0.Get(), &srvDesc, m_CPUDescriptorHeap->GetCpuHandle(2));
		m_d3dDevice->CreateShaderResourceView(m_Intermediate1.Get(), &srvDesc, m_CPUDescriptorHeap->GetCpuHandle(3));
		
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		uavDesc.Texture2D.MipSlice = 0;

		m_d3dDevice->CreateUnorderedAccessView(m_Intermediate0.Get(), nullptr, &uavDesc, m_CPUDescriptorHeap->GetCpuHandle(4));
		m_d3dDevice->CreateUnorderedAccessView(m_Intermediate1.Get(), nullptr, &uavDesc, m_CPUDescriptorHeap->GetCpuHandle(5));

	}
	{

		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(256 * /*real & imag*/2 * /*2 merge step*/2 * sizeof(uint16_t), D3D12_RESOURCE_FLAG_NONE);

		auto DefaultProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto UploadtProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Buffer.NumElements = resourceDesc.Width / 4;
		srvDesc.Buffer.StructureByteStride = 0;
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

		m_d3dDevice->CreateCommittedResource(&DefaultProperty,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(m_FBuffer.ReleaseAndGetAddressOf()));

		m_d3dDevice->CreateCommittedResource(&UploadtProperty,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_FBuffer_Upload.ReleaseAndGetAddressOf()));
		
		m_d3dDevice->CreateShaderResourceView(m_FBuffer.Get(), &srvDesc, m_CPUDescriptorHeap->GetCpuHandle(0));

		m_d3dDevice->CreateCommittedResource(&DefaultProperty,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(m_FBuffer_Inverse.ReleaseAndGetAddressOf()));

		m_d3dDevice->CreateCommittedResource(&UploadtProperty,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_FBuffer_Inverse_Upload.ReleaseAndGetAddressOf()));

		m_d3dDevice->CreateShaderResourceView(m_FBuffer_Inverse.Get(), &srvDesc, m_CPUDescriptorHeap->GetCpuHandle(1));

		{
			FString FMatrixPath = FPaths::Combine(*FMatrixDir, TEXT("f_1024.txt"));
			wchar_t* MyWCharPtr = (wchar_t*)*FMatrixPath;
			std::ifstream myfile;
			myfile.open(MyWCharPtr);
			myfile.read(reinterpret_cast<char*>(FBufferData_1024), std::streamsize(256 * 4 * sizeof(uint16_t)));
			myfile.close();
		}
		{
			FString FMatrixPath = FPaths::Combine(*FMatrixDir, TEXT("f_inv_1024.txt"));
			wchar_t* MyWCharPtr = (wchar_t*)*FMatrixPath;
			std::ifstream myfile;
			myfile.open(MyWCharPtr);
			myfile.read(reinterpret_cast<char*>(FBufferData_Inverse_1024), std::streamsize(256 * 4 * sizeof(uint16_t)));
			myfile.close();
		}
		{
			FString FMatrixPath = FPaths::Combine(*FMatrixDir, TEXT("f_2048.txt"));
			wchar_t* MyWCharPtr = (wchar_t*)*FMatrixPath;
			std::ifstream myfile;
			myfile.open(MyWCharPtr);
			myfile.read(reinterpret_cast<char*>(FBufferData_2048), std::streamsize(256 * 4 * sizeof(uint16_t)));
			myfile.close();
		}
		{
			FString FMatrixPath = FPaths::Combine(*FMatrixDir, TEXT("f_inv_2048.txt"));
			wchar_t* MyWCharPtr = (wchar_t*)*FMatrixPath;
			std::ifstream myfile;
			myfile.open(MyWCharPtr);
			myfile.read(reinterpret_cast<char*>(FBufferData_Inverse_2048), std::streamsize(256 * 4 * sizeof(uint16_t)));
			myfile.close();
		}

		//// i for row index and j for col index
		//for (int i = 0; i < 16; i++) {
		//	for (int j = 0; j < 16; j++) {
		//		UINT FlattenedIndex = i * 16 + j;

		//		float Expo = -2.f * PI * float(i) * float(j) / 16.f;
		//		FBufferData_1024[FlattenedIndex] = Float16Compressor::compress(std::cos(Expo));
		//		FBufferData_1024[FlattenedIndex + 256] = Float16Compressor::compress(std::sin(Expo));
		//		FBufferData_Inverse_1024[FlattenedIndex] = Float16Compressor::compress(std::cos(-Expo));
		//		FBufferData_Inverse_1024[FlattenedIndex + 256] = Float16Compressor::compress(std::sin(-Expo));

		//		FBufferData_2048[FlattenedIndex] = FBufferData_1024[FlattenedIndex];
		//		FBufferData_2048[FlattenedIndex + 256] = FBufferData_1024[FlattenedIndex + 256];
		//		FBufferData_Inverse_2048[FlattenedIndex] = FBufferData_Inverse_1024[FlattenedIndex];
		//		FBufferData_Inverse_2048[FlattenedIndex + 256] = FBufferData_Inverse_1024[FlattenedIndex + 256];
		//	}
		//}
		//for (int i = 0; i < 16; i++) {
		//	for (int j = 0; j < 16; j++) {
		//		UINT FlattenedIndex = i * 16 + j;
		//		if (i > 4 || j > 4) {
		//			FBufferData_1024[FlattenedIndex + 512] = Float16Compressor::compress(0.f);
		//			FBufferData_1024[FlattenedIndex + 768] = Float16Compressor::compress(0.f);
		//			FBufferData_Inverse_1024[FlattenedIndex + 512] = Float16Compressor::compress(0.f);
		//			FBufferData_Inverse_1024[FlattenedIndex + 768] = Float16Compressor::compress(0.f);
		//		}
		//		else {
		//			float Expo = -2.f * PI * float(i) * float(j) / 4.f;

		//			FBufferData_1024[FlattenedIndex + 512] = Float16Compressor::compress(std::cos(Expo));
		//			FBufferData_1024[FlattenedIndex + 768] = Float16Compressor::compress(std::sin(Expo));
		//			FBufferData_Inverse_1024[FlattenedIndex + 512] = Float16Compressor::compress(std::cos(-Expo));
		//			FBufferData_Inverse_1024[FlattenedIndex + 768] = Float16Compressor::compress(std::sin(-Expo));
		//		}
		//		if (i > 8 || j > 8) {
		//			FBufferData_2048[FlattenedIndex + 512] = Float16Compressor::compress(0.f);
		//			FBufferData_2048[FlattenedIndex + 768] = Float16Compressor::compress(0.f);
		//			FBufferData_Inverse_2048[FlattenedIndex + 512] = Float16Compressor::compress(0.f);
		//			FBufferData_Inverse_2048[FlattenedIndex + 768] = Float16Compressor::compress(0.f);
		//		}
		//		else{
		//			float Expo = -2.f * PI * float(i) * float(j) / 8.f;

		//			FBufferData_2048[FlattenedIndex + 512] = Float16Compressor::compress(std::cos(Expo));
		//			FBufferData_2048[FlattenedIndex + 768] = Float16Compressor::compress(std::sin(Expo));
		//			FBufferData_Inverse_2048[FlattenedIndex + 512] = Float16Compressor::compress(std::cos(-Expo));
		//			FBufferData_Inverse_2048[FlattenedIndex + 768] = Float16Compressor::compress(std::sin(-Expo));
		//		}
		//	}
		//}
	}
	//{
	//	D3D12_COMPUTE_PIPELINE_STATE_DESC PSODesc = {};
	//	PSODesc.CS = { g_InverseTwoForOne_CS, sizeof(g_InverseTwoForOne_CS) };// include by header
	//	PSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	//	PSODesc.NodeMask = 0;
	//	PSODesc.pRootSignature = m_SM66RootSignature.Get();
	//	FAILED(m_d3dDevice->CreateComputePipelineState(&PSODesc, IID_PPV_ARGS(&m_InverseTwoForOnePSO)));
	//}
	return BloomTensorcore_Result::Result_Success;
}


BloomTensorcore_Result FBloomTensorcoreExecuteD3D12RHI::ExecuteBloomTensorcore(FRHICommandList& CmdList, const FBloomRHIExecuteArguments& InArguments)
{
	

	check(!IsRunningRHIInSeparateThread() || IsInRHIThread());

	// const std::map<std::string, FRHITextureRef> InArguments.ModelInputs
	
	/*ID3D12DescriptorHeap* PreviousHeaps[2] =
	{
		StateCache.GetDescriptorCache()->GetCurrentViewHeap()->GetHeap(),
		StateCache.GetDescriptorCache()->GetCurrentSamplerHeap()->GetHeap(),
	};*/

	/*FD3D12Device* Device = InternalD3D12RHI->GetAdapter().GetDevice(CmdList.GetGPUMask().ToIndex());
	FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();*/

	const uint32 DeviceIndex = D3D12RHI->RHIGetResourceDeviceIndex(InArguments.SrcTexture);
	ID3D12GraphicsCommandList* D3DGraphicsCommandList = D3D12RHI->RHIGetGraphicsCommandList(DeviceIndex);

	/*FD3D12Texture* const SrcTextureD3D12 = DefaultContext.RetrieveTexture(InArguments.SrcTexture);
	FD3D12Texture* const KernelTextureD3D12 = DefaultContext.RetrieveTexture(InArguments.KernelTexture);
	FD3D12Texture* const IntermediateTexture0D3D12 = DefaultContext.RetrieveTexture(InArguments.Intermediate0);
	FD3D12Texture* const IntermediateTexture1D3D12 = DefaultContext.RetrieveTexture(InArguments.Intermediate1);*/
	FD3D12Texture* const SrcTextureD3D12 = (FD3D12Texture* const)(InArguments.SrcTexture);
	FD3D12Texture* const KernelTextureD3D12 = (FD3D12Texture* const)(InArguments.KernelTexture);
	/*FD3D12Texture* const IntermediateTexture0D3D12 = (FD3D12Texture* const)(InArguments.Intermediate0);
	FD3D12Texture* const IntermediateTexture1D3D12 = (FD3D12Texture* const)(InArguments.Intermediate1);*/

	/*FD3D12UnorderedAccessView* OutputUAV = DefaultContext.RetrieveObject<>(InArguments.OutputTexture);
	FD3D12UnorderedAccessView* Intermediate0_UAV = DefaultContext.RetrieveObject<FD3D12UnorderedAccessView_RHI>(InArguments.Intermediate0_UAV);
	FD3D12UnorderedAccessView* Intermediate1_UAV = DefaultContext.RetrieveObject<FD3D12UnorderedAccessView_RHI>(InArguments.Intermediate1_UAV);*/

	FD3D12ShaderResourceView* const PostFilterParaSrv = static_cast<FD3D12ShaderResourceView*>((FD3D12ShaderResourceView_RHI*)InArguments.PostFilterParaBuffer);

	FD3D12UnorderedAccessView* const OutputUAV = static_cast<FD3D12UnorderedAccessView*>((FD3D12UnorderedAccessView_RHI*)InArguments.OutputTexture);
	/*FD3D12UnorderedAccessView* const Intermediate0_UAV = static_cast<FD3D12UnorderedAccessView*>((FD3D12UnorderedAccessView_RHI*)InArguments.Intermediate0_UAV);
	FD3D12UnorderedAccessView* const Intermediate1_UAV = static_cast<FD3D12UnorderedAccessView*>((FD3D12UnorderedAccessView_RHI*)InArguments.Intermediate1_UAV);*/
	
	auto SrcSrvHandle = SrcTextureD3D12->GetShaderResourceView()->GetOfflineCpuHandle();
	auto KernelSrvHandle = KernelTextureD3D12->GetShaderResourceView()->GetOfflineCpuHandle();
	/*auto Intermediate0SrvHandle = IntermediateTexture0D3D12->GetShaderResourceView()->GetOfflineCpuHandle();
	auto Intermediate1SrvHandle = IntermediateTexture1D3D12->GetShaderResourceView()->GetOfflineCpuHandle();*/
	auto PostFilterParaSrvHandle = PostFilterParaSrv->GetOfflineCpuHandle();
	auto OutputUavHandle = OutputUAV->GetOfflineCpuHandle();
	/*auto Intermediate0UavHandle = Intermediate0_UAV->GetOfflineCpuHandle();
	auto Intermediate1UavHandle = Intermediate1_UAV->GetOfflineCpuHandle();*/

	// copy srv/uav to our descriptor heap
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::SrcTexture), SrcSrvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::KernelTexture), KernelSrvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	/*m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::Intermediate0), Intermediate0SrvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::Intermediate1), Intermediate1SrvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	*/
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::Intermediate0), m_CPUDescriptorHeap->GetCpuHandle(2), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::Intermediate1), m_CPUDescriptorHeap->GetCpuHandle(3), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::OutputUav), OutputUavHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	/*m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::Intermediate0Uav), Intermediate0UavHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::Intermediate1Uav), Intermediate1UavHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	*/
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::Intermediate0Uav), m_CPUDescriptorHeap->GetCpuHandle(4), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::Intermediate1Uav), m_CPUDescriptorHeap->GetCpuHandle(5), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::PostFilterPara), PostFilterParaSrvHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	
	
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::FBuffer), m_CPUDescriptorHeap->GetCpuHandle(0), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_d3dDevice->CopyDescriptorsSimple(1, m_DescriptorHeap[FrameIndex % 3]->GetCpuHandle(DescriptorOffsetMapping::FBufferInverse), m_CPUDescriptorHeap->GetCpuHandle(1), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	ID3D12DescriptorHeap* pHeaps[] = { m_DescriptorHeap[FrameIndex % 3]->Heap() };
	D3DGraphicsCommandList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);

	D3DGraphicsCommandList->SetComputeRootSignature(m_SM66RootSignature.Get());

	// debug
	//{
	//	struct Params {
	//		FInt32Vector4 DstRect;
	//		FVector3f BrightPixelGain;
	//		UINT Width;
	//		UINT Height;
	//		UINT TransformType;
	//		UINT InputTextureOffset;
	//		UINT OutputTextureOffset;
	//		UINT DstPostFilterParaOffset;
	//	};
	//	UINT TextureWidth = InArguments.SrcTexture->GetDesc().Extent.X;
	//	UINT TextureHeight = InArguments.SrcTexture->GetDesc().Extent.Y;
	//	Params p = { {0,0,0,0}, {0.f,0.f,0.f}, TextureWidth, TextureHeight, 0, DescriptorOffsetMapping::SrcTexture, DescriptorOffsetMapping::OutputUav, 0 };
	//	D3DGraphicsCommandList->SetComputeRoot32BitConstants(0, sizeof(Params) / sizeof(UINT), &p, 0);
	//	D3DGraphicsCommandList->SetPipelineState(m_DebugTwoForOnePSO.Get());

	//	// dispatch
	//	D3DGraphicsCommandList->Dispatch(FMath::DivideAndRoundUp(TextureWidth, (UINT)16), FMath::DivideAndRoundUp(TextureHeight, (UINT)16), 1);

	//}

	enum TransformType {
		Horizontal = 0x1,
		Forward = 0x2,
		ModifyInput = 0x4,
		UseAlpha = 0x8
	};

	const UINT MaxLength = FMath::Max(InArguments.ViewportWidth, InArguments.ViewportHeight);
	const bool SCAN_LINE_LENGTH_1024 = (MaxLength > 512 && MaxLength <= 1024);
	const bool SCAN_LINE_LENGTH_2048 = (MaxLength > 1024 && MaxLength <= 2048);

	const UINT ScanLineLength = SCAN_LINE_LENGTH_2048 ? 2048 : 1024;

	if (cachedScanLineLenght != ScanLineLength) {
		{
			D3D12_SUBRESOURCE_DATA bufferData = {};
			bufferData.pData = SCAN_LINE_LENGTH_1024 ? FBufferData_1024 : FBufferData_2048;
			UpdateSubresources(D3DGraphicsCommandList, m_FBuffer.Get(), m_FBuffer_Upload.Get(), 0, 0, 1, &bufferData);
		}
		{
			D3D12_SUBRESOURCE_DATA bufferData = {};
			bufferData.pData = SCAN_LINE_LENGTH_1024 ? FBufferData_Inverse_1024 : FBufferData_Inverse_2048;
			UpdateSubresources(D3DGraphicsCommandList, m_FBuffer_Inverse.Get(), m_FBuffer_Inverse_Upload.Get(), 0, 0, 1, &bufferData);
		}
		cachedScanLineLenght = ScanLineLength;
	}
	{
		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = m_Intermediate0.Get();
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS ;
		D3DGraphicsCommandList->ResourceBarrier(1, &BarrierDesc);
	}
	// two for one
	{
		struct Params {
			FInt32Vector4 DstRect;
			FVector3f BrightPixelGain;
			UINT Width;
			UINT Height;
			UINT TransformType;
			UINT InputTextureOffset;
			UINT OutputTextureOffset;
			UINT DstPostFilterParaOffset;
			UINT FBufferOffset;
			UINT FBufferInverseOffset;
		};
		Params p;
		p.DstRect = FInt32Vector4(0, 0, ScanLineLength + 2, InArguments.ViewportHeight);
		p.BrightPixelGain = InArguments.BrightPixelGain;
		p.Width = InArguments.ViewportWidth;
		p.Height = InArguments.ViewportHeight;
		p.TransformType = Horizontal | Forward | ModifyInput;
		p.InputTextureOffset = DescriptorOffsetMapping::SrcTexture;
		p.OutputTextureOffset = DescriptorOffsetMapping::Intermediate0Uav;
		p.DstPostFilterParaOffset = DescriptorOffsetMapping::PostFilterPara;
		p.FBufferOffset = DescriptorOffsetMapping::FBuffer;
		p.FBufferInverseOffset = DescriptorOffsetMapping::FBufferInverse;
		D3DGraphicsCommandList->SetComputeRoot32BitConstants(0, sizeof(Params) / sizeof(UINT), &p, 0);
		D3DGraphicsCommandList->SetPipelineState(m_TwoForOnePSO[SCAN_LINE_LENGTH_2048].Get());

		// dispatch
		D3DGraphicsCommandList->Dispatch(1, 1, InArguments.ViewportHeight);

		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = m_Intermediate0.Get();
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		D3DGraphicsCommandList->ResourceBarrier(1, &BarrierDesc);
	}
	{
		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = m_Intermediate1.Get();
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS ;
		D3DGraphicsCommandList->ResourceBarrier(1, &BarrierDesc);
	}
	// convolve with texture
	{
		struct Params {
			FInt32Vector2 SrcRectMax;
			FInt32Vector2 DstExtent;
			UINT TransformType;
			UINT InputTextureOffset;
			UINT FilterTextureOffset;
			UINT OutputTextureOffset;
			UINT FBufferOffset;
			UINT FBufferInverseOffset;
		};
		Params p;
		p.SrcRectMax = FInt32Vector2(ScanLineLength + 2, InArguments.ViewportHeight);
		p.DstExtent = FInt32Vector2(ScanLineLength + 2, InArguments.ViewportHeight);
		p.TransformType = Forward | UseAlpha;
		p.InputTextureOffset = DescriptorOffsetMapping::Intermediate0;
		p.FilterTextureOffset = DescriptorOffsetMapping::KernelTexture;
		p.OutputTextureOffset = DescriptorOffsetMapping::Intermediate1Uav;
		p.FBufferOffset = DescriptorOffsetMapping::FBuffer;
		p.FBufferInverseOffset = DescriptorOffsetMapping::FBufferInverse;
		D3DGraphicsCommandList->SetComputeRoot32BitConstants(0, sizeof(Params) / sizeof(UINT), &p, 0);
		D3DGraphicsCommandList->SetPipelineState(m_ConvolveWithTexturePSO[SCAN_LINE_LENGTH_2048].Get());

		D3DGraphicsCommandList->Dispatch(1, 1, ScanLineLength + 2);


		D3D12_RESOURCE_BARRIER BarrierDesc = {};
		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = m_Intermediate1.Get();
		BarrierDesc.Transition.Subresource = 0;
		BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
		D3DGraphicsCommandList->ResourceBarrier(1, &BarrierDesc);
	}
	

	// two for one back to final image
	{
		struct Params {
			FInt32Vector4 DstRect;
			FVector3f BrightPixelGain;
			UINT Width;
			UINT Height;
			UINT TransformType;
			UINT InputTextureOffset;
			UINT OutputTextureOffset;
			UINT DstPostFilterParaOffset;
			UINT FBufferOffset;
			UINT FBufferInverseOffset;
		};
		Params p;
		p.DstRect = FInt32Vector4(0, 0, InArguments.ViewportWidth, InArguments.ViewportHeight);
		p.BrightPixelGain = InArguments.BrightPixelGain;
		p.Width = ScanLineLength + 2;
		p.Height = InArguments.ViewportHeight;
		p.TransformType = Horizontal | ModifyInput;
		p.InputTextureOffset = DescriptorOffsetMapping::Intermediate1;
		p.OutputTextureOffset = DescriptorOffsetMapping::OutputUav;
		p.DstPostFilterParaOffset = DescriptorOffsetMapping::PostFilterPara;
		p.FBufferOffset = DescriptorOffsetMapping::FBuffer;
		p.FBufferInverseOffset = DescriptorOffsetMapping::FBufferInverse;
		D3DGraphicsCommandList->SetComputeRoot32BitConstants(0, sizeof(Params) / sizeof(UINT), &p, 0);
		D3DGraphicsCommandList->SetPipelineState(m_TwoForOnePSO[SCAN_LINE_LENGTH_2048].Get());

		D3DGraphicsCommandList->Dispatch(1, 1, InArguments.ViewportHeight);

	}
	
	/*if (Device->GetDefaultCommandContext().IsDefaultContext())
	{
		Device->RegisterGPUWork(1);
	}*/

	//D3DGraphicsCommandList->SetDescriptorHeaps(2, PreviousHeaps);
	/*Device->GetDefaultCommandContext().StateCache.GetDescriptorCache()->SetDescriptorHeaps(true);
	Device->GetDefaultCommandContext().StateCache.ForceSetComputeRootSignature();*/

	D3D12RHI->RHIFinishExternalComputeWork(0, D3DGraphicsCommandList); // New API
	FrameIndex = (FrameIndex + 1) % 3;
	// Device->GetCommandContext().StateCache.GetDescriptorCache()->SetCurrentCommandList(Device->GetCommandContext().CommandListHandle);
	return BloomTensorcore_Result::Result_Success;
}

/** IModuleInterface implementation */

void FBloomTensorcoreExecuteD3D12RHIModule::StartupModule()
{
	// BloomTensorcoreExecuteRHI module should be loaded to ensure logging state is initialized
	FModuleManager::LoadModuleChecked<IBloomTensorcoreExecuteRHIModule>(TEXT("BloomTensorcoreExecuteRHI"));
}

void FBloomTensorcoreExecuteD3D12RHIModule::ShutdownModule()
{
}

TUniquePtr<BloomTensorcoreExecuteRHI> FBloomTensorcoreExecuteD3D12RHIModule::CreateBloomTensorcoreExecuteRHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments)
{
	TUniquePtr<BloomTensorcoreExecuteRHI> Result(new FBloomTensorcoreExecuteD3D12RHI(Arguments));
	return Result;
}

IMPLEMENT_MODULE(FBloomTensorcoreExecuteD3D12RHIModule, BloomTensorcoreExecuteD3D12RHI)

#undef LOCTEXT_NAMESPACE




