#include "StdAfx.h"
#include <set>
#include "../eterBase/Stl.h"
#include "GrpBackendDX12.h"
#include "GraphicShaderPool.h"
#include "GrpDynamicDrawDX12.h"
#include "GrpReadbackDX12.h"
#include "StateManager.h"

#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

CGraphicBackendDX12* CGraphicBackendDX12::ms_pkInstance = NULL;

CGraphicBackendDX12* CGraphicBackendDX12::GetInstance()
{
	return ms_pkInstance;
}

CGraphicBackendDX12::CGraphicBackendDX12()
	: m_apkProgramBytecode(NULL)
	, m_apkProgramBytecodeAlphaTest(NULL)
	, m_uProgramCount(0)
	, m_akInputElements(NULL)
	, m_uInputElementCount(0)
	, m_uInputLayoutID(0)
	, m_uVertexProgram(0)
	, m_uPixelProgram(0)
	, m_uSRVIncrementSize(0)
	, m_pkWhiteTexture(NULL)
	, m_pkCPUHeap(NULL)
	, m_pkBoundRenderTarget(NULL)
	, m_bInFrame(false)
	, m_bCreated(false)
	, m_fGammaFactor(1.0f)
	, m_pkSceneCopy(NULL)
	, m_uSceneCopyWidth(0)
	, m_uSceneCopyHeight(0)
	, m_bSceneCopyReadable(false)
	, m_uFrameOrdinal(0)
{
	for (UINT u = 0; u < TEXTURE_STAGE_COUNT; ++u)
		m_akTextureSRVs[u].ptr = 0;
	m_kWhiteSRV.ptr = 0;
	m_kSceneCopySRV.ptr = 0;
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
		!m_kTextureDescriptors.Create(pkDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) ||
		!m_kRTVDescriptors.Create(pkDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV) ||
		!m_kDSVDescriptors.Create(pkDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV))
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

	for (UINT u = 0; u < m_kRetiredResources.size(); ++u)
		m_kRetiredResources[u].pkResource->Release();
	m_kRetiredResources.clear();

	if (m_apkProgramBytecode)
	{
		for (UINT u = 0; u < m_uProgramCount; ++u)
			safe_release(m_apkProgramBytecode[u]);
		delete [] m_apkProgramBytecode;
		m_apkProgramBytecode = NULL;
	}
	if (m_apkProgramBytecodeAlphaTest)
	{
		for (UINT u = 0; u < m_uProgramCount; ++u)
			safe_release(m_apkProgramBytecodeAlphaTest[u]);
		delete [] m_apkProgramBytecodeAlphaTest;
		m_apkProgramBytecodeAlphaTest = NULL;
	}
	m_uProgramCount = 0;

	for (std::unordered_map<const void*, TRenderTargetDX12>::iterator it = m_kRenderTargetMap.begin();
		 it != m_kRenderTargetMap.end(); ++it)
	{
		safe_release(it->second.pkColor);
		safe_release(it->second.pkDepth);
	}
	m_kRenderTargetMap.clear();
	m_pkBoundRenderTarget = NULL;

	for (std::unordered_map<const void*, TRawTextureTwin>::iterator it = m_kRawTextureTwinMap.begin();
		 it != m_kRawTextureTwinMap.end(); ++it)
		safe_release(it->second.pkTexture);
	m_kRawTextureTwinMap.clear();

	safe_release(m_pkWhiteTexture);
	safe_release(m_pkCPUHeap);
	m_kWhiteSRV.ptr = 0;

	m_kGammaPass.Destroy();
	safe_release(m_pkSceneCopy);
	m_kSceneCopySRV.ptr = 0;
	m_uSceneCopyWidth = 0;
	m_uSceneCopyHeight = 0;
	m_bSceneCopyReadable = false;
	m_fGammaFactor = 1.0f;

	m_kRTVDescriptors.Destroy();
	m_kDSVDescriptors.Destroy();
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
	for (UINT u = 0; u < TEXTURE_STAGE_COUNT; ++u)
		if (m_akTextureSRVs[u].ptr == kHandle.ptr)
			m_akTextureSRVs[u].ptr = 0;

	m_kTextureDescriptors.Free(kHandle);
}

