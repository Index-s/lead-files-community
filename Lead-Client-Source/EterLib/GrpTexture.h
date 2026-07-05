#pragma once

#include <d3d12.h>

#include "GrpBase.h"

class CGraphicTexture : public CGraphicBase
{
	public:
		virtual bool IsEmpty() const;

		int GetWidth() const;
		int GetHeight() const;

		void SetTextureStage(int stage) const;
		LPDIRECT3DTEXTURE9 GetD3DTexture() const;

		// DX12 twin, built beside the D3D9 texture while the backend is live.
		bool HasDX12Twin() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandleDX12() const;

		void DestroyDeviceObjects();

	protected:
		CGraphicTexture();
		virtual	~CGraphicTexture();

		void Destroy();
		void Initialize();

		// Builds the twin from source pixels (top mip only); no-op without a
		// live backend, non-fatal on failure - the texture stays DX9-only.
		bool CreateDX12Twin(UINT uWidth, UINT uHeight, D3DFORMAT eFormat,
							const void* pvPixels, UINT uSrcRowPitch);
		void DestroyDX12Twin();

	protected:
		bool m_bEmpty;

		int m_width;
		int m_height;

		LPDIRECT3DTEXTURE9 m_lpd3dTexture;

		ID3D12Resource* m_pkTextureDX12;
		D3D12_CPU_DESCRIPTOR_HANDLE m_kSRVHandleDX12;
};
