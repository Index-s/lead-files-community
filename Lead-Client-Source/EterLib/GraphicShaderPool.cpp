#include "StdAfx.h"
#include <d3dcompiler.h>
#include "../eterBase/Stl.h"
#include "GraphicShaderPool.h"
#include "StateManager.h"

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	// One WVP through dp4, matching the SpeedTree leaf shader convention.
	const char c_achPDTVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"struct VS_INPUT { float3 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    Out.vDiffuse = In.vDiffuse;\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		"    return Out;\n"
		"}\n";

	// Fixed-function stage 0: COLOROP/ALPHAOP = MODULATE(TEXTURE, DIFFUSE).
	const char c_achModulatePixelProgram[] =
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(float4 vDiffuse : COLOR0, float2 vTexCoord : TEXCOORD0) : COLOR0\n"
		"{\n"
		"    return tex2D(g_kSampler0, vTexCoord) * vDiffuse;\n"
		"}\n";

	// Fixed-function NULL-texture draw: only the interpolated diffuse reaches the output.
	const char c_achDiffusePixelProgram[] =
		"float4 main(float4 vDiffuse : COLOR0) : COLOR0\n"
		"{\n"
		"    return vDiffuse;\n"
		"}\n";

	// XYZ|TEX1 (no diffuse) through the same WVP.
	const char c_achPTVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"struct VS_INPUT { float3 vPosition : POSITION; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float2 vTexCoord : TEXCOORD0; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		"    return Out;\n"
		"}\n";

	// Fixed-function stage 0: COLOROP/ALPHAOP = SELECTARG1(TEXTURE).
	const char c_achTexturePixelProgram[] =
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(float2 vTexCoord : TEXCOORD0) : COLOR0\n"
		"{\n"
		"    return tex2D(g_kSampler0, vTexCoord);\n"
		"}\n";

	// Fixed-function: COLOROP=MODULATE(TEXTURE,DIFFUSE), ALPHAOP=SELECTARG1(TEXTURE).
	const char c_achModulateTexAlphaPixelProgram[] =
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(float4 vDiffuse : COLOR0, float2 vTexCoord : TEXCOORD0) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    return float4(kTexel.rgb * vDiffuse.rgb, kTexel.a);\n"
		"}\n";
}

CGraphicShaderPool::CGraphicShaderPool()
	: m_bCreateFailed(false)
	, m_lpPDTVertexShader(NULL)
	, m_lpPTVertexShader(NULL)
	, m_lpModulatePixelShader(NULL)
	, m_lpDiffusePixelShader(NULL)
	, m_lpTexturePixelShader(NULL)
	, m_lpModulateTexAlphaPixelShader(NULL)
	, m_lpPDTDeclaration(NULL)
	, m_lpPTDeclaration(NULL)
{
}

CGraphicShaderPool::~CGraphicShaderPool()
{
	Destroy();
}

void CGraphicShaderPool::Destroy()
{
	safe_release(m_lpPDTVertexShader);
	safe_release(m_lpPTVertexShader);
	safe_release(m_lpModulatePixelShader);
	safe_release(m_lpDiffusePixelShader);
	safe_release(m_lpTexturePixelShader);
	safe_release(m_lpModulateTexAlphaPixelShader);
	safe_release(m_lpPDTDeclaration);
	safe_release(m_lpPTDeclaration);
	m_bCreateFailed = false;
}