bool CGraphicBackendDX12::__CompilePrograms()
{
	m_uProgramCount = CGraphicShaderPool::GetProgramCount();
	m_apkProgramBytecode = new ID3DBlob*[m_uProgramCount];
	m_apkProgramBytecodeAlphaTest = new ID3DBlob*[m_uProgramCount];
	for (UINT u = 0; u < m_uProgramCount; ++u)
	{
		m_apkProgramBytecode[u] = NULL;
		m_apkProgramBytecodeAlphaTest[u] = NULL;
	}

	static const D3D_SHADER_MACRO c_akBaseMacros[] = { { "SM5", "1" }, { NULL, NULL } };
	static const D3D_SHADER_MACRO c_akAlphaTestMacros[] = { { "ALPHA_TEST", "1" }, { "SM5", "1" }, { NULL, NULL } };

	for (UINT u = 0; u < m_uProgramCount; ++u)
	{
		const CGraphicShaderPool::TProgramInfo* pkInfo = CGraphicShaderPool::GetProgramInfo(u);

		for (UINT uVariant = 0; uVariant < 2; ++uVariant)
		{
			// Alpha test exists only as a pixel-shader clip() variant.
			if (uVariant && pkInfo->bVertexProgram)
				continue;

			ID3DBlob** ppkTarget = uVariant ? &m_apkProgramBytecodeAlphaTest[u] : &m_apkProgramBytecode[u];
			ID3DBlob* pkErrors = NULL;
			const HRESULT hrCompile = D3DCompile(pkInfo->c_szSource, pkInfo->uSourceLength,
												 pkInfo->c_szName, uVariant ? c_akAlphaTestMacros : c_akBaseMacros, NULL, "main",
												 pkInfo->bVertexProgram ? "vs_5_0" : "ps_5_0",
												 D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY, 0,
												 ppkTarget, &pkErrors);
			if (FAILED(hrCompile))
			{
				TraceError("CGraphicBackendDX12: SM5 build of %s (variant %u) failed [ %s ].", pkInfo->c_szName,
						   uVariant, pkErrors ? (const char*)pkErrors->GetBufferPointer() : "unknown");
				safe_release(pkErrors);
				return false;
			}
			safe_release(pkErrors);
		}
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

	++m_uFrameOrdinal;
	m_kUploadRing.OnFrameCompleted(m_kDevice.GetCompletedFenceValue());
	m_kSRVRing.OnFrameCompleted(m_kDevice.GetCompletedFenceValue());
	__ReleaseRetiredResources();
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

	m_kCurrentRTV = m_kDevice.GetCurrentRTVHandle();
	m_kCurrentDSV = m_kDevice.GetDSVHandle();
	m_pkBoundRenderTarget = NULL;

	ID3D12DescriptorHeap* apkHeaps[] = { m_kSRVRing.GetHeap(), m_kSamplerCache.GetHeap() };
	pkCommandList->SetDescriptorHeaps(2, apkHeaps);
	pkCommandList->SetGraphicsRootSignature(m_kRootSignature.GetRootSignature());
	m_bInFrame = true;
	return true;
}

void CGraphicBackendDX12::ClearTargets(DWORD dwFlags, DWORD dwColor, float fDepth, DWORD dwStencil)
{
	if (!m_bCreated || !m_bInFrame)
		return;

	ID3D12GraphicsCommandList* pkCommandList = m_kDevice.GetCommandList();

	if (dwFlags & D3DCLEAR_TARGET)
	{
		const float c_fInv255 = 1.0f / 255.0f;
		const float afColor[4] =
		{
			((dwColor >> 16) & 0xff) * c_fInv255,
			((dwColor >> 8) & 0xff) * c_fInv255,
			(dwColor & 0xff) * c_fInv255,
			((dwColor >> 24) & 0xff) * c_fInv255,
		};
		pkCommandList->ClearRenderTargetView(m_kCurrentRTV, afColor, 0, NULL);
	}

	D3D12_CLEAR_FLAGS eDepthFlags = static_cast<D3D12_CLEAR_FLAGS>(0);
	if (dwFlags & D3DCLEAR_ZBUFFER)
		eDepthFlags |= D3D12_CLEAR_FLAG_DEPTH;
	if (dwFlags & D3DCLEAR_STENCIL)
		eDepthFlags |= D3D12_CLEAR_FLAG_STENCIL;
	if (eDepthFlags)
		pkCommandList->ClearDepthStencilView(m_kCurrentDSV, eDepthFlags, fDepth,
											 static_cast<UINT8>(dwStencil), 0, NULL);
}

void CGraphicBackendDX12::UnregisterRenderTarget(const void* pkTextureD3D9)
{
	std::unordered_map<const void*, TRenderTargetDX12>::iterator it = m_kRenderTargetMap.find(pkTextureD3D9);
	if (it == m_kRenderTargetMap.end())
		return;

	if (m_pkBoundRenderTarget == &it->second)
		m_pkBoundRenderTarget = NULL;

	m_kRTVDescriptors.Free(it->second.kRTV);
	m_kDSVDescriptors.Free(it->second.kDSV);
	m_kTextureDescriptors.Free(it->second.kSRV);
	RetireResource(it->second.pkColor);
	RetireResource(it->second.pkDepth);
	m_kRenderTargetMap.erase(it);
}

bool CGraphicBackendDX12::RegisterRenderTarget(const void* pkTextureD3D9, UINT uWidth, UINT uHeight)
{
	if (!m_bCreated || !pkTextureD3D9)
		return false;

	UnregisterRenderTarget(pkTextureD3D9);

	ID3D12Device* pkDevice = m_kDevice.GetDevice();
	TRenderTargetDX12 kTarget = {};
	kTarget.uWidth = uWidth;
	kTarget.uHeight = uHeight;
	kTarget.bShaderReadable = true;

	D3D12_HEAP_PROPERTIES kHeapProps = {};
	kHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC kColorDesc = {};
	kColorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	kColorDesc.Width = uWidth;
	kColorDesc.Height = uHeight;
	kColorDesc.DepthOrArraySize = 1;
	kColorDesc.MipLevels = 1;
	kColorDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	kColorDesc.SampleDesc.Count = 1;
	kColorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE kColorClear = {};
	kColorClear.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	kColorClear.Color[0] = kColorClear.Color[1] = kColorClear.Color[2] = kColorClear.Color[3] = 1.0f;

	if (FAILED(pkDevice->CreateCommittedResource(&kHeapProps, D3D12_HEAP_FLAG_NONE, &kColorDesc,
												  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &kColorClear,
												  IID_PPV_ARGS(&kTarget.pkColor))))
	{
		TraceError("CGraphicBackendDX12: render-target color creation failed (%ux%u).", uWidth, uHeight);
		return false;
	}

	D3D12_RESOURCE_DESC kDepthDesc = kColorDesc;
	kDepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	kDepthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE kDepthClear = {};
	kDepthClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	kDepthClear.DepthStencil.Depth = 1.0f;

	if (FAILED(pkDevice->CreateCommittedResource(&kHeapProps, D3D12_HEAP_FLAG_NONE, &kDepthDesc,
												  D3D12_RESOURCE_STATE_DEPTH_WRITE, &kDepthClear,
												  IID_PPV_ARGS(&kTarget.pkDepth))))
	{
		TraceError("CGraphicBackendDX12: render-target depth creation failed (%ux%u).", uWidth, uHeight);
		safe_release(kTarget.pkColor);
		return false;
	}

	if (!m_kRTVDescriptors.Allocate(&kTarget.kRTV) ||
		!m_kDSVDescriptors.Allocate(&kTarget.kDSV) ||
		!m_kTextureDescriptors.Allocate(&kTarget.kSRV))
	{
		safe_release(kTarget.pkColor);
		safe_release(kTarget.pkDepth);
		return false;
	}

	pkDevice->CreateRenderTargetView(kTarget.pkColor, NULL, kTarget.kRTV);
	pkDevice->CreateDepthStencilView(kTarget.pkDepth, NULL, kTarget.kDSV);
	pkDevice->CreateShaderResourceView(kTarget.pkColor, NULL, kTarget.kSRV);

	m_kRenderTargetMap[pkTextureD3D9] = kTarget;
	return true;
}

bool CGraphicBackendDX12::IsRenderTarget(const void* pkTextureD3D9) const
{
	return m_kRenderTargetMap.find(pkTextureD3D9) != m_kRenderTargetMap.end();
}

D3D12_CPU_DESCRIPTOR_HANDLE CGraphicBackendDX12::GetRenderTargetSRV(const void* pkTextureD3D9) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE kHandle;
	kHandle.ptr = 0;
	std::unordered_map<const void*, TRenderTargetDX12>::const_iterator it = m_kRenderTargetMap.find(pkTextureD3D9);
	if (it != m_kRenderTargetMap.end())
		kHandle = it->second.kSRV;
	return kHandle;
}

