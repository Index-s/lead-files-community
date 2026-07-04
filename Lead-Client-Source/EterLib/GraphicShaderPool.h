#pragma once

#include <d3d9.h>

// Shader replacements for fixed-function draw paths (DX12 migration).
// Compiled lazily on first bind, released on device destroy.
class CGraphicShaderPool
{
	public:
		CGraphicShaderPool();
		~CGraphicShaderPool();

		// XYZ|DIFFUSE|TEX1 vertices through WORLD*VIEW*PROJECTION,
		// pixel = texture * diffuse (the fixed-function default cascade).
		bool BindPDTModulate();
		// Same vertex path, pixel = diffuse only (fixed-function NULL-texture draws).
		bool BindPDTDiffuse();
		// XYZ|TEX1 vertices, pixel = texture only (fixed-function SELECTARG1(TEXTURE)).
		bool BindPTTexture();
		// XYZ|DIFFUSE|TEX1 vertices, pixel = texture only (diffuse present but unused).
		bool BindPDTTexture();
		// XYZ|DIFFUSE|TEX1 vertices, rgb = texture * diffuse, alpha = texture.
		bool BindPDTModulateTexAlpha();
		void Unbind();

		void Destroy();

	private:
		bool __Create();
		bool __Bind(LPDIRECT3DVERTEXDECLARATION9 lpDeclaration, LPDIRECT3DVERTEXSHADER9 lpVertexShader, LPDIRECT3DPIXELSHADER9 lpPixelShader);

		bool m_bCreateFailed;
		LPDIRECT3DVERTEXSHADER9			m_lpPDTVertexShader;
		LPDIRECT3DVERTEXSHADER9			m_lpPTVertexShader;
		LPDIRECT3DPIXELSHADER9			m_lpModulatePixelShader;
		LPDIRECT3DPIXELSHADER9			m_lpDiffusePixelShader;
		LPDIRECT3DPIXELSHADER9			m_lpTexturePixelShader;
		LPDIRECT3DPIXELSHADER9			m_lpModulateTexAlphaPixelShader;
		LPDIRECT3DVERTEXDECLARATION9	m_lpPDTDeclaration;
		LPDIRECT3DVERTEXDECLARATION9	m_lpPTDeclaration;
};
