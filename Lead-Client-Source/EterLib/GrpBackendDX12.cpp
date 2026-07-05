#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpBackendDX12.h"
#include "GraphicShaderPool.h"
#include "GrpDynamicDrawDX12.h"

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

CGraphicBackendDX12* CGraphicBackendDX12::ms_pkInstance = NULL;

CGraphicBackendDX12* CGraphicBackendDX12::GetInstance()
{
	return ms_pkInstance;
}

CGraphicBackendDX12::CGraphicBackendDX12()
	: m_apkProgramBytecode(NULL)
	, m_uProgramCount(0)
	, m_akInputElements(NULL)
	, m_uInputElementCount(0)
	, m_uInputLayoutID(0)
	, m_uVertexProgram(0)
	, m_uPixelProgram(0)
	, m_uSRVIncrementSize(0)
	, m_pkWhiteTexture(NULL)
	, m_pkCPUHeap(NULL)
	, m_bInFrame(false)
	, m_bCreated(false)
{
	for (UINT u = 0; u < TEXTURE_STAGE_COUNT; ++u)
		m_akTextureSRVs[u].ptr = 0;
	m_kWhiteSRV.ptr = 0;
}

CGraphicBackendDX12::~CGraphicBackendDX12()
{
	Destroy();
}

bool CGraphicBackendDX12::Create(HWND hWnd, UINT uWidth, UINT uHeight, bool bWindowed)
{
	Destroy();

	if (!m_kDevice.Create(hWnd, uWidth, uHeight, bWindowed))
	{
		Destroy();
		return false;
	}

	ID3D12Device* pkDevice = m_kDevice.GetDevice();

	if (!m_kRootSignature.Create(pkDevice) ||
		!m_kUploadRing.Create(pkDevice, UPLOAD_RING_BYTES) ||
		!m_kSRVRing.Create(pkDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SRV_RING_CAPACITY) ||
		!m_kSamplerCache.Create(pkDevice, SAMPLER_TABLE_CAPACITY) ||
		!m_kPipelineCache.Create(pkDevice, m_kRootSignature.GetRootSignature()) ||
		!m_kUploader.Create(pkDevice) ||
		!m_kTextureDescriptors.Create(pkDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV))
	{
		TraceError("CGraphicBackendDX12: component creation failed.");
		Destroy();
		return false;
	}

	m_uSRVIncrementSize = pkDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_kConstantShadow.Reset();

	if (!__CompilePrograms() || !__CreateWhiteTexture())
	{
		Destroy();
		return false;
	}

	m_bCreated = true;
	ms_pkInstance = this;
	return true;
}

void CGraphicBackendDX12::Destroy()
{
	if (ms_pkInstance == this)
		ms_pkInstance = NULL;

	if (m_kDevice.IsCreated())
		m_kDevice.WaitForGPU();

	if (m_apkProgramBytecode)
	{
		for (UINT u = 0; u < m_uProgramCount; ++u)
			safe_release(m_apkProgramBytecode[u]);
		delete [] m_apkProgramBytecode;
		m_apkProgramBytecode = NULL;
	}
	m_uProgramCount = 0;

	safe_release(m_pkWhiteTexture);
	safe_release(m_pkCPUHeap);
	m_kWhiteSRV.ptr = 0;

	m_kPipelineCache.Destroy();
	m_kSamplerCache.Destroy();
	m_kSRVRing.Destroy();
	m_kUploadRing.Destroy();
	m_kUploader.Destroy();
	m_kTextureDescriptors.Destroy();
	m_kRootSignature.Destroy();
	m_kDevice.Destroy();

	for (UINT u = 0; u < TEXTURE_STAGE_COUNT; ++u)
		m_akTextureSRVs[u].ptr = 0;
	m_akInputElements = NULL;
	m_uInputElementCount = 0;
	m_bInFrame = false;
	m_bCreated = false;
}

bool CGraphicBackendDX12::IsCreated() const
{
	return m_bCreated;
}

CGraphicDeviceDX12& CGraphicBackendDX12::GetDevice()
{
	return m_kDevice;
}

CGraphicResourceUploaderDX12& CGraphicBackendDX12::GetUploader()
{
	return m_kUploader;
}