bool CGraphicBackendDX12::RegisterRawTextureTwin(const void* pkTextureD3D9, UINT uWidth, UINT uHeight,
												 DXGI_FORMAT eFormat,
												 const TTextureLevelData* akLevels, UINT uLevelCount)
{
	if (!m_bCreated || !pkTextureD3D9 || !akLevels || !uLevelCount)
		return false;

	UnregisterRawTextureTwin(pkTextureD3D9);

	TRawTextureTwin kTwin;
	kTwin.pkTexture = m_kUploader.CreateTexture2D(m_kDevice.GetCommandQueue(), uWidth, uHeight,
												  eFormat, akLevels, uLevelCount);
	if (!kTwin.pkTexture)
		return false;

	if (!CreateTextureSRV(kTwin.pkTexture, &kTwin.kSRV))
	{
		safe_release(kTwin.pkTexture);
		return false;
	}

	m_kRawTextureTwinMap[pkTextureD3D9] = kTwin;

	if (CStateManager::InstancePtr())
		STATEMANAGER.RegisterTextureSRVDX12(pkTextureD3D9, kTwin.kSRV);
	return true;
}

void CGraphicBackendDX12::UnregisterRawTextureTwin(const void* pkTextureD3D9)
{
	std::unordered_map<const void*, TRawTextureTwin>::iterator it = m_kRawTextureTwinMap.find(pkTextureD3D9);
	if (it == m_kRawTextureTwinMap.end())
		return;

	if (CStateManager::InstancePtr())
		STATEMANAGER.UnregisterTextureSRVDX12(pkTextureD3D9);

	FreeTextureSRV(it->second.kSRV);
	RetireResource(it->second.pkTexture);
	m_kRawTextureTwinMap.erase(it);
}

