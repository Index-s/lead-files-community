///////////////////////////////////////////////////////////////////////  
//	SpeedTreeRT DirectX Example
//
//	(c) 2003 IDV, Inc.
//
//	This example demonstrates how to render trees using SpeedTreeRT
//	and DirectX.  Techniques illustrated include ".spt" file parsing,
//	static lighting, dynamic lighting, LOD implementation, cloning,
//	instancing, and dynamic wind effects.
//
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement.
//
//      Copyright (c) 2001-2003 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		1233 Washington St. Suite 610
//		Columbia, SC 29201
//		Voice: (803) 799-1699
//		Fax:   (803) 931-0320
//		Web:   http://www.idvinc.com

///////////////////////////////////////////////////////////////////////  
//	Includes

#pragma once
#include "SpeedTreeConfig.h"
#include "../eterlib/StateManager.h"
#include <d3dcompiler.h>
#include <map>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

///////////////////////////////////////////////////////////////////////  
//	Branch & Frond Vertex Formats

static DWORD D3DFVF_SPEEDTREE_BRANCH_VERTEX =
		D3DFVF_XYZ |							// always have the position
	#ifdef WRAPPER_USE_DYNAMIC_LIGHTING			// precomputed colors or geometric normals
		D3DFVF_NORMAL |
	#else
		D3DFVF_DIFFUSE |
	#endif
	#ifdef WRAPPER_RENDER_SELF_SHADOWS
		D3DFVF_TEX2 | D3DFVF_TEXCOORDSIZE2(0) | D3DFVF_TEXCOORDSIZE2(1) // shadow texture coordinates
	#else
		D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0)	// always have first texture layer coords
	#endif
	#ifdef WRAPPER_USE_GPU_WIND					
		| D3DFVF_TEX3 | D3DFVF_TEXCOORDSIZE2(2)	// GPU Only - wind weight and index passed in second texture layer
	#endif
		;

/////////////////////////////////////////////////////////////////////// 
// FVF Branch Vertex Structure

struct SFVFBranchVertex
{
	D3DXVECTOR3		m_vPosition;			// Always Used							
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING			
	D3DXVECTOR3		m_vNormal;				// Dynamic Lighting Only			
#else										     
	DWORD			m_dwDiffuseColor;		// Static Lighting Only	
#endif	
	FLOAT			m_fTexCoords[2];		// Always Used
#ifdef WRAPPER_RENDER_SELF_SHADOWS
	FLOAT			m_fShadowCoords[2];		// Texture coordinates for the shadows
#endif
#ifdef WRAPPER_USE_GPU_WIND		
	FLOAT			m_fWindIndex;			// GPU Only
	FLOAT			m_fWindWeight;			
#endif
};


///////////////////////////////////////////////////////////////////////
//	LoadBranchShader

static LPDIRECT3DVERTEXDECLARATION9 LoadBranchShader()
{
	// branch shader declaration
	D3DVERTEXELEMENT9 pBranchShaderDecl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END()
	};

	// assemble shader
	LPDIRECT3DVERTEXDECLARATION9 dwShader = NULL;

	if (STATEMANAGER.CreateVertexDeclaration(pBranchShaderDecl, &dwShader) != D3D_OK)
	{
		char szError[1024];
		sprintf_s(szError, "Failed to create branch vertex shader.");
		MessageBox(NULL, szError, "Vertex Shader Error", MB_ICONSTOP);
	}

	return dwShader;
}

///////////////////////////////////////////////////////////////////////  
//	Leaf Vertex Formats

static DWORD D3DFVF_SPEEDTREE_LEAF_VERTEX =
		D3DFVF_XYZ |							// always have the position
	#ifdef WRAPPER_USE_DYNAMIC_LIGHTING			// precomputed colors or geometric normals
		D3DFVF_NORMAL |
	#else
		D3DFVF_DIFFUSE |
	#endif
		D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0)	// always have first texture layer coords
	#if defined WRAPPER_USE_GPU_WIND || defined WRAPPER_USE_GPU_LEAF_PLACEMENT					
		| D3DFVF_TEX3 | D3DFVF_TEXCOORDSIZE4(2)	// GPU Only - wind weight and index passed in second texture layer
	#endif
		;


/////////////////////////////////////////////////////////////////////// 
// FVF Leaf Vertex Structure

struct SFVFLeafVertex
{
		D3DXVECTOR3		m_vPosition;			// Always Used							
	#ifdef WRAPPER_USE_DYNAMIC_LIGHTING			
		D3DXVECTOR3		m_vNormal;				// Dynamic Lighting Only			
	#else										     
		DWORD			m_dwDiffuseColor;		// Static Lighting Only	
	#endif											
		FLOAT			m_fTexCoords[2];		// Always Used
	#if defined WRAPPER_USE_GPU_WIND || defined WRAPPER_USE_GPU_LEAF_PLACEMENT
		FLOAT			m_fWindIndex;			// Only used when GPU is involved
		FLOAT			m_fWindWeight;					
		FLOAT			m_fLeafPlacementIndex;
		FLOAT			m_fLeafScalarValue;
	#endif
};


///////////////////////////////////////////////////////////////////////
//	Leaf Vertex Program
//
//	HLSL (vs_2_0 via D3DCompile) translation of the original vs.1.1 assembly.
//	The constant file is addressed as one absolute-register array so the
//	CPU-side SetVertexShaderConstant layout (Constants at c0..c85) is unchanged.

static const char g_achLeafVertexProgram[] =
		"float4 g_avConstants[86] : register(c0);\n"

		"struct VS_INPUT\n"
		"{\n"
		"    float3 vPosition  : POSITION;\n"