bool CGraphicShaderPool::__Create()
{
	if (m_bCreateFailed)
		return false;

	const D3DVERTEXELEMENT9 akPDTElements[] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	ID3DBlob* pCode = NULL;
	ID3DBlob* pError = NULL;

	if (FAILED(D3DCompile(c_achPDTVertexProgram, sizeof(c_achPDTVertexProgram) - 1, "PDTVertexProgram",
						  NULL, NULL, "main", "vs_2_0", 0, 0, &pCode, &pError)) ||
		FAILED(STATEMANAGER.CreateVertexShader((const DWORD*)pCode->GetBufferPointer(), &m_lpPDTVertexShader)))
	{
		TraceError("CGraphicShaderPool: failed to build PDT vertex shader [ %s ].",
				   pError ? (const char*)pError->GetBufferPointer() : "unknown");
		safe_release(pCode);
		safe_release(pError);
		m_bCreateFailed = true;
		return false;
	}

	safe_release(pCode);
	safe_release(pError);

	if (FAILED(D3DCompile(c_achModulatePixelProgram, sizeof(c_achModulatePixelProgram) - 1, "ModulatePixelProgram",
						  NULL, NULL, "main", "ps_2_0", 0, 0, &pCode, &pError)) ||
		FAILED(STATEMANAGER.CreatePixelShader((const DWORD*)pCode->GetBufferPointer(), &m_lpModulatePixelShader)))
	{
		TraceError("CGraphicShaderPool: failed to build modulate pixel shader [ %s ].",
				   pError ? (const char*)pError->GetBufferPointer() : "unknown");
		safe_release(pCode);
		safe_release(pError);
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	safe_release(pCode);
	safe_release(pError);

	if (FAILED(D3DCompile(c_achDiffusePixelProgram, sizeof(c_achDiffusePixelProgram) - 1, "DiffusePixelProgram",
						  NULL, NULL, "main", "ps_2_0", 0, 0, &pCode, &pError)) ||
		FAILED(STATEMANAGER.CreatePixelShader((const DWORD*)pCode->GetBufferPointer(), &m_lpDiffusePixelShader)))
	{
		TraceError("CGraphicShaderPool: failed to build diffuse pixel shader [ %s ].",
				   pError ? (const char*)pError->GetBufferPointer() : "unknown");
		safe_release(pCode);
		safe_release(pError);
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	safe_release(pCode);
	safe_release(pError);

	if (FAILED(STATEMANAGER.CreateVertexDeclaration(akPDTElements, &m_lpPDTDeclaration)))
	{
		TraceError("CGraphicShaderPool: failed to create PDT vertex declaration.");
		Destroy();
		m_bCreateFailed = true;
		return false;
	}


	const D3DVERTEXELEMENT9 akPTElements[] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	if (FAILED(D3DCompile(c_achPTVertexProgram, sizeof(c_achPTVertexProgram) - 1, "PTVertexProgram",
						  NULL, NULL, "main", "vs_2_0", 0, 0, &pCode, &pError)) ||
		FAILED(STATEMANAGER.CreateVertexShader((const DWORD*)pCode->GetBufferPointer(), &m_lpPTVertexShader)))
	{
		TraceError("CGraphicShaderPool: failed to build PT vertex shader [ %s ].",
				   pError ? (const char*)pError->GetBufferPointer() : "unknown");
		safe_release(pCode);
		safe_release(pError);
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	safe_release(pCode);
	safe_release(pError);

	if (FAILED(D3DCompile(c_achTexturePixelProgram, sizeof(c_achTexturePixelProgram) - 1, "TexturePixelProgram",
						  NULL, NULL, "main", "ps_2_0", 0, 0, &pCode, &pError)) ||
		FAILED(STATEMANAGER.CreatePixelShader((const DWORD*)pCode->GetBufferPointer(), &m_lpTexturePixelShader)))
	{
		TraceError("CGraphicShaderPool: failed to build texture pixel shader [ %s ].",
				   pError ? (const char*)pError->GetBufferPointer() : "unknown");
		safe_release(pCode);
		safe_release(pError);
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	safe_release(pCode);
	safe_release(pError);

	if (FAILED(D3DCompile(c_achModulateTexAlphaPixelProgram, sizeof(c_achModulateTexAlphaPixelProgram) - 1, "ModulateTexAlphaPixelProgram",
						  NULL, NULL, "main", "ps_2_0", 0, 0, &pCode, &pError)) ||
		FAILED(STATEMANAGER.CreatePixelShader((const DWORD*)pCode->GetBufferPointer(), &m_lpModulateTexAlphaPixelShader)))
	{
		TraceError("CGraphicShaderPool: failed to build modulate-tex-alpha pixel shader [ %s ].",
				   pError ? (const char*)pError->GetBufferPointer() : "unknown");
		safe_release(pCode);
		safe_release(pError);
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	safe_release(pCode);
	safe_release(pError);

	if (FAILED(STATEMANAGER.CreateVertexDeclaration(akPTElements, &m_lpPTDeclaration)))
	{
		TraceError("CGraphicShaderPool: failed to create PT vertex declaration.");
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	return true;
}

bool CGraphicShaderPool::__Bind(LPDIRECT3DVERTEXDECLARATION9 lpDeclaration, LPDIRECT3DVERTEXSHADER9 lpVertexShader, LPDIRECT3DPIXELSHADER9 lpPixelShader)
{
	if (!m_lpPDTDeclaration && !__Create())
		return false;

	D3DXMATRIX matWorld, matView, matProj, matWVP;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	STATEMANAGER.GetTransform(D3DTS_VIEW, &matView);
	STATEMANAGER.GetTransform(D3DTS_PROJECTION, &matProj);
	D3DXMatrixMultiply(&matWVP, &matWorld, &matView);
	D3DXMatrixMultiply(&matWVP, &matWVP, &matProj);
	D3DXMatrixTranspose(&matWVP, &matWVP);
	STATEMANAGER.SetVertexShaderConstant(0, &matWVP, 4);

	STATEMANAGER.SetVertexDeclaration(lpDeclaration);
	STATEMANAGER.SetVertexShader(lpVertexShader);
	STATEMANAGER.SetPixelShader(lpPixelShader);
	return true;
}

bool CGraphicShaderPool::BindPDTModulate()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpModulatePixelShader);
}

bool CGraphicShaderPool::BindPDTDiffuse()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpDiffusePixelShader);
}

bool CGraphicShaderPool::BindPTTexture()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTexturePixelShader);
}

bool CGraphicShaderPool::BindPDTTexture()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpTexturePixelShader);
}

bool CGraphicShaderPool::BindPDTModulateTexAlpha()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpModulateTexAlphaPixelShader);
}

void CGraphicShaderPool::Unbind()
{
	STATEMANAGER.SetVertexShader(NULL);
	STATEMANAGER.SetPixelShader(NULL);
}