void CGraphicBackendDX12::RetireResource(ID3D12Resource* pkResource)
{
	if (!pkResource)
		return;

	if (!m_kDevice.IsCreated())
	{
		pkResource->Release();
		return;
	}

	TRetiredResource kRetired;
	kRetired.pkResource = pkResource;
	kRetired.uFrameOrdinal = m_uFrameOrdinal;
	m_kRetiredResources.push_back(kRetired);
}

void CGraphicBackendDX12::__ReleaseRetiredResources()
{
	UINT uKept = 0;
	for (UINT u = 0; u < m_kRetiredResources.size(); ++u)
	{
		if (m_uFrameOrdinal >= m_kRetiredResources[u].uFrameOrdinal + CGraphicDeviceDX12::FRAME_COUNT + 1)
			m_kRetiredResources[u].pkResource->Release();
		else
			m_kRetiredResources[uKept++] = m_kRetiredResources[u];
	}
	m_kRetiredResources.resize(uKept);
}

bool CGraphicBackendDX12::SetRenderTargetTexture(const void* pkTextureD3D9)
{
	if (!m_bCreated || !m_bInFrame)
		return false;

	std::unordered_map<const void*, TRenderTargetDX12>::iterator it = m_kRenderTargetMap.find(pkTextureD3D9);
	if (it == m_kRenderTargetMap.end())
		return false;

	TRenderTargetDX12& rkTarget = it->second;
	ID3D12GraphicsCommandList* pkCommandList = m_kDevice.GetCommandList();

	if (rkTarget.bShaderReadable)
	{
		D3D12_RESOURCE_BARRIER kBarrier = {};
		kBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		kBarrier.Transition.pResource = rkTarget.pkColor;
		kBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		kBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		kBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		pkCommandList->ResourceBarrier(1, &kBarrier);
		rkTarget.bShaderReadable = false;
	}

	m_kCurrentRTV = rkTarget.kRTV;
	m_kCurrentDSV = rkTarget.kDSV;
	m_pkBoundRenderTarget = &rkTarget;
	pkCommandList->OMSetRenderTargets(1, &m_kCurrentRTV, FALSE, &m_kCurrentDSV);

	D3D12_VIEWPORT kViewport = {};
	kViewport.Width = static_cast<float>(rkTarget.uWidth);
	kViewport.Height = static_cast<float>(rkTarget.uHeight);
	kViewport.MaxDepth = 1.0f;
	pkCommandList->RSSetViewports(1, &kViewport);

	D3D12_RECT kScissor = {};
	kScissor.right = static_cast<LONG>(rkTarget.uWidth);
	kScissor.bottom = static_cast<LONG>(rkTarget.uHeight);
	pkCommandList->RSSetScissorRects(1, &kScissor);
	return true;
}

