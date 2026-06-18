#include "StdAfx.h"
#include "../eterBase/Utils.h"
#include "GrpText.h"

CGraphicText::CGraphicText(const char* c_szFileName) : CResource(c_szFileName)
{
}

CGraphicText::~CGraphicText()
{
}

bool CGraphicText::CreateDeviceObjects()
{
	return m_fontTexture.CreateDeviceObjects();
}

void CGraphicText::DestroyDeviceObjects()
{
	m_fontTexture.DestroyDeviceObjects();
}

CGraphicFontTexture* CGraphicText::GetFontTexturePointer()
{
	return &m_fontTexture;
}

CGraphicText::TType CGraphicText::Type()
{
	static TType s_type = StringToType("CGraphicText");
	return s_type;
}

bool CGraphicText::OnLoad(int /*iSize*/, const void* /*c_pvBuf*/)
{
	static char strName[32];
	int size;
	bool bItalic = false;

	// format
	// Gulim.fnt Loads “Gulim” font default size 12
	// Gulim:18.fnt Loading with “Gulim” font size 18
	// Gulim:14i.fnt “Gulim” font size 14 & loading in italics
	const char * p = strrchr(GetFileName(), ':');

	if (p)
	{
		strncpy_s(strName, sizeof(strName), GetFileName(), MIN(31, static_cast<int>(p - GetFileName())));
		++p;

		static char num[8];

		int i = 0;
		while (*p && isdigit(*p))
		{
			num[i++] = *(p++);
		}

		num[i] = '\0';
		if(*p == 'i')
			bItalic = true;
		size = static_cast<int>(atoi(num));
	}
	else
	{
		p = strrchr(GetFileName(), '.');

		if (!p)
		{
			assert(!"CGraphicText::OnLoadFromFile there is no extension (ie: .fnt)");
			strName[0] = '\0';
		}
		else
			strncpy_s(strName, sizeof(strName), GetFileName(), MIN(31, static_cast<int>(p - GetFileName())));

		size = 12;
	}

	if (!m_fontTexture.Create(strName, size, bItalic))
		return false;

	return true;
}

void CGraphicText::OnClear()
{
	m_fontTexture.Destroy();
}

bool CGraphicText::OnIsEmpty() const
{
	return m_fontTexture.IsEmpty();
}

bool CGraphicText::OnIsType(TType type)
{
	if (CGraphicText::Type() == type)
		return true;
	
	return CResource::OnIsType(type);
}