#ifdef WRAPPER_USE_STATIC_LIGHTING
		"    float4 vColor     : COLOR0;\n"
#else
		"    float3 vNormal    : NORMAL;\n"
#endif
		"    float2 vTexCoord  : TEXCOORD0;\n"
#if defined WRAPPER_USE_GPU_WIND || defined WRAPPER_USE_GPU_LEAF_PLACEMENT
		"    float4 vPlacement : TEXCOORD2;\n"
#endif
		"};\n"

		"struct VS_OUTPUT\n"
		"{\n"
		"    float4 vPosition : POSITION;\n"
		"    float4 vDiffuse  : COLOR0;\n"
		"    float2 vTexCoord : TEXCOORD0;\n"
#ifdef WRAPPER_USE_FOG
		"    float  fFog      : FOG;\n"
#endif
		"};\n"

		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"

		// always pass texcoord0 through
		"    Out.vTexCoord = In.vTexCoord;\n"

#ifdef WRAPPER_USE_GPU_WIND
		// wind interpolation: r0 = lerp(v0, v0 * wind_matrix[c54 + v9.x], v9.y)
		"    float4 vVertex = float4(In.vPosition, 1.0f);\n"
		"    int nWindIndex = (int)In.vPlacement.x;\n"
		"    float4 vWind;\n"
		"    vWind.x = dot(vVertex, g_avConstants[54 + nWindIndex]);\n"
		"    vWind.y = dot(vVertex, g_avConstants[55 + nWindIndex]);\n"
		"    vWind.z = dot(vVertex, g_avConstants[56 + nWindIndex]);\n"
		"    vWind.w = dot(vVertex, g_avConstants[57 + nWindIndex]);\n"
		"    float4 r0 = (vWind - vVertex) * In.vPlacement.y + vVertex;\n"
#else
		// wind already handled, pass the vertex through
		"    float4 r0 = float4(In.vPosition, 1.0f);\n"
#endif

#ifdef WRAPPER_USE_GPU_LEAF_PLACEMENT
		// place the leaves: r0 += leaf_table[v9.z] * v9.w (absolute register index)
		"    r0 += g_avConstants[(int)In.vPlacement.z] * In.vPlacement.w;\n"
#endif

		// translate to tree's position, project to screen (m4x4 with c0..c3)
		"    r0 += g_avConstants[52];\n"
		"    Out.vPosition.x = dot(r0, g_avConstants[0]);\n"
		"    Out.vPosition.y = dot(r0, g_avConstants[1]);\n"
		"    Out.vPosition.z = dot(r0, g_avConstants[2]);\n"
		"    Out.vPosition.w = dot(r0, g_avConstants[3]);\n"

#ifdef WRAPPER_USE_FOG
		// linear fog: (c85.y - dp4(r0, c2)) * c85.z
		"    Out.fFog = (g_avConstants[85].y - dot(r0, g_avConstants[2])) * g_avConstants[85].z;\n"
#endif

#ifdef WRAPPER_USE_STATIC_LIGHTING
		// pass color through
		"    Out.vDiffuse = In.vColor;\n"
#else
		// diffuse = max(dot(normal, light_dir), c70.x) * (c73*c74) + (c72*c75)
		"    float4 vDiffuseValues = g_avConstants[73] * g_avConstants[74];\n"
		"    float4 vAmbientValues = g_avConstants[72] * g_avConstants[75];\n"
		"    float fDot = max(dot(In.vNormal, g_avConstants[71].xyz), g_avConstants[70].x);\n"
		"    Out.vDiffuse = fDot * vDiffuseValues + vAmbientValues;\n"
#endif

		"    return Out;\n"
		"}\n";


///////////////////////////////////////////////////////////////////////  
//	LoadLeafShader

static void LoadLeafShader(LPDIRECT3DVERTEXDECLARATION9& pVertexDecl, LPDIRECT3DVERTEXSHADER9& pVertexShader)
{
    SAFE_RELEASE(pVertexDecl);
    SAFE_RELEASE(pVertexShader);

	const D3DVERTEXELEMENT9 leafVertexDecl[] = {
			{ 0,  0, D3DDECLTYPE_FLOAT3,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,     0 },
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
			{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,        0 },
			{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,      0 },
	#if defined WRAPPER_USE_GPU_WIND || defined WRAPPER_USE_GPU_LEAF_PLACEMENT
			{ 0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,      2 },
	#endif
#else
			{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,       0 },
			{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,      0 },
	#if defined WRAPPER_USE_GPU_WIND || defined WRAPPER_USE_GPU_LEAF_PLACEMENT
			{ 0, 24, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,      2 },
	#endif
#endif
			D3DDECL_END()
	};

    ID3DBlob* pCode = nullptr;
    ID3DBlob* pError = nullptr;
    if (SUCCEEDED(D3DCompile(g_achLeafVertexProgram, sizeof(g_achLeafVertexProgram) - 1, "LeafVertexProgram",
                             nullptr, nullptr, "main", "vs_2_0", 0, 0, &pCode, &pError))) {
        if (STATEMANAGER.CreateVertexShader((const DWORD*)pCode->GetBufferPointer(), &pVertexShader) != D3D_OK) {
            TraceError("Failed to create leaf vertex shader.");
        }
    }
    else {
        TraceError("Failed to compile leaf vertex shader. The error reported is [ %s ].",
                   pError ? (const char*)pError->GetBufferPointer() : "unknown");
    }

    if (FAILED(STATEMANAGER.CreateVertexDeclaration(leafVertexDecl, &pVertexDecl))) {
        TraceError("Failed to create leaf vertex declaration");
    }

    SAFE_RELEASE(pCode);
    SAFE_RELEASE(pError);
}




