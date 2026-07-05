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
	safe_release(m_lpd3dTexture);
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
		TraceError("CGraphicTexture: no DX12 format for %u; texture stays DX9-only.",
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

	m_pkTextureDX12 = pkBackend->GetUploader().CreateTexture2D(pkBackend->GetDevice().GetCommandQueue(),
															   uWidth, uHeight, eFormatDX12,
															   pvUpload, uUploadPitch);
	if (!m_pkTextureDX12)
	{
		TraceError("CGraphicTexture: DX12 twin creation failed (%ux%u fmt %u).",
				   uWidth, uHeight, static_cast<unsigned>(eFormat));
		return true;
	}

	if (!pkBackend->CreateTextureSRV(m_pkTextureDX12, &m_kSRVHandleDX12))
	{
		safe_release(m_pkTextureDX12);
		m_kSRVHandleDX12.ptr = 0;
		return true;
	}

	return true;
}

void CGraphicTexture::DestroyDX12Twin()
{
	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (pkBackend && m_kSRVHandleDX12.ptr)
		pkBackend->FreeTextureSRV(m_kSRVHandleDX12);

	m_kSRVHandleDX12.ptr = 0;
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
