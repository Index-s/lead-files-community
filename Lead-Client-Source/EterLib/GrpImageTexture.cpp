#include "StdAfx.h"
#include "../eterBase/MappedFile.h"
#include "../eterPack/EterPackManager.h"
#include "GrpImageTexture.h"
#include "GrpBackendDX12.h"
#include "StateManager.h"
#include "GrpFormatDX12.h"
#include "ImageFileDecoder.h"

namespace
{
	// D3DX-style colorkey: pixels matching the ARGB key become transparent black.
	void ApplyColorKey(SDecodedImage& rkImage, DWORD dwARGBColorKey)
	{
		const BYTE byA = BYTE(dwARGBColorKey >> 24);
		const BYTE byR = BYTE(dwARGBColorKey >> 16);
		const BYTE byG = BYTE(dwARGBColorKey >> 8);
		const BYTE byB = BYTE(dwARGBColorKey);

		BYTE* pbyPixel = rkImage.kPixels.data();
		BYTE* pbyEnd = pbyPixel + rkImage.kPixels.size();
		for (; pbyPixel < pbyEnd; pbyPixel += 4)
		{
			if (pbyPixel[0] == byB && pbyPixel[1] == byG && pbyPixel[2] == byR && pbyPixel[3] == byA)
			{
				pbyPixel[0] = 0;
				pbyPixel[1] = 0;
				pbyPixel[2] = 0;
				pbyPixel[3] = 0;
			}
		}
	}

	// 2x2 box filter (clamped at odd edges) - equivalent of D3DX_FILTER_LINEAR mips.
	void DownsampleBox(const std::vector<BYTE>& c_rkSrc, UINT uSrcWidth, UINT uSrcHeight,
					   std::vector<BYTE>& rkDst, UINT uDstWidth, UINT uDstHeight)
	{
		rkDst.resize(size_t(uDstWidth) * uDstHeight * 4);

		for (UINT y = 0; y < uDstHeight; ++y)
		{
			const UINT uSrcY0 = y * 2;
			const UINT uSrcY1 = (uSrcY0 + 1 < uSrcHeight) ? uSrcY0 + 1 : uSrcY0;
			for (UINT x = 0; x < uDstWidth; ++x)
			{
				const UINT uSrcX0 = x * 2;
				const UINT uSrcX1 = (uSrcX0 + 1 < uSrcWidth) ? uSrcX0 + 1 : uSrcX0;

				const BYTE* pbySrc00 = &c_rkSrc[(size_t(uSrcY0) * uSrcWidth + uSrcX0) * 4];
				const BYTE* pbySrc01 = &c_rkSrc[(size_t(uSrcY0) * uSrcWidth + uSrcX1) * 4];
				const BYTE* pbySrc10 = &c_rkSrc[(size_t(uSrcY1) * uSrcWidth + uSrcX0) * 4];
				const BYTE* pbySrc11 = &c_rkSrc[(size_t(uSrcY1) * uSrcWidth + uSrcX1) * 4];

				BYTE* pbyDst = &rkDst[(size_t(y) * uDstWidth + x) * 4];
				for (UINT c = 0; c < 4; ++c)
					pbyDst[c] = BYTE((UINT(pbySrc00[c]) + pbySrc01[c] + pbySrc10[c] + pbySrc11[c] + 2) / 4);
			}
		}
	}

	bool BuildImageLevels(const SDecodedImage& c_rkImage, std::vector<SDecodedImage>& rkLevels)
	{
		if (!c_rkImage.uWidth || !c_rkImage.uHeight)
			return false;

		UINT uMipCount = 1;
		{
			UINT uWidth = c_rkImage.uWidth;
			UINT uHeight = c_rkImage.uHeight;
			while (uWidth > 1 || uHeight > 1)
			{
				uWidth = uWidth > 1 ? uWidth / 2 : 1;
				uHeight = uHeight > 1 ? uHeight / 2 : 1;
				++uMipCount;
			}
		}

		std::vector<BYTE> kLevelPixels = c_rkImage.kPixels;
		std::vector<BYTE> kNextPixels;
		UINT uLevelWidth = c_rkImage.uWidth;
		UINT uLevelHeight = c_rkImage.uHeight;

		for (UINT uLevel = 0; uLevel < uMipCount; ++uLevel)
		{
			rkLevels.push_back(SDecodedImage());
			SDecodedImage& rkLevel = rkLevels.back();
			rkLevel.uWidth = uLevelWidth;
			rkLevel.uHeight = uLevelHeight;
			rkLevel.kPixels = kLevelPixels;

			if (uLevel + 1 < uMipCount)
			{
				const UINT uNextWidth = uLevelWidth > 1 ? uLevelWidth / 2 : 1;
				const UINT uNextHeight = uLevelHeight > 1 ? uLevelHeight / 2 : 1;
				DownsampleBox(kLevelPixels, uLevelWidth, uLevelHeight, kNextPixels, uNextWidth, uNextHeight);
				kLevelPixels.swap(kNextPixels);
				uLevelWidth = uNextWidth;
				uLevelHeight = uNextHeight;
			}
		}

		return true;
	}