bool CGraphicBackendDX12::CreateTextureSRV(ID3D12Resource* pkTexture, D3D12_CPU_DESCRIPTOR_HANDLE* pkHandleOut)
{
	if (!pkTexture || !m_kTextureDescriptors.Allocate(pkHandleOut))
		return false;

	m_kDevice.GetDevice()->CreateShaderResourceView(pkTexture, NULL, *pkHandleOut);
	return true;
}

void CGraphicBackendDX12::FreeTextureSRV(D3D12_CPU_DESCRIPTOR_HANDLE kHandle)
{
	m_kTextureDescriptors.Free(kHandle);
}

bool CGraphicBackendDX12::__CompilePrograms()
{
	m_uProgramCount = CGraphicShaderPool::GetProgramCount();
	m_apkProgramBytecode = new ID3DBlob*[m_uProgramCount];
	for (UINT u = 0; u < m_uProgramCount; ++u)
		m_apkProgramBytecode[u] = NULL;

	for (UINT u = 0; u < m_uProgramCount; ++u)
	{
		const CGraphicShaderPool::TProgramInfo* pkInfo = CGraphicShaderPool::GetProgramInfo(u);

		ID3DBlob* pkErrors = NULL;
		const HRESULT hrCompile = D3DCompile(pkInfo->c_szSource, pkInfo->uSourceLength,
											 pkInfo->c_szName, NULL, NULL, "main",
											 pkInfo->bVertexProgram ? "vs_5_0" : "ps_5_0",
											 D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, 0,
											 &m_apkProgramBytecode[u], &pkErrors);
		if (FAILED(hrCompile))
		{
			TraceError("CGraphicBackendDX12: SM5 build of %s failed [ %s ].", pkInfo->c_szName,
					   pkErrors ? (const char*)pkErrors->GetBufferPointer() : "unknown");
			safe_release(pkErrors);
			return false;
		}
		safe_release(pkErrors);
	}

	return true;
}

bool CGraphicBackendDX12::__CreateWhiteTexture()
{
	D3D12_DESCRIPTOR_HEAP_DESC kHeapDesc = {};
	kHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	kHeapDesc.NumDescriptors = 1;

	if (FAILED(m_kDevice.GetDevice()->CreateDescriptorHeap(&kHeapDesc, IID_PPV_ARGS(&m_pkCPUHeap))))
	{
		TraceError("CGraphicBackendDX12: CPU descriptor heap creation failed.");
		return false;
	}

	const DWORD c_dwWhitePixel = 0xFFFFFFFF;
	m_pkWhiteTexture = m_kUploader.CreateTexture2D(m_kDevice.GetCommandQueue(), 1, 1,
												   DXGI_FORMAT_B8G8R8A8_UNORM, &c_dwWhitePixel, 4);
	if (!m_pkWhiteTexture)
		return false;

	m_kWhiteSRV = m_pkCPUHeap->GetCPUDescriptorHandleForHeapStart();
	m_kDevice.GetDevice()->CreateShaderResourceView(m_pkWhiteTexture, NULL, m_kWhiteSRV);
	return true;
}

bool CGraphicBackendDX12::BeginFrame(DWORD dwClearColor)
{
	if (!m_bCreated || !m_kDevice.BeginFrame())
		return false;

	m_kUploadRing.OnFrameCompleted(m_kDevice.GetCompletedFenceValue());
	m_kSRVRing.OnFrameCompleted(m_kDevice.GetCompletedFenceValue());
	m_kConstantShadow.OnFrameBegin();

	ID3D12GraphicsCommandList* pkCommandList = m_kDevice.GetCommandList();

	const float c_fInv255 = 1.0f / 255.0f;
	const float afClearColor[4] =
	{
		((dwClearColor >> 16) & 0xff) * c_fInv255,
		((dwClearColor >> 8) & 0xff) * c_fInv255,
		(dwClearColor & 0xff) * c_fInv255,
		((dwClearColor >> 24) & 0xff) * c_fInv255,
	};
	pkCommandList->ClearRenderTargetView(m_kDevice.GetCurrentRTVHandle(), afClearColor, 0, NULL);
	pkCommandList->ClearDepthStencilView(m_kDevice.GetDSVHandle(),
										 D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
										 1.0f, 0, 0, NULL);

	D3D12_VIEWPORT kViewport = {};
	kViewport.Width = static_cast<float>(m_kDevice.GetWidth());
	kViewport.Height = static_cast<float>(m_kDevice.GetHeight());
	kViewport.MaxDepth = 1.0f;
	pkCommandList->RSSetViewports(1, &kViewport);

	D3D12_RECT kScissor = {};
	kScissor.right = static_cast<LONG>(m_kDevice.GetWidth());
	kScissor.bottom = static_cast<LONG>(m_kDevice.GetHeight());
	pkCommandList->RSSetScissorRects(1, &kScissor);

	ID3D12DescriptorHeap* apkHeaps[] = { m_kSRVRing.GetHeap(), m_kSamplerCache.GetHeap() };
	pkCommandList->SetDescriptorHeaps(2, apkHeaps);
	pkCommandList->SetGraphicsRootSignature(m_kRootSignature.GetRootSignature());
	m_bInFrame = true;
	return true;
}

