#include "StdAfx.h"
#include "BlockTexture.h"
#include "GrpBase.h"
#include "GrpDib.h"
#include "GrpBackendDX12.h"
#include "../eterbase/Stl.h"
#include "../eterlib/StateManager.h"

void CBlockTexture::SetClipRect(const RECT & c_rRect)
{
	m_bClipEnable = TRUE;
	m_clipRect = c_rRect;
}

void CBlockTexture::Render(int ix, int iy)
{
	int isx = ix + m_rect.left;
	int isy = iy + m_rect.top;
	int iex = ix + m_rect.left + m_dwWidth;
	int iey = iy + m_rect.top + m_dwHeight;

	float su = 0.0f;
	float sv = 0.0f;
	float eu = 1.0f;
	float ev = 1.0f;

	if (m_bClipEnable)
	{
		if (isx > m_clipRect.right)
			return;
		if (iex < m_clipRect.left)
			return;

		if (isy > m_clipRect.bottom)
			return;
		if (iey < m_clipRect.top)
			return;

		if (m_clipRect.left > isx)
		{
			int idx = m_clipRect.left - isx;
			isx += idx;
			su += float(idx) / float(m_dwWidth);
		}
		if (iex > m_clipRect.right)
		{
			int idx = iex - m_clipRect.right;
			iex -= idx;
			eu -= float(idx) / float(m_dwWidth);
		}

		if (m_clipRect.top > isy)
		{
			int idy = m_clipRect.top - isy;
			isy += idy;
			sv += float(idy) / float(m_dwHeight);
		}
		if (iey > m_clipRect.bottom)
		{
			int idy = iey - m_clipRect.bottom;
			iey -= idy;
			ev -= float(idy) / float(m_dwHeight);
		}
	}

	TPDTVertex vertices[4];
	vertices[0].position.x	= float(isx);
	vertices[0].position.y	= float(isy);
	vertices[0].position.z	= 0.0f;
	vertices[0].texCoord	= TTextureCoordinate(su, sv);
	vertices[0].diffuse		= 0xffffffff;

	vertices[1].position.x	= float(iex);
	vertices[1].position.y	= float(isy);
	vertices[1].position.z	= 0.0f;
	vertices[1].texCoord	= TTextureCoordinate(eu, sv);
	vertices[1].diffuse		= 0xffffffff;

	vertices[2].position.x	= float(isx);
	vertices[2].position.y	= float(iey);
	vertices[2].position.z	= 0.0f;
	vertices[2].texCoord	= TTextureCoordinate(su, ev);
	vertices[2].diffuse		= 0xffffffff;

	vertices[3].position.x	= float(iex);
	vertices[3].position.y	= float(iey);
	vertices[3].position.z	= 0.0f;
	vertices[3].texCoord	= TTextureCoordinate(eu, ev);
	vertices[3].diffuse		= 0xffffffff;

	if (CGraphicBase::SetPDTStream(vertices, 4))
	{
		CGraphicBase::SetDefaultIndexBuffer(CGraphicBase::DEFAULT_IB_FILL_RECT);

		STATEMANAGER.SetTexture(0, m_lpd3dTexture);
		STATEMANAGER.SetTexture(1, NULL);
		if (CGraphicBase::BeginPDTShader())
		{
			STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4, 0, 2);
			CGraphicBase::EndPDTShader();
		}
		else
		{
			STATEMANAGER.SetFVF(D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
			STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4, 0, 2);
		}
	}
}

void CBlockTexture::InvalidateRect(const RECT & c_rsrcRect)
{
	RECT dstRect = m_rect;
	if (c_rsrcRect.right < dstRect.left ||
		c_rsrcRect.left > dstRect.right ||
		c_rsrcRect.bottom < dstRect.top ||
		c_rsrcRect.top > dstRect.bottom)
	{
		Tracef("InvalidateRect() - Strange rect");
		return;
	}


	// DIBBAR_LONGSIZE_BUGFIX
	const RECT clipRect = { 				
		max(c_rsrcRect.left - dstRect.left, 0),
		max(c_rsrcRect.top - dstRect.top, 0),
		min(c_rsrcRect.right - dstRect.left, dstRect.right - dstRect.left),
		min(c_rsrcRect.bottom - dstRect.top, dstRect.bottom - dstRect.top),
	};
	// END_OF_DIBBAR_LONGSIZE_BUGFIX


	if (m_kPixels.empty())
		return;

	DWORD * pdwSrc;
	pdwSrc = (DWORD *)m_pDIB->GetPointer();
	pdwSrc += dstRect.left + dstRect.top*m_pDIB->GetWidth();

	int iclipWidth = clipRect.right - clipRect.left;
	int iclipHeight = clipRect.bottom - clipRect.top;
	DWORD * pdwDst = &m_kPixels[0] + static_cast<size_t>(clipRect.top) * m_dwWidth + clipRect.left;
	DWORD dwDstWidth = m_dwWidth;
	DWORD dwSrcWidth = m_pDIB->GetWidth();
	for (int i = 0; i < iclipHeight; ++i)
	{
		for (int i = 0; i < iclipWidth; ++i)
		{
			if (pdwSrc[i])
				pdwDst[i] = pdwSrc[i] | 0xff000000;
			else
				pdwDst[i] = 0;
		}
		pdwDst += dwDstWidth;
		pdwSrc += dwSrcWidth;
	}

	__RegisterTwin();
}

void CBlockTexture::__RegisterTwin()
{
	if (m_kPixels.empty() || !m_lpd3dTexture)
		return;

	if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
	{
		TTextureLevelData kLevel;
		kLevel.pvPixels = &m_kPixels[0];
		kLevel.uRowPitch = m_dwWidth * 4;
		if (!pkBackend->RegisterRawTextureTwin(m_lpd3dTexture, m_dwWidth, m_dwHeight,
											   DXGI_FORMAT_B8G8R8A8_UNORM, &kLevel, 1))
			TraceError("CBlockTexture::__RegisterTwin - RegisterRawTextureTwin failed %ux%u", m_dwWidth, m_dwHeight);
	}
}

bool CBlockTexture::Create(CGraphicDib * pDIB, const RECT & c_rRect, DWORD dwWidth, DWORD dwHeight)
{
	m_pDIB = pDIB;
	m_rect = c_rRect;
	m_dwWidth = dwWidth;
	m_dwHeight = dwHeight;
	m_bClipEnable = FALSE;

	m_kPixels.assign(static_cast<size_t>(dwWidth) * dwHeight, 0);
	m_lpd3dTexture = (LPDIRECT3DTEXTURE9)this;

	__RegisterTwin();

	return true;
}

CBlockTexture::CBlockTexture()
{
	m_pDIB = NULL;
	m_lpd3dTexture = NULL;
}

CBlockTexture::~CBlockTexture()
{
	if (m_lpd3dTexture)
	{
		if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
			pkBackend->UnregisterRawTextureTwin(m_lpd3dTexture);
	}
	m_lpd3dTexture = NULL;
}