	UINT GetFormatBytesPerPixel(D3DFORMAT eFormat)
	{
		switch (eFormat)
		{
			case D3DFMT_A8R8G8B8:
			case D3DFMT_X8R8G8B8:
				return 4;
			case D3DFMT_A4R4G4B4:
			case D3DFMT_A1R5G5B5:
			case D3DFMT_R5G6B5:
				return 2;
			default:
				TraceError("CGraphicImageTexture: unexpected canvas format %u",
						   static_cast<unsigned>(eFormat));
				return 4;
		}
	}
}

bool CGraphicImageTexture::Lock(int* pRetPitch, void** ppRetPixels, int level)
{
	if (0 != level)
		return false;

	if (m_kCanvas.empty())
		return false;

	*pRetPitch = m_width * static_cast<int>(GetFormatBytesPerPixel(m_d3dFmt));
	*ppRetPixels = &m_kCanvas[0];
	return true;
}

void CGraphicImageTexture::Unlock(int level)
{
	assert(m_lpd3dTexture != NULL);

	if (0 != level || m_kCanvas.empty())
		return;

	CreateDX12Twin(m_width, m_height, m_d3dFmt, &m_kCanvas[0],
				   m_width * GetFormatBytesPerPixel(m_d3dFmt));
	if (m_lpd3dTexture && HasDX12Twin() && CStateManager::InstancePtr())
		STATEMANAGER.RegisterTextureSRVDX12(m_lpd3dTexture, GetSRVHandleDX12());
}

void CGraphicImageTexture::Initialize()
{
	CGraphicTexture::Initialize();

	m_stFileName = "";

	m_d3dFmt=D3DFMT_UNKNOWN;
	m_dwFilter=0;
	std::vector<BYTE>().swap(m_kCanvas);
}

void CGraphicImageTexture::Destroy()
{
	CGraphicTexture::Destroy();

	Initialize();
}

bool CGraphicImageTexture::CreateDeviceObjects()
{
	assert(IsDeviceCreated());
	assert(m_lpd3dTexture == NULL);

	if (m_stFileName.empty())
	{
		const UINT uBytesPerPixel = GetFormatBytesPerPixel(m_d3dFmt);
		m_kCanvas.assign(static_cast<size_t>(m_width) * m_height * uBytesPerPixel, 0);
		m_lpd3dTexture = (LPDIRECT3DTEXTURE9)this;
	}
	else
	{
		CMappedFile	mappedFile;
		LPCVOID		c_pvMap;

		if (!CEterPackManager::Instance().Get(mappedFile, m_stFileName.c_str(), &c_pvMap))
			return false;

		if (!CreateFromMemoryFile(mappedFile.Size(), c_pvMap, m_d3dFmt, m_dwFilter))
		{
			TraceError("CGraphicImageTexture::CreateDeviceObjects - texture not found(%s)", m_stFileName.c_str());
			return false;
		}

		return true;
	}

	m_bEmpty = false;
	return true;
}

bool CGraphicImageTexture::Create(UINT width, UINT height, D3DFORMAT d3dFmt, DWORD dwFilter)
{
	assert(IsDeviceCreated());
	Destroy();

	m_width = width;
	m_height = height;
	m_d3dFmt = d3dFmt;
	m_dwFilter = dwFilter;

	return CreateDeviceObjects();
}

void CGraphicImageTexture::CreateFromTexturePointer(const CGraphicTexture * c_pSrcTexture)
{
	m_width = c_pSrcTexture->GetWidth();
	m_height = c_pSrcTexture->GetHeight();
	m_lpd3dTexture = c_pSrcTexture->GetD3DTexture();

	m_bEmpty = false;
}

