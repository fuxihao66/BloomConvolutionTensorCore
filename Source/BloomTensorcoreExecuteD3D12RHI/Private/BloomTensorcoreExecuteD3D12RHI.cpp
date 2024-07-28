#include "BloomTensorcoreExecuteD3D12RHI.h"

#include "D3D12RHIPrivate.h"
#include "D3D12Util.h"
#include "D3D12State.h"
#include "D3D12Resources.h"
#include "D3D12Viewport.h"
#include "D3D12ConstantBuffer.h"
#include "D3D12CommandContext.h"
#include "RHIValidationCommon.h"
#include "ID3D12DynamicRHI.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Interfaces/IPluginManager.h"



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
	std::unique_ptr<DescriptorHeapWrapper>        	m_DescriptorHeap;
	
	UINT                                            m_currentDescriptorTopIndex;
public:
	FBloomTensorcoreExecuteD3D12RHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments);
	virtual BloomTensorcore_Result ExecuteInference(FRHICommandList& CmdList, const FBloomRHIExecuteArguments& InArguments) final;
	virtual ~FBloomTensorcoreExecuteD3D12RHI();
private:
	BloomTensorcore_Result CreateD3D12Resources();

	ID3D12Device* m_d3dDevice = nullptr;
	ID3D12DynamicRHI* D3D12RHI = nullptr;
};


FBloomTensorcoreExecuteD3D12RHI::FBloomTensorcoreExecuteD3D12RHI(const FBloomTensorcoreExecuteRHICreateArguments& Arguments)
	: BloomTensorcoreExecuteRHI(Arguments)
	, D3D12RHI(CastDynamicRHI<ID3D12DynamicRHI>(Arguments.DynamicRHI))
	//, D3D12RHI(static_cast<FD3D12DynamicRHI*>(Arguments.DynamicRHI))
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
BloomTensorcore_Result FBloomTensorcoreExecuteD3D12RHI::CreateD3D12Resources(){
	// initialize once
	
	m_DescriptorHeap = std::make_unique<DescriptorHeapWrapper>(
		m_d3dDevice,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		MAX_DESCRIPTOR_COUNT);
	m_currentDescriptorTopIndex = 0;

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

	/*FD3D12Device* Device = D3D12RHI->GetAdapter().GetDevice(CmdList.GetGPUMask().ToIndex());
	ID3D12GraphicsCommandList* D3DGraphicsCommandList = Device->GetCommandContext().CommandListHandle.GraphicsCommandList();*/
	/*FD3D12Device* Device = D3D12RHI->GetAdapter().GetDevice(CmdList.GetGPUMask().ToIndex());
	ID3D12GraphicsCommandList* D3DGraphicsCommandList = Device->GetDefaultCommandContext().GraphicsCommandList().Get();*/
	ID3D12GraphicsCommandList* D3DGraphicsCommandList = D3D12RHI->RHIGetGraphicsCommandList(0);
	
	FD3D12Texture* const SrcTextureD3D12 = Device->GetCommandContext().RetrieveTexture(InArguments.SrcTexture);
	FD3D12Texture* const KernelTextureD3D12 = Device->GetCommandContext().RetrieveTexture(InArguments.KernelTexture);
	FD3D12Texture* const IntermediateTexture0D3D12 = Device->GetCommandContext().RetrieveTexture(InArguments.Intermediate0);
	FD3D12Texture* const IntermediateTexture1D3D12 = Device->GetCommandContext().RetrieveTexture(InArguments.Intermediate1);
	FD3D12UnorderedAccessView* OutputUAV = Device->GetCommandContext().RetrieveObject<FD3D12UnorderedAccessView_RHI>(InArguments.OutputTexture);
	FD3D12UnorderedAccessView* Intermediate0_UAV = Device->GetCommandContext().RetrieveObject<FD3D12UnorderedAccessView_RHI>(InArguments.Intermediate0_UAV);
	FD3D12UnorderedAccessView* Intermediate1_UAV = Device->GetCommandContext().RetrieveObject<FD3D12UnorderedAccessView_RHI>(InArguments.Intermediate1_UAV);

	auto SrcSrvHandle = SrcTextureD3D12->GetShaderResourceView()->GetOfflineCpuHandle();
	auto KernelSrvHandle = KernelTextureD3D12->GetShaderResourceView()->GetOfflineCpuHandle();
	auto Intermediate0SrvHandle = IntermediateTexture0D3D12->GetShaderResourceView()->GetOfflineCpuHandle();
	auto Intermediate1SrvHandle = IntermediateTexture1D3D12->GetShaderResourceView()->GetOfflineCpuHandle();
	auto OutputUavHandle = OutputUAV->GetOfflineCpuHandle();
	auto Intermediate0UavHandle = Intermediate0_UAV->GetOfflineCpuHandle();
	auto Intermediate1UavHandle = Intermediate1_UAV->GetOfflineCpuHandle();

	// copy srv/uav to our descriptor heap
	m_d3dDevice->CopyDescriptorsSimple(7, );

	// two for one

	D3DGraphicsCommandList->ResourceBarrier(1, &barrier);
	// convolve with texture

	D3DGraphicsCommandList->ResourceBarrier(1, &barrier);

	// two for one back to final image
	D3DGraphicsCommandList->ResourceBarrier(1, &barrier);


	auto & modelInputs = InArguments.InputBuffers;
	auto modelOutputResourcePointer = D3D12RHI->RHIGetResource(InArguments.OutputBuffer); // version >= 5.1


	ID3D12DescriptorHeap* pHeaps[] = { m_DescriptorHeap->Heap() };
	D3DGraphicsCommandList->SetDescriptorHeaps(_countof(pHeaps), pHeaps);
	
	
	// execute

	/*if (Device->GetDefaultCommandContext().IsDefaultContext())
	{
		Device->RegisterGPUWork(1);
	}*/

	//D3DGraphicsCommandList->SetDescriptorHeaps(2, PreviousHeaps);
	/*Device->GetDefaultCommandContext().StateCache.GetDescriptorCache()->SetDescriptorHeaps(true);
	Device->GetDefaultCommandContext().StateCache.ForceSetComputeRootSignature();*/

	D3D12RHI->RHIFinishExternalComputeWork(0, D3DGraphicsCommandList); // New API

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




