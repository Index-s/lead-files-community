#pragma once

// The DX12 backend spine: owns the device, upload/descriptor rings, PSO and
// sampler caches and the SM5 builds of the pool programs, and turns mirrored
// draw state into command-list work. The state manager routes here once
// BACKEND_DX12 goes live; until then it compiles but stays unreachable.

#include <d3d12.h>
#include <unordered_map>

#include "GrpDeviceDX12.h"
#include "GrpUploadRingDX12.h"
#include "GrpDescriptorRingDX12.h"
#include "GrpRootSignatureDX12.h"
#include "GrpPipelineKeyDX12.h"
#include "GrpPipelineCacheDX12.h"
#include "GrpSamplerKeyDX12.h"
#include "GrpSamplerCacheDX12.h"
#include "GrpConstantShadowDX12.h"
#include "GrpResourceUploadDX12.h"
#include "GrpCPUDescriptorsDX12.h"

class CGraphicBackendDX12
{
	public:
		enum
		{
			TEXTURE_STAGE_COUNT = 2,
			UPLOAD_RING_BYTES = 16 * 1024 * 1024,
			SRV_RING_CAPACITY = 4096,
			SAMPLER_TABLE_CAPACITY = 64,
		};

		CGraphicBackendDX12();
		~CGraphicBackendDX12();

		// The live backend, or NULL while rendering runs on DX9: texture
		// loaders use this to decide whether to build their DX12 twins.
		static CGraphicBackendDX12*	GetInstance();

		bool	Create(HWND hWnd, UINT uWidth, UINT uHeight, bool bWindowed);
		void	Destroy();
		bool	IsCreated() const;

		CGraphicDeviceDX12&				GetDevice();
		CGraphicResourceUploaderDX12&	GetUploader();

		// Persistent SRV slots for textures (backed by the CPU allocator).
		bool	CreateTextureSRV(ID3D12Resource* pkTexture, D3D12_CPU_DESCRIPTOR_HANDLE* pkHandleOut);
		void	FreeTextureSRV(D3D12_CPU_DESCRIPTOR_HANDLE kHandle);

		bool	BeginFrame(DWORD dwClearColor);
		bool	EndFrame();

		// Offscreen render-target twins (character shadow map). Registered
		// by texture-creation hooks; SetRenderTargetTexture redirects the
		// output merger, RestoreDefaultTarget returns to the backbuffer and
		// flips the twin to shader-readable for the receive pass.
		bool	RegisterRenderTarget(const void* pkTextureD3D9, UINT uWidth, UINT uHeight);
		bool	SetRenderTargetTexture(const void* pkTextureD3D9);
		void	RestoreDefaultTarget();
		bool	IsRenderTarget(const void* pkTextureD3D9) const;
		D3D12_CPU_DESCRIPTOR_HANDLE	GetRenderTargetSRV(const void* pkTextureD3D9) const;

		void	SetViewport(const D3DVIEWPORT9& rkViewport);

		// Mid-frame clear of the bound targets (dwFlags = D3DCLEAR_* bits).
		void	ClearTargets(DWORD dwFlags, DWORD dwColor, float fDepth, DWORD dwStencil);

		// --- mirrored draw state ---
		// Program indices follow CGraphicShaderPool::GetProgramInfo order.
		bool	SetProgram(UINT uVertexProgramIndex, UINT uPixelProgramIndex);
		void	SetInputLayout(UINT uLayoutID, const D3D12_INPUT_ELEMENT_DESC* akElements, UINT uElementCount);
		// Unbound stages sample the built-in white texture.
		void	SetTextureSRV(UINT uStage, D3D12_CPU_DESCRIPTOR_HANDLE kSRVHandle);
		void	ClearTextureSRV(UINT uStage);
		void	SetSamplerKey(UINT uStage, const CGraphicSamplerKeyDX12& rkKey);
		// Render-state portion only; shader/layout identity stays internal.
		void	SetPipelineStates(const CGraphicPipelineKeyDX12& rkKey);
		bool	SetVSConstants(UINT uStartRegister, const float* afData, UINT uVector4Count);
		bool	SetPSConstants(UINT uStartRegister, const float* afData, UINT uVector4Count);

