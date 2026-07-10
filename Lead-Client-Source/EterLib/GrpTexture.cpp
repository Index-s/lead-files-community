#include "StdAfx.h"
#include <vector>
#include "../eterBase/Stl.h"
#include "GrpTexture.h"
#include "GrpBackendDX12.h"
#include "GrpFormatDX12.h"
#include "StateManager.h"

void CGraphicTexture::DestroyDeviceObjects()
{
	DestroyDX12Twin();
	m_lpd3dTexture = NULL;
}

void CGraphicTexture::Destroy()
{
	DestroyDeviceObjects();

	Initialize();
}

void CGraphicTexture::Initialize()
{
	m_lpd3dTexture = NULL;
	m_pkTextureDX12 = NULL;
	m_kSRVHandleDX12.ptr = 0;
	m_width = 0;
	m_height = 0;
	m_bEmpty = true;
}

bool CGraphicTexture::IsEmpty() const
{
	return m_bEmpty;
}

void CGraphicTexture::SetTextureStage(int stage) const
{
	assert(IsDeviceCreated());
	STATEMANAGER.SetTexture(stage, m_lpd3dTexture);

	// DX12-native bind (D3D9-removal stage 1): the texture object carries its
	// own shader-resource view, so the renderer resolves it directly instead
	// of looking it up from the D3D9 texture pointer. This is the path that
	// survives once the D3D9 texture object is gone.
	if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
	{
		if (m_pkTextureDX12)
			pkBackend->SetTextureSRV(stage, m_kSRVHandleDX12);
		else
			pkBackend->ClearTextureSRV(stage);
	}
}

LPDIRECT3DTEXTURE9 CGraphicTexture::GetD3DTexture() const
{
	return m_lpd3dTexture;
}

bool CGraphicTexture::HasDX12Twin() const
{
	return NULL != m_pkTextureDX12;
}

D3D12_CPU_DESCRIPTOR_HANDLE CGraphicTexture::GetSRVHandleDX12() const
{
	return m_kSRVHandleDX12;
}

bool CGraphicTexture::CreateDX12Twin(UINT uWidth, UINT uHeight, D3DFORMAT eFormat,
									 const void* pvPixels, UINT uSrcRowPitch)
{
	DestroyDX12Twin();

	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (!pkBackend || !pvPixels || !uWidth || !uHeight)
		return true;

	bool bNeedsWidening = false;
	const DXGI_FORMAT eFormatDX12 = CGraphicFormatDX12::ToTextureFormatDX12(eFormat, &bNeedsWidening);
	if (DXGI_FORMAT_UNKNOWN == eFormatDX12)
	{
		TraceError("CGraphicTexture: no DX12 format for %u.",
				   static_cast<unsigned>(eFormat));
		return true;
	}

	std::vector<DWORD> kWidened;
	const void* pvUpload = pvPixels;
	UINT uUploadPitch = uSrcRowPitch;

	if (bNeedsWidening)
	{
		kWidened.resize(static_cast<size_t>(uWidth) * uHeight);
		for (UINT uRow = 0; uRow != uHeight; ++uRow)
		{
			const void* pvSourceRow = static_cast<const BYTE*>(pvPixels) + static_cast<size_t>(uRow) * uSrcRowPitch;
			DWORD* adwDestRow = &kWidened[static_cast<size_t>(uRow) * uWidth];

			switch (eFormat)
			{
				case D3DFMT_R5G6B5:
					CGraphicFormatDX12::WidenR5G6B5(pvSourceRow, uWidth, adwDestRow);
					break;
				case D3DFMT_A1R5G5B5:
					CGraphicFormatDX12::WidenA1R5G5B5(pvSourceRow, uWidth, adwDestRow);
					break;
				case D3DFMT_X1R5G5B5:
					CGraphicFormatDX12::WidenA1R5G5B5(pvSourceRow, uWidth, adwDestRow);
					for (UINT uCol = 0; uCol != uWidth; ++uCol)
						adwDestRow[uCol] |= 0xFF000000;
					break;
				case D3DFMT_A4R4G4B4:
					CGraphicFormatDX12::WidenA4R4G4B4(pvSourceRow, uWidth, adwDestRow);
					break;
				default:
					return true;
			}
		}
		pvUpload = &kWidened[0];
		uUploadPitch = uWidth * 4;
	}

	TTextureLevelData kLevel;
	kLevel.pvPixels = pvUpload;
	kLevel.uRowPitch = uUploadPitch;
	return __UploadTwinLevels(uWidth, uHeight, eFormatDX12, &kLevel, 1);
}