bool CGraphicBackendDX12::EndFrame()
{
	if (!m_bCreated)
		return false;

	m_bInFrame = false;
	m_kDevice.EndFrame();
	const bool bPresented = m_kDevice.Present();

	const UINT64 uSubmitted = m_kDevice.GetLastSubmittedFenceValue();
	m_kUploadRing.OnFrameSubmitted(uSubmitted);
	m_kSRVRing.OnFrameSubmitted(uSubmitted);
	return bPresented;
}

bool CGraphicBackendDX12::SetProgram(UINT uVertexProgramIndex, UINT uPixelProgramIndex)
{
	if (uVertexProgramIndex >= m_uProgramCount || uPixelProgramIndex >= m_uProgramCount)
		return false;

	m_uVertexProgram = uVertexProgramIndex;
	m_uPixelProgram = uPixelProgramIndex;
	return true;
}

void CGraphicBackendDX12::SetInputLayout(UINT uLayoutID, const D3D12_INPUT_ELEMENT_DESC* akElements, UINT uElementCount)
{
	m_uInputLayoutID = uLayoutID;
	m_akInputElements = akElements;
	m_uInputElementCount = uElementCount;
}

void CGraphicBackendDX12::SetTextureSRV(UINT uStage, D3D12_CPU_DESCRIPTOR_HANDLE kSRVHandle)
{
	if (uStage < TEXTURE_STAGE_COUNT)
		m_akTextureSRVs[uStage] = kSRVHandle;
}

void CGraphicBackendDX12::ClearTextureSRV(UINT uStage)
{
	if (uStage < TEXTURE_STAGE_COUNT)
		m_akTextureSRVs[uStage].ptr = 0;
}

void CGraphicBackendDX12::SetSamplerKey(UINT uStage, const CGraphicSamplerKeyDX12& rkKey)
{
	if (uStage < TEXTURE_STAGE_COUNT)
		m_akSamplerKeys[uStage] = rkKey;
}

void CGraphicBackendDX12::SetPipelineStates(const CGraphicPipelineKeyDX12& rkKey)
{
	m_kPipelineKey = rkKey;
}

bool CGraphicBackendDX12::SetVSConstants(UINT uStartRegister, const float* afData, UINT uVector4Count)
{
	return m_kConstantShadow.SetVSConstants(uStartRegister, afData, uVector4Count);
}

bool CGraphicBackendDX12::SetPSConstants(UINT uStartRegister, const float* afData, UINT uVector4Count)
{
	return m_kConstantShadow.SetPSConstants(uStartRegister, afData, uVector4Count);
}

UINT CGraphicBackendDX12::ToTopologyTypeDX12(D3D_PRIMITIVE_TOPOLOGY eTopology)
{
	switch (eTopology)
	{
		case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
		case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		default:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}
}

