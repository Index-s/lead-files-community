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
#include <map>
#include <string>

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
	LPDIRECT3DVERTEXDECLARATION9 dwShader = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(pBranchShaderDecl);

	if (!dwShader)
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
//	LoadLeafShader

static void LoadLeafShader(LPDIRECT3DVERTEXDECLARATION9& pVertexDecl, LPDIRECT3DVERTEXSHADER9& pVertexShader)
{
    pVertexDecl = NULL;
    pVertexShader = NULL;

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

    pVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
    STATEMANAGER.RegisterShaderProgramDX12(pVertexShader, "SpeedTreeLeafVertexProgram");

    pVertexDecl = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(leafVertexDecl);
    if (!pVertexDecl) {
        TraceError("Failed to create leaf vertex declaration");
    }
}