bool CGraphicTexture::CreateTwinFromLevels(UINT uWidth, UINT uHeight, D3DFORMAT eFormat,
										   const TTextureLevelData* akLevels, UINT uLevelCount)
{
	if (!akLevels || !uLevelCount)
		return true;

	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (!pkBackend || !uWidth || !uHeight)
	{
		DestroyDX12Twin();
		return true;
	}

	bool bNeedsWidening = false;
	const DXGI_FORMAT eFormatDX12 = CGraphicFormatDX12::ToTextureFormatDX12(eFormat, &bNeedsWidening);
	if (DXGI_FORMAT_UNKNOWN == eFormatDX12)
	{
		DestroyDX12Twin();
		TraceError("CGraphicTexture: no DX12 format for %u.",
				   static_cast<unsigned>(eFormat));
		return true;
	}

	// 16bpp sources widen row-by-row; route those through the top-mip path.
	if (bNeedsWidening)
		return CreateDX12Twin(uWidth, uHeight, eFormat, akLevels[0].pvPixels, akLevels[0].uRowPitch);

	DestroyDX12Twin();
	return __UploadTwinLevels(uWidth, uHeight, eFormatDX12, akLevels, uLevelCount);
}

bool CGraphicTexture::__UploadTwinLevels(UINT uWidth, UINT uHeight, DXGI_FORMAT eFormatDX12,
											 const TTextureLevelData* akLevels, UINT uLevelCount)
{
	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (!pkBackend)
		return true;

	m_pkTextureDX12 = pkBackend->GetUploader().CreateTexture2D(pkBackend->GetDevice().GetCommandQueue(),
															   uWidth, uHeight, eFormatDX12,
															   akLevels, uLevelCount);
	if (!m_pkTextureDX12)
	{
		TraceError("CGraphicTexture: DX12 twin creation failed (%ux%u fmt %d mips %u).",
				   uWidth, uHeight, static_cast<int>(eFormatDX12), uLevelCount);
		return true;
	}

	if (!pkBackend->CreateTextureSRV(m_pkTextureDX12, &m_kSRVHandleDX12))
	{
		safe_release(m_pkTextureDX12);
		m_kSRVHandleDX12.ptr = 0;
		return true;
	}

	if (m_lpd3dTexture && CStateManager::InstancePtr())
		STATEMANAGER.RegisterTextureSRVDX12(m_lpd3dTexture, m_kSRVHandleDX12);

	return true;
}

void CGraphicTexture::DestroyDX12Twin()
{
	if (m_lpd3dTexture && m_kSRVHandleDX12.ptr && CStateManager::InstancePtr())
		STATEMANAGER.UnregisterTextureSRVDX12(m_lpd3dTexture);

	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (pkBackend && m_kSRVHandleDX12.ptr)
		pkBackend->FreeTextureSRV(m_kSRVHandleDX12);

	m_kSRVHandleDX12.ptr = 0;
	if (pkBackend)
	{
		pkBackend->RetireResource(m_pkTextureDX12);
		m_pkTextureDX12 = NULL;
	}
	else
		safe_release(m_pkTextureDX12);
}

int CGraphicTexture::GetWidth() const
{
	return m_width;
}

int CGraphicTexture::GetHeight() const
{
	return m_height;
}

CGraphicTexture::CGraphicTexture()
{
	Initialize();
}

CGraphicTexture::~CGraphicTexture()
{
}