void CGraphicBackendDX12::RestoreDefaultTarget()
{
	if (!m_bCreated || !m_bInFrame)
		return;

	ID3D12GraphicsCommandList* pkCommandList = m_kDevice.GetCommandList();

	if (m_pkBoundRenderTarget && !m_pkBoundRenderTarget->bShaderReadable)
	{
		// The receive pass samples this target next.
		D3D12_RESOURCE_BARRIER kBarrier = {};
		kBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		kBarrier.Transition.pResource = m_pkBoundRenderTarget->pkColor;
		kBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		kBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		kBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		pkCommandList->ResourceBarrier(1, &kBarrier);
		m_pkBoundRenderTarget->bShaderReadable = true;
	}

	m_pkBoundRenderTarget = NULL;
	m_kCurrentRTV = m_kDevice.GetCurrentRTVHandle();
	m_kCurrentDSV = m_kDevice.GetDSVHandle();
	pkCommandList->OMSetRenderTargets(1, &m_kCurrentRTV, FALSE, &m_kCurrentDSV);

	D3D12_VIEWPORT kViewport = {};
	kViewport.Width = static_cast<float>(m_kDevice.GetWidth());
	kViewport.Height = static_cast<float>(m_kDevice.GetHeight());
	kViewport.MaxDepth = 1.0f;
	pkCommandList->RSSetViewports(1, &kViewport);

	D3D12_RECT kScissor = {};
	kScissor.right = static_cast<LONG>(m_kDevice.GetWidth());
	kScissor.bottom = static_cast<LONG>(m_kDevice.GetHeight());
	pkCommandList->RSSetScissorRects(1, &kScissor);
}

void CGraphicBackendDX12::SetViewport(const D3DVIEWPORT9& rkViewport)
{
	if (!m_bCreated || !m_bInFrame)
		return;

	D3D12_VIEWPORT kViewport = {};
	kViewport.TopLeftX = static_cast<float>(rkViewport.X);
	kViewport.TopLeftY = static_cast<float>(rkViewport.Y);
	kViewport.Width = static_cast<float>(rkViewport.Width);
	kViewport.Height = static_cast<float>(rkViewport.Height);
	kViewport.MinDepth = rkViewport.MinZ;
	kViewport.MaxDepth = rkViewport.MaxZ;
	m_kDevice.GetCommandList()->RSSetViewports(1, &kViewport);
}

bool CGraphicBackendDX12::EndFrame()
{
	if (!m_bCreated)
		return false;

	__RecordGammaPass();

	m_bInFrame = false;
	m_kDevice.EndFrame();
	const bool bPresented = m_kDevice.Present();

	if (m_kDevice.IsDeviceRemoved())
	{
		static bool s_bReported = false;
		if (!s_bReported)
		{
			s_bReported = true;
			TraceError("CGraphicBackendDX12: graphics device removed; rendering stopped, restart the client.");
		}
	}

	const UINT64 uSubmitted = m_kDevice.GetLastSubmittedFenceValue();
	m_kUploadRing.OnFrameSubmitted(uSubmitted);
	m_kSRVRing.OnFrameSubmitted(uSubmitted);
	return bPresented;
}