		// Static-buffer draw; pkIndexView NULL = non-indexed.
		bool	DrawBuffers(D3D_PRIMITIVE_TOPOLOGY eTopology,
							const D3D12_VERTEX_BUFFER_VIEW& rkVertexView,
							UINT uStartVertex, UINT uVertexCount,
							const D3D12_INDEX_BUFFER_VIEW* pkIndexView,
							UINT uStartIndex, UINT uIndexCount, INT nBaseVertex);

		// UP-style draw through the upload ring; awIndices NULL = non-indexed.
		bool	DrawTransient(D3D_PRIMITIVE_TOPOLOGY eTopology,
							  const void* pvVertices, UINT uVertexCount, UINT uStrideBytes,
							  const WORD* awIndices, UINT uIndexCount);

		static UINT	ToTopologyTypeDX12(D3D_PRIMITIVE_TOPOLOGY eTopology);

	private:
		bool	__CompilePrograms();
		bool	__CreateWhiteTexture();
		bool	__ApplyState(D3D_PRIMITIVE_TOPOLOGY eTopology);

		CGraphicDeviceDX12				m_kDevice;
		CGraphicRootSignatureDX12		m_kRootSignature;
		CGraphicUploadRingDX12			m_kUploadRing;
		CGraphicDescriptorRingDX12		m_kSRVRing;
		CGraphicSamplerCacheDX12		m_kSamplerCache;
		CGraphicPipelineCacheDX12		m_kPipelineCache;
		CGraphicConstantShadowDX12		m_kConstantShadow;
		CGraphicResourceUploaderDX12	m_kUploader;
		CGraphicCPUDescriptorsDX12		m_kTextureDescriptors;

		static CGraphicBackendDX12*		ms_pkInstance;

		ID3DBlob**						m_apkProgramBytecode;
		ID3DBlob**						m_apkProgramBytecodeAlphaTest;
		UINT							m_uProgramCount;

		CGraphicPipelineKeyDX12			m_kPipelineKey;
		CGraphicSamplerKeyDX12			m_akSamplerKeys[TEXTURE_STAGE_COUNT];
		D3D12_CPU_DESCRIPTOR_HANDLE		m_akTextureSRVs[TEXTURE_STAGE_COUNT];
		const D3D12_INPUT_ELEMENT_DESC*	m_akInputElements;
		UINT							m_uInputElementCount;
		UINT							m_uInputLayoutID;
		UINT							m_uVertexProgram;
		UINT							m_uPixelProgram;
		UINT							m_uSRVIncrementSize;

		ID3D12Resource*					m_pkWhiteTexture;
		ID3D12DescriptorHeap*			m_pkCPUHeap;
		CGraphicCPUDescriptorsDX12		m_kRTVDescriptors;
		CGraphicCPUDescriptorsDX12		m_kDSVDescriptors;

		struct TRenderTargetDX12
		{
			ID3D12Resource*				pkColor;
			ID3D12Resource*				pkDepth;
			D3D12_CPU_DESCRIPTOR_HANDLE	kRTV;
			D3D12_CPU_DESCRIPTOR_HANDLE	kDSV;
			D3D12_CPU_DESCRIPTOR_HANDLE	kSRV;
			UINT						uWidth;
			UINT						uHeight;
			bool						bShaderReadable;
		};
		std::unordered_map<const void*, TRenderTargetDX12>	m_kRenderTargetMap;
		TRenderTargetDX12*				m_pkBoundRenderTarget;
		D3D12_CPU_DESCRIPTOR_HANDLE		m_kCurrentRTV;
		D3D12_CPU_DESCRIPTOR_HANDLE		m_kCurrentDSV;
		bool							m_bInFrame;
		D3D12_CPU_DESCRIPTOR_HANDLE		m_kWhiteSRV;
		bool							m_bCreated;
};
