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

		bool HasDX12Twin() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandleDX12() const;

		void DestroyDeviceObjects();

	protected:
		CGraphicTexture();
		virtual	~CGraphicTexture();

		void Destroy();
		void Initialize();

		bool CreateDX12Twin(UINT uWidth, UINT uHeight, D3DFORMAT eFormat,
							const void* pvPixels, UINT uSrcRowPitch);
		bool CreateTwinFromLevels(UINT uWidth, UINT uHeight, D3DFORMAT eFormat,
								  const struct TTextureLevelData* akLevels, UINT uLevelCount);
		void DestroyDX12Twin();

	private:
		bool __UploadTwinLevels(UINT uWidth, UINT uHeight, DXGI_FORMAT eFormatDX12,
									const struct TTextureLevelData* akLevels, UINT uLevelCount);

	protected:

	protected:
		bool m_bEmpty;

		int m_width;
		int m_height;

		LPDIRECT3DTEXTURE9 m_lpd3dTexture;

		ID3D12Resource* m_pkTextureDX12;
		D3D12_CPU_DESCRIPTOR_HANDLE m_kSRVHandleDX12;
};