void CGraphicBackendDX12::SetGammaFactor(float fFactor)
{
	m_fGammaFactor = fFactor;
}

bool CGraphicBackendDX12::CaptureBackBuffer(std::vector<BYTE>& rkPixels, UINT* puWidth, UINT* puHeight)
{
	if (!m_bCreated || !puWidth || !puHeight)
		return false;

	ID3D12Resource* pkSource = m_kDevice.GetLastPresentedBuffer();
	if (!pkSource)
		return false;

	CGraphicReadbackDX12 kReadback;
	if (!kReadback.Create(m_kDevice.GetDevice()))
		return false;

	const UINT uWidth = m_kDevice.GetWidth();
	const UINT uHeight = m_kDevice.GetHeight();
	rkPixels.resize(static_cast<size_t>(uWidth) * uHeight * 4);

	const bool bRead = kReadback.ReadTexture2D(m_kDevice.GetCommandQueue(), pkSource,
											   D3D12_RESOURCE_STATE_PRESENT,
											   &rkPixels[0], uWidth * 4);
	kReadback.Destroy();
	if (!bRead)
		return false;

	*puWidth = uWidth;
	*puHeight = uHeight;
	return true;
}

void CGraphicBackendDX12::__RecordGammaPass()
{
	if (m_fGammaFactor > 0.999f && m_fGammaFactor < 1.001f)
		return;

	ID3D12Device* pkDevice = m_kDevice.GetDevice();
	ID3D12GraphicsCommandList* pkCommandList = m_kDevice.GetCommandList();
	const UINT uWidth = m_kDevice.GetWidth();
	const UINT uHeight = m_kDevice.GetHeight();

	if (m_pkSceneCopy && (m_uSceneCopyWidth != uWidth || m_uSceneCopyHeight != uHeight))
	{
		if (m_kSceneCopySRV.ptr)
			FreeTextureSRV(m_kSceneCopySRV);
		m_kSceneCopySRV.ptr = 0;
		RetireResource(m_pkSceneCopy);
		m_pkSceneCopy = NULL;
		m_bSceneCopyReadable = false;
	}

	if (!m_pkSceneCopy)
	{
		D3D12_HEAP_PROPERTIES kHeap = {};
		kHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC kDesc = {};
		kDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		kDesc.Width = uWidth;
		kDesc.Height = uHeight;
		kDesc.DepthOrArraySize = 1;
		kDesc.MipLevels = 1;
		kDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		kDesc.SampleDesc.Count = 1;

		if (FAILED(pkDevice->CreateCommittedResource(&kHeap, D3D12_HEAP_FLAG_NONE, &kDesc,
													 D3D12_RESOURCE_STATE_COPY_DEST, NULL,
													 IID_PPV_ARGS(&m_pkSceneCopy))))
			return;

		m_uSceneCopyWidth = uWidth;
		m_uSceneCopyHeight = uHeight;
		m_bSceneCopyReadable = false;

		if (!CreateTextureSRV(m_pkSceneCopy, &m_kSceneCopySRV))
		{
			safe_release(m_pkSceneCopy);
			return;
		}
	}

	if (!m_kGammaPass.IsCreated() && !m_kGammaPass.Create(pkDevice))
		return;

	ID3D12Resource* pkBackBuffer = m_kDevice.GetCurrentBuffer();
	if (!pkBackBuffer)
		return;

	D3D12_RESOURCE_BARRIER akBarriers[2] = {};
	akBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	akBarriers[0].Transition.pResource = pkBackBuffer;
	akBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	akBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	akBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

	UINT uBarrierCount = 1;
	if (m_bSceneCopyReadable)
	{
		akBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		akBarriers[1].Transition.pResource = m_pkSceneCopy;
		akBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		akBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		akBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		uBarrierCount = 2;
	}
	pkCommandList->ResourceBarrier(uBarrierCount, akBarriers);

	pkCommandList->CopyResource(m_pkSceneCopy, pkBackBuffer);

	akBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
	akBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	akBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	akBarriers[1].Transition.pResource = m_pkSceneCopy;
	akBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	akBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	akBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	pkCommandList->ResourceBarrier(2, akBarriers);
	m_bSceneCopyReadable = true;

	CGraphicDescriptorRingDX12::TTable kTable;
	if (!m_kSRVRing.Allocate(1, &kTable))
		return;
	pkDevice->CopyDescriptorsSimple(1, kTable.kCPUHandle, m_kSceneCopySRV, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	m_kGammaPass.Record(pkCommandList, kTable.kGPUHandle, m_fGammaFactor);
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
	ID3DBlob* pkPixelShader = (m_kPipelineKey.m_bAlphaTestEnable && m_apkProgramBytecodeAlphaTest[m_uPixelProgram])
		? m_apkProgramBytecodeAlphaTest[m_uPixelProgram] : m_apkProgramBytecode[m_uPixelProgram];

	D3D12_SHADER_BYTECODE kVSBytecode = { pkVertexShader->GetBufferPointer(), pkVertexShader->GetBufferSize() };
	D3D12_SHADER_BYTECODE kPSBytecode = { pkPixelShader->GetBufferPointer(), pkPixelShader->GetBufferSize() };

	ID3D12PipelineState* pkPipelineState =
		m_kPipelineCache.GetPipelineState(m_kPipelineKey, kVSBytecode, kPSBytecode,
										  m_akInputElements, m_uInputElementCount);
	if (!pkPipelineState)
	{
		static std::set<UINT64> s_kReportedCombos;
		const UINT64 uCombo = (static_cast<UINT64>(m_uVertexProgram) << 40)
			| (static_cast<UINT64>(m_uPixelProgram) << 16) | m_uInputLayoutID;
		if (s_kReportedCombos.size() < 64 && s_kReportedCombos.insert(uCombo).second)
			TraceError("CGraphicBackendDX12: pipeline creation failed (vs=%u ps=%u layout=%u alphaTest=%d).",
					   m_uVertexProgram, m_uPixelProgram, m_uInputLayoutID,
					   m_kPipelineKey.m_bAlphaTestEnable ? 1 : 0);
		return false;
	}

	pkCommandList->SetPipelineState(pkPipelineState);
	pkCommandList->IASetPrimitiveTopology(eTopology);
	return true;
}

bool CGraphicBackendDX12::DrawBuffers(D3D_PRIMITIVE_TOPOLOGY eTopology,
									   const D3D12_VERTEX_BUFFER_VIEW& rkVertexView,
									   UINT uStartVertex, UINT uVertexCount,
									   const D3D12_INDEX_BUFFER_VIEW* pkIndexView,
									   UINT uStartIndex, UINT uIndexCount, INT nBaseVertex)
{
	if (!m_bCreated || !m_bInFrame || !m_akInputElements)
		return false;

	if (!__ApplyState(eTopology))
		return false;

	ID3D12GraphicsCommandList* pkCommandList = m_kDevice.GetCommandList();
	pkCommandList->IASetVertexBuffers(0, 1, &rkVertexView);

	if (pkIndexView)
	{
		pkCommandList->IASetIndexBuffer(pkIndexView);
		pkCommandList->DrawIndexedInstanced(uIndexCount, 1, uStartIndex, nBaseVertex, 0);
	}
	else
	{
		pkCommandList->DrawInstanced(uVertexCount, 1, uStartVertex, 0);
	}

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

UINT64 CGraphicBackendDX12::GetFrameOrdinal() const
{
	return m_uFrameOrdinal;
}

bool CGraphicBackendDX12::UploadVertices(const void* pvVertices, UINT uStrideBytes, UINT uVertexCount,
										 D3D12_VERTEX_BUFFER_VIEW* pkViewOut)
{
	if (!m_bCreated || !m_bInFrame)
		return false;

	return CGraphicDynamicDrawDX12::WriteVertices(m_kUploadRing, pvVertices, uStrideBytes, uVertexCount, pkViewOut);
}

bool CGraphicBackendDX12::UploadIndices(const WORD* awIndices, UINT uIndexCount,
										D3D12_INDEX_BUFFER_VIEW* pkViewOut)
{
	if (!m_bCreated || !m_bInFrame)
		return false;

	return CGraphicDynamicDrawDX12::WriteIndices(m_kUploadRing, awIndices, uIndexCount, pkViewOut);
}