bool CGraphicImageTexture::CreateDDSTexture(CDXTCImage & image, const BYTE * /*c_pbBuf*/)
{
	int mipmapCount = image.m_dwMipMapCount == 0 ? 1 : image.m_dwMipMapCount;

	D3DFORMAT format;

	if(image.m_CompFormat == PF_DXT5)
		format = D3DFMT_DXT5;
	else if(image.m_CompFormat == PF_DXT4)
		format = D3DFMT_DXT4;
	else if(image.m_CompFormat == PF_DXT3)
		format = D3DFMT_DXT3;
	else if(image.m_CompFormat == PF_DXT2)
		format = D3DFMT_DXT2;
	else
		format = D3DFMT_DXT1;

	m_width = image.m_nWidth;
	m_height = image.m_nHeight;
	m_lpd3dTexture = (LPDIRECT3DTEXTURE9)this;

	bool bWiden = false;
	const DXGI_FORMAT eFormatDX12 = CGraphicFormatDX12::ToTextureFormatDX12(format, &bWiden);
	if (DXGI_FORMAT_UNKNOWN != eFormatDX12)
	{
		const UINT uLevelCount = static_cast<UINT>(mipmapCount);
		std::vector<std::vector<BYTE> > kLevelStore(uLevelCount);
		std::vector<TTextureLevelData> kLevels(uLevelCount);

		UINT uLevelWidth = image.m_nWidth;
		UINT uLevelHeight = image.m_nHeight;
		bool bCopied = true;
		for (UINT i = 0; i != uLevelCount && bCopied; ++i)
		{
			const UINT uPitch = CGraphicFormatDX12::GetRowPitch(eFormatDX12, uLevelWidth);
			const UINT uRows = CGraphicFormatDX12::GetRowCount(eFormatDX12, uLevelHeight);
			kLevelStore[i].resize(static_cast<size_t>(uPitch) * uRows);
			bCopied = image.Copy(static_cast<int>(i), &kLevelStore[i][0], uPitch);
			kLevels[i].pvPixels = &kLevelStore[i][0];
			kLevels[i].uRowPitch = uPitch;
			uLevelWidth = uLevelWidth > 1 ? uLevelWidth / 2 : 1;
			uLevelHeight = uLevelHeight > 1 ? uLevelHeight / 2 : 1;
		}

		if (bCopied)
			CreateTwinFromLevels(image.m_nWidth, image.m_nHeight, format,
								 &kLevels[0], uLevelCount);
	}

	if (!HasDX12Twin())
	{
		TraceError("CreateDDSTexture: Cannot create texture (%dx%d fmt %u mips %d)",
				   image.m_nWidth, image.m_nHeight, static_cast<unsigned>(format), mipmapCount);
		Destroy();
		return false;
	}

	m_bEmpty = false;
	return true;
}

bool CGraphicImageTexture::CreateFromMemoryFile(UINT bufSize, const void * c_pvBuf, D3DFORMAT d3dFmt, DWORD dwFilter)
{
	assert(IsDeviceCreated());
	assert(m_lpd3dTexture == NULL);

	static CDXTCImage image;

	if (image.LoadHeaderFromMemory((const BYTE *) c_pvBuf))	// Check if it is DDS
	{
		return (CreateDDSTexture(image, (const BYTE *) c_pvBuf));
	}
	else
	{
		if (D3DFMT_UNKNOWN != d3dFmt && D3DFMT_A8R8G8B8 != d3dFmt)
		{
			TraceError("CreateFromMemoryFile: Unsupported format request %u", d3dFmt);
			return false;
		}

		SDecodedImage kImage;
		if (!DecodeImageFileFromMemory(c_pvBuf, bufSize, &kImage))
		{
			TraceError("CreateFromMemoryFile: Cannot create texture");
			return false;
		}

		ApplyColorKey(kImage, 0xffff00ff);

		std::vector<SDecodedImage> kTwinLevels;
		if (!BuildImageLevels(kImage, kTwinLevels))
		{
			TraceError("CreateFromMemoryFile: Cannot create texture");
			return false;
		}

		m_width = kImage.uWidth;
		m_height = kImage.uHeight;
		m_lpd3dTexture = (LPDIRECT3DTEXTURE9)this;

		std::vector<TTextureLevelData> kLevels(kTwinLevels.size());
		for (size_t i = 0; i != kTwinLevels.size(); ++i)
		{
			kLevels[i].pvPixels = &kTwinLevels[i].kPixels[0];
			kLevels[i].uRowPitch = kTwinLevels[i].uWidth * 4;
		}
		CreateTwinFromLevels(kImage.uWidth, kImage.uHeight, D3DFMT_A8R8G8B8,
							 &kLevels[0], static_cast<UINT>(kLevels.size()));

		if (!HasDX12Twin())
		{
			TraceError("CreateFromMemoryFile: Cannot create texture (%ux%u)",
					   kImage.uWidth, kImage.uHeight);
			Destroy();
			return false;
		}
	}

	m_bEmpty = false;
	return true;
}

void CGraphicImageTexture::SetFileName(const char * c_szFileName)
{
	m_stFileName=c_szFileName;
}

bool CGraphicImageTexture::CreateFromDiskFile(const char * c_szFileName, D3DFORMAT d3dFmt, DWORD dwFilter)
{
	Destroy();

	SetFileName(c_szFileName);

	m_d3dFmt = d3dFmt;
	m_dwFilter = dwFilter;
	return CreateDeviceObjects();
}

CGraphicImageTexture::CGraphicImageTexture()
{
	Initialize();
}

CGraphicImageTexture::~CGraphicImageTexture()
{
	Destroy();
}