bool CGraphicBackendDX12::__ApplyState(D3D_PRIMITIVE_TOPOLOGY eTopology)
{
	ID3D12GraphicsCommandList* pkCommandList = m_kDevice.GetCommandList();
	ID3D12Device* pkDevice = m_kDevice.GetDevice();

	// constants
	D3D12_GPU_VIRTUAL_ADDRESS uVSAddress = 0;
	D3D12_GPU_VIRTUAL_ADDRESS uPSAddress = 0;
	if (!m_kConstantShadow.FlushVS(m_kUploadRing, &uVSAddress) ||
		!m_kConstantShadow.FlushPS(m_kUploadRing, &uPSAddress))
		return false;
	pkCommandList->SetGraphicsRootConstantBufferView(CGraphicRootSignatureDX12::ROOT_PARAM_VS_CONSTANTS, uVSAddress);
	pkCommandList->SetGraphicsRootConstantBufferView(CGraphicRootSignatureDX12::ROOT_PARAM_PS_CONSTANTS, uPSAddress);

	// textures: unbound stages read the white texture
	CGraphicDescriptorRingDX12::TTable kSRVTable;
	if (!m_kSRVRing.Allocate(TEXTURE_STAGE_COUNT, &kSRVTable))
		return false;
	for (UINT u = 0; u < TEXTURE_STAGE_COUNT; ++u)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE kSource = m_akTextureSRVs[u].ptr ? m_akTextureSRVs[u] : m_kWhiteSRV;
		D3D12_CPU_DESCRIPTOR_HANDLE kDest = kSRVTable.kCPUHandle;
		kDest.ptr += static_cast<SIZE_T>(u) * m_uSRVIncrementSize;
		pkDevice->CopyDescriptorsSimple(1, kDest, kSource, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
	pkCommandList->SetGraphicsRootDescriptorTable(CGraphicRootSignatureDX12::ROOT_PARAM_SRV_TABLE, kSRVTable.kGPUHandle);

	// samplers
	D3D12_GPU_DESCRIPTOR_HANDLE kSamplerTable;
	if (!m_kSamplerCache.GetTable(m_akSamplerKeys[0], m_akSamplerKeys[1], &kSamplerTable))
		return false;
	pkCommandList->SetGraphicsRootDescriptorTable(CGraphicRootSignatureDX12::ROOT_PARAM_SAMPLER_TABLE, kSamplerTable);

	// pipeline
	m_kPipelineKey.m_uVertexShaderID = m_uVertexProgram;
	m_kPipelineKey.m_uPixelShaderID = m_uPixelProgram;
	m_kPipelineKey.m_uDeclarationID = m_uInputLayoutID;
	m_kPipelineKey.m_uTopologyType = ToTopologyTypeDX12(eTopology);

	ID3DBlob* pkVertexShader = m_apkProgramBytecode[m_uVertexProgram];
	ID3DBlob* pkPixelShader = m_apkProgramBytecode[m_uPixelProgram];

	D3D12_SHADER_BYTECODE kVSBytecode = { pkVertexShader->GetBufferPointer(), pkVertexShader->GetBufferSize() };
	D3D12_SHADER_BYTECODE kPSBytecode = { pkPixelShader->GetBufferPointer(), pkPixelShader->GetBufferSize() };

	ID3D12PipelineState* pkPipelineState =
		m_kPipelineCache.GetPipelineState(m_kPipelineKey, kVSBytecode, kPSBytecode,
										  m_akInputElements, m_uInputElementCount);
	if (!pkPipelineState)
		return false;

	pkCommandList->SetPipelineState(pkPipelineState);
	pkCommandList->IASetPrimitiveTopology(eTopology);
	return true;
}

bool CGraphicBackendDX12::DrawTransient(D3D_PRIMITIVE_TOPOLOGY eTopology,
										const void* pvVertices, UINT uVertexCount, UINT uStrideBytes,
										const WORD* awIndices, UINT uIndexCount)
{
	if (!m_bCreated || !m_bInFrame || !m_akInputElements)
		return false;

	if (!__ApplyState(eTopology))
		return false;

	ID3D12GraphicsCommandList* pkCommandList = m_kDevice.GetCommandList();

	D3D12_VERTEX_BUFFER_VIEW kVertexView;
	if (!CGraphicDynamicDrawDX12::WriteVertices(m_kUploadRing, pvVertices, uStrideBytes, uVertexCount, &kVertexView))
		return false;
	pkCommandList->IASetVertexBuffers(0, 1, &kVertexView);

	if (awIndices)
	{
		D3D12_INDEX_BUFFER_VIEW kIndexView;
		if (!CGraphicDynamicDrawDX12::WriteIndices(m_kUploadRing, awIndices, uIndexCount, &kIndexView))
			return false;
		pkCommandList->IASetIndexBuffer(&kIndexView);
		pkCommandList->DrawIndexedInstanced(uIndexCount, 1, 0, 0, 0);
	}
	else
	{
		pkCommandList->DrawInstanced(uVertexCount, 1, 0, 0);
	}

	return true;
}
