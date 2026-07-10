#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GraphicShaderPool.h"
#include "StateManager.h"

namespace
{
	// The fixed-function vertex-fog factor, evaluated exactly like D3D9 FFP:
	// LINEAR ramp or EXP density on the (optionally radial) view-space distance,
	// selected by the flags __Bind derives from the cached fog render states.
	// Reads the transposed WORLDVIEW rows in c11-c13 and the params in c28-c29.
	#define FOG_VS_PARAMS \
		"float4 g_vFogParams : register(c28);\n" \
		"float4 g_vFogParams2 : register(c29);\n"
	#define FOG_VS_DECLARATIONS \
		"float4 g_avWorldView[3] : register(c11);\n" \
		FOG_VS_PARAMS
	#define FOG_VS_BODY \
		"    float3 vViewPos;\n" \
		"    vViewPos.x = dot(vPosition, g_avWorldView[0]);\n" \
		"    vViewPos.y = dot(vPosition, g_avWorldView[1]);\n" \
		"    vViewPos.z = dot(vPosition, g_avWorldView[2]);\n" \
		"    float fFogDist = lerp(vViewPos.z, length(vViewPos), g_vFogParams2.y);\n" \
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n" \
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"

	// One WVP through dp4, matching the SpeedTree leaf shader convention.
	const char c_achPDTVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		FOG_VS_DECLARATIONS
		"struct VS_INPUT { float3 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vPadCoord1 : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
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
		FOG_VS_BODY
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadCoord1 = 0;\n"
		"    return Out;\n"
		"}\n";

	// Fixed-function stage 0: COLOROP/ALPHAOP = MODULATE(TEXTURE, DIFFUSE).
	const char c_achModulatePixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = tex2D(g_kSampler0, vTexCoord) * vDiffuse;\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	const char c_achModulateSpecAlphaPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTex = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = kTex * vDiffuse;\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"    kFinal.a = kTex.a * g_kTFactor.a;\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	const char c_achModulateNoFogPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0) : COLOR0\n"
		"{\n"
		"    float4 kFinal = tex2D(g_kSampler0, vTexCoord) * vDiffuse;\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Actor fade (BlendRender): rgb = texture * lit diffuse, alpha = TFACTOR
	// (stage0 ALPHAOP SELECTARG2(TFACTOR), stage1 disabled).
	const char c_achLitBlendPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = float4(tex2D(g_kSampler0, vTexCoord).rgb * vDiffuse.rgb, g_kTFactor.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Actor hit flash (AddRender): stage1 ADD(CURRENT, TFACTOR) on the lit base.
	const char c_achLitAddPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb * vDiffuse.rgb + g_kTFactor.rgb, kTexel.a * vDiffuse.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Fixed-function NULL-texture draw: only the interpolated diffuse reaches the output.
	const char c_achDiffusePixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vPadCoord0 : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = vDiffuse;\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Character-shadow cast: solid TEXTUREFACTOR silhouette
	// (stage0 SELECTARG1(TFACTOR), stage1 disabled).
	const char c_achFlatTFactorPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vPadCoord0 : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = float4(g_kTFactor.rgb, 1.0f);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// XYZ|TEX1 (no diffuse) through the same WVP.
	const char c_achPTVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		FOG_VS_DECLARATIONS
		"struct VS_INPUT { float3 vPosition : POSITION; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vPadDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vPadCoord1 : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		FOG_VS_BODY
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadDiffuse = 0;\n"
		"    Out.vPadCoord1 = 0;\n"
		"    return Out;\n"
		"}\n";

	// Fixed-function stage 0: COLOROP/ALPHAOP = SELECTARG1(TEXTURE).
		// Water patches: XYZ|DIFFUSE stream, UVs generated from the camera-space
	// position through the TEXTURE0 transform (rows in c4-c5); the far variant
	// drops the texture and passes the vertex color through.
	const char c_achWaterVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avTexMat0[2] : register(c4);\n"
		"float4 g_avWorldView[3] : register(c11);\n"
		"float4 g_vFogParams : register(c28);\n"
		"float4 g_vFogParams2 : register(c29);\n"
		"struct VS_INPUT { float3 vPosition : POSITION; float4 vDiffuse : COLOR0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vPadCoord1 : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float4 vCamPos;\n"
		"    vCamPos.x = dot(vPosition, g_avWorldView[0]);\n"
		"    vCamPos.y = dot(vPosition, g_avWorldView[1]);\n"
		"    vCamPos.z = dot(vPosition, g_avWorldView[2]);\n"
		"    vCamPos.w = 1.0f;\n"
		"    Out.vTexCoord.x = dot(vCamPos, g_avTexMat0[0]);\n"
		"    Out.vTexCoord.y = dot(vCamPos, g_avTexMat0[1]);\n"
		"    Out.vDiffuse = In.vDiffuse;\n"
		"    float fFogDist = lerp(vCamPos.z, length(vCamPos.xyz), g_vFogParams2.y);\n"
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n"
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadCoord1 = 0;\n"
		"    return Out;\n"
		"}\n";

	// Water surface: rgb from the animated texture, alpha from the vertex color
	// (stage0 SELECTARG1(TEXTURE) color + SELECTARG1(DIFFUSE) alpha).
	const char c_achWaterPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = float4(tex2D(g_kSampler0, vTexCoord).rgb, vDiffuse.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

const char c_achTexturePixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = tex2D(g_kSampler0, vTexCoord);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Fixed-function: COLOROP=MODULATE(TEXTURE,DIFFUSE), ALPHAOP=SELECTARG1(TEXTURE).
	const char c_achModulateTexAlphaPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb * vDiffuse.rgb, kTexel.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Fixed-function two-texture materials: stage0 MODULATE(TEXTURE,DIFFUSE),
	// stage1 MODULATE(TEXTURE,CURRENT) sharing the single UV set, alpha from
	// the lit vertex color (stage0 ALPHAOP=DISABLE).
	const char c_achLitTwoTexPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"sampler2D g_kSampler1 : register(s1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kBase = tex2D(g_kSampler0, vTexCoord);\n"
		"    float3 kSecond = tex2D(g_kSampler1, vTexCoord).rgb;\n"
		"    float4 kFinal = float4(kBase.rgb * vDiffuse.rgb * kSecond, vDiffuse.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Fixed-function tint pass: stage0 MODULATE(TEXTURE,DIFFUSE) for color and
	// alpha, stage1 MODULATE(CURRENT,TFACTOR) multiplying the tint in.
	const char c_achLitTFactorTintPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb * vDiffuse.rgb * g_kTFactor.rgb, kTexel.a * vDiffuse.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Blocking-building fade (RenderPCBlocker): rgb = lit base texture, alpha
	// sampled from the screen-projected blocker mask on the generated
	// TEXCOORD1 (stage1 ALPHAOP=SELECTARG1(TEXTURE) via CAMERASPACEPOSITION).
	const char c_achLitProjectedAlphaPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"sampler2D g_kSampler1 : register(s1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vProjectedCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb * vDiffuse.rgb, tex2D(g_kSampler1, vProjectedCoord).a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Minimap terrain tiles: XYZ|TEX1 stream, tile UVs passed through, cover
	// UVs generated from the camera-space position through the TEXTURE1
	// transform (rows in c6-c7).
	const char c_achMiniMapVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avTexMat1[2] : register(c6);\n"
		"float4 g_avWorldView[3] : register(c11);\n"
		"float4 g_vFogParams : register(c28);\n"
		"float4 g_vFogParams2 : register(c29);\n"
		"struct VS_INPUT { float3 vPosition : POSITION; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vPadDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vCoverCoord : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float4 vCamPos;\n"
		"    vCamPos.x = dot(vPosition, g_avWorldView[0]);\n"
		"    vCamPos.y = dot(vPosition, g_avWorldView[1]);\n"
		"    vCamPos.z = dot(vPosition, g_avWorldView[2]);\n"
		"    vCamPos.w = 1.0f;\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		"    Out.vCoverCoord.x = dot(vCamPos, g_avTexMat1[0]);\n"
		"    Out.vCoverCoord.y = dot(vCamPos, g_avTexMat1[1]);\n"
		"    float fFogDist = lerp(vCamPos.z, length(vCamPos.xyz), g_vFogParams2.y);\n"
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n"
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadDiffuse = 0;\n"
		"    return Out;\n"
		"}\n";

	// Minimap tile: rgb = tile * cover, alpha = cover (stage1 MODULATE +
	// alpha SELECTARG1(TEXTURE) over the stage0 tile).
	const char c_achMiniMapPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"Texture2D g_kTexture0 : register(t0);\n"
		"Texture2D g_kTexture1 : register(t1);\n"
		"SamplerState g_kSampler0 : register(s0);\n"
		"SamplerState g_kSampler1 : register(s1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vCoverCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kCover = g_kTexture1.Sample(g_kSampler1, vCoverCoord);\n"
		"    float4 kFinal = float4(g_kTexture0.Sample(g_kSampler0, vTexCoord).rgb * kCover.rgb, kCover.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Unloaded minimap tile: TFACTOR fill times the cover.
	const char c_achMiniMapTFactorPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"Texture2D g_kTexture1 : register(t1);\n"
		"SamplerState g_kSampler1 : register(s1);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vPadCoord0 : TEXCOORD0,\n"
		"    float2 vCoverCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kCover = g_kTexture1.Sample(g_kSampler1, vCoverCoord);\n"
		"    float4 kFinal = float4(g_kTFactor.rgb * kCover.rgb, kCover.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// PDT vertices with the TEXTURE0 transform applied to the UVs (COUNT2).
	const char c_achPDTTexMatVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avTexMat[2] : register(c4);\n"
		FOG_VS_DECLARATIONS
		"struct VS_INPUT { float3 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vPadCoord1 : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    Out.vDiffuse = In.vDiffuse;\n"
		"    float4 vTexCoord = float4(In.vTexCoord, 1.0f, 0.0f);\n"
		"    Out.vTexCoord.x = dot(vTexCoord, g_avTexMat[0]);\n"
		"    Out.vTexCoord.y = dot(vTexCoord, g_avTexMat[1]);\n"
		FOG_VS_BODY
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadCoord1 = 0;\n"
		"    return Out;\n"
		"}\n";


	// SpeedTree branches/fronds: XYZ|DIFFUSE|TEX2 stream with precomputed
	// lighting in the vertex color and the self-shadow map on the second UV set.
	const char c_achSpeedTreeBranchVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avWorldView[3] : register(c11);\n"
		"float4 g_vFogParams : register(c28);\n"
		"float4 g_vFogParams2 : register(c29);\n"
		"struct VS_INPUT { float3 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vShadowCoord : TEXCOORD1; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vShadowCoord : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
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
		"    Out.vShadowCoord = In.vShadowCoord;\n"
		"    float3 vViewPos;\n"
		"    vViewPos.x = dot(vPosition, g_avWorldView[0]);\n"
		"    vViewPos.y = dot(vPosition, g_avWorldView[1]);\n"
		"    vViewPos.z = dot(vPosition, g_avWorldView[2]);\n"
		"    float fFogDist = lerp(vViewPos.z, length(vViewPos), g_vFogParams2.y);\n"
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n"
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    return Out;\n"
		"}\n";

	// Branch with self-shadow: both stages modulate (texture, diffuse, shadow).
	const char c_achSpeedTreeShadowPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"sampler2D g_kSampler1 : register(s1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vShadowCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = tex2D(g_kSampler0, vTexCoord) * vDiffuse * tex2D(g_kSampler1, vShadowCoord);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// XYZ|NORMAL|TEX1 with the fixed-function directional-light formula:
	// color = saturate(cAmbient + cDiffuse * max(0, dot(worldNormal, lightDir))).
	const char c_achPNTLitVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avWorld[3] : register(c4);\n"
		"float4 g_vLightDirection : register(c8);\n"
		"float4 g_kLitDiffuse : register(c9);\n"
		"float4 g_kLitAmbient : register(c10);\n"
		FOG_VS_DECLARATIONS
		"struct VS_INPUT { float3 vPosition : POSITION; float3 vNormal : NORMAL; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vPadCoord1 : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float4 vNormal = float4(In.vNormal, 0.0f);\n"
		"    float3 vWorldNormal;\n"
		"    vWorldNormal.x = dot(vNormal, g_avWorld[0]);\n"
		"    vWorldNormal.y = dot(vNormal, g_avWorld[1]);\n"
		"    vWorldNormal.z = dot(vNormal, g_avWorld[2]);\n"
		"    float fDot = max(0.0f, dot(vWorldNormal, g_vLightDirection.xyz));\n"
		"    Out.vDiffuse = saturate(g_kLitAmbient + g_kLitDiffuse * fDot);\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		FOG_VS_BODY
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadCoord1 = 0;\n"
		"    return Out;\n"
		"}\n";

	// XYZ|NORMAL|TEX1 with the fixed-function spot + point vertex lighting used
	// by the character-preview screens (grp.SetOmniLight): per light,
	// atten = 1/(a0 + a1*d + a2*d*d) inside Range, spot factor is the linear
	// theta/phi cone ramp (Falloff = 1), ambient and diffuse both scaled by them.
	const char c_achPNTLitOmniVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avWorld[3] : register(c4);\n"
		FOG_VS_DECLARATIONS
		"float4 g_vSpotPos : register(c18);\n"       // xyz = position, w = range
		"float4 g_vSpotDir : register(c19);\n"       // xyz = unit direction
		"float4 g_vSpotAtten : register(c20);\n"     // xyz = a0/a1/a2, w = cos(theta/2)
		"float4 g_kSpotDiffuse : register(c21);\n"   // rgb = light.Diffuse*mat.Diffuse, w = cos(phi/2)
		"float4 g_kSpotAmbient : register(c22);\n"   // rgb = light.Ambient*mat.Ambient
		"float4 g_vPointPos : register(c23);\n"      // xyz = position, w = range
		"float4 g_vPointAtten : register(c24);\n"    // xyz = a0/a1/a2, w = enabled (0/1)
		"float4 g_kPointDiffuse : register(c25);\n"
		"float4 g_kPointAmbient : register(c26);\n"
		"float4 g_kBaseColor : register(c27);\n"     // rgb = mat.Ambient*RS_AMBIENT + mat.Emissive, w = mat.Diffuse.a
		"struct VS_INPUT { float3 vPosition : POSITION; float3 vNormal : NORMAL; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vPadCoord1 : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float3 vWorldPos;\n"
		"    vWorldPos.x = dot(vPosition, g_avWorld[0]);\n"
		"    vWorldPos.y = dot(vPosition, g_avWorld[1]);\n"
		"    vWorldPos.z = dot(vPosition, g_avWorld[2]);\n"
		"    float4 vNormal = float4(In.vNormal, 0.0f);\n"
		"    float3 vWorldNormal;\n"
		"    vWorldNormal.x = dot(vNormal, g_avWorld[0]);\n"
		"    vWorldNormal.y = dot(vNormal, g_avWorld[1]);\n"
		"    vWorldNormal.z = dot(vNormal, g_avWorld[2]);\n"
		"    float3 vColor = g_kBaseColor.rgb;\n"
		"    {\n"
		"        float3 vToLight = g_vSpotPos.xyz - vWorldPos;\n"
		"        float fDist = length(vToLight);\n"
		"        float3 vL = vToLight / fDist;\n"
		"        float fAtten = 1.0f / dot(g_vSpotAtten.xyz, float3(1.0f, fDist, fDist * fDist));\n"
		"        fAtten *= (fDist <= g_vSpotPos.w) ? 1.0f : 0.0f;\n"
		"        float fRho = dot(g_vSpotDir.xyz, -vL);\n"
		"        float fSpot = saturate((fRho - g_kSpotDiffuse.w) / (g_vSpotAtten.w - g_kSpotDiffuse.w));\n"
		"        float fDot = max(0.0f, dot(vWorldNormal, vL));\n"
		"        vColor += fAtten * fSpot * (g_kSpotAmbient.rgb + g_kSpotDiffuse.rgb * fDot);\n"
		"    }\n"
		"    {\n"
		"        float3 vToLight = g_vPointPos.xyz - vWorldPos;\n"
		"        float fDist = length(vToLight);\n"
		"        float3 vL = vToLight / fDist;\n"
		"        float fAtten = 1.0f / dot(g_vPointAtten.xyz, float3(1.0f, fDist, fDist * fDist));\n"
		"        fAtten *= (fDist <= g_vPointPos.w) ? g_vPointAtten.w : 0.0f;\n"
		"        float fDot = max(0.0f, dot(vWorldNormal, vL));\n"
		"        vColor += fAtten * (g_kPointAmbient.rgb + g_kPointDiffuse.rgb * fDot);\n"
		"    }\n"
		"    Out.vDiffuse = saturate(float4(vColor, g_kBaseColor.w));\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		FOG_VS_BODY
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadCoord1 = 0;\n"
		"    return Out;\n"
		"}\n";


	// Lit PNT with a second texcoord: the fixed-function CAMERASPACEREFLECTIONVECTOR
	// texgen (R = 2(E.N)N - E in camera space) through the TEXTURE1 transform.
	const char c_achPNTLitSpecVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avWorld[3] : register(c4);\n"
		"float4 g_vLightDirection : register(c8);\n"
		"float4 g_kLitDiffuse : register(c9);\n"
		"float4 g_kLitAmbient : register(c10);\n"
		"float4 g_avWorldView[3] : register(c11);\n"
		"float4 g_avTexMat1[2] : register(c15);\n"
		FOG_VS_PARAMS
		"struct VS_INPUT { float3 vPosition : POSITION; float3 vNormal : NORMAL; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vSpecCoord : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float4 vNormal = float4(In.vNormal, 0.0f);\n"
		"    float3 vWorldNormal;\n"
		"    vWorldNormal.x = dot(vNormal, g_avWorld[0]);\n"
		"    vWorldNormal.y = dot(vNormal, g_avWorld[1]);\n"
		"    vWorldNormal.z = dot(vNormal, g_avWorld[2]);\n"
		"    float fDot = max(0.0f, dot(vWorldNormal, g_vLightDirection.xyz));\n"
		"    Out.vDiffuse = saturate(g_kLitAmbient + g_kLitDiffuse * fDot);\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		"    float3 vCamPos;\n"
		"    vCamPos.x = dot(vPosition, g_avWorldView[0]);\n"
		"    vCamPos.y = dot(vPosition, g_avWorldView[1]);\n"
		"    vCamPos.z = dot(vPosition, g_avWorldView[2]);\n"
		"    float3 vCamNormal;\n"
		"    vCamNormal.x = dot(vNormal, g_avWorldView[0]);\n"
		"    vCamNormal.y = dot(vNormal, g_avWorldView[1]);\n"
		"    vCamNormal.z = dot(vNormal, g_avWorldView[2]);\n"
		"    float3 vEye = normalize(vCamPos);\n"
		"    float3 vUnitNormal = normalize(vCamNormal);\n"
		"    float4 vReflect = float4(2.0f * dot(vEye, vUnitNormal) * vUnitNormal - vEye, 1.0f);\n"
		"    Out.vSpecCoord.x = dot(vReflect, g_avTexMat1[0]);\n"
		"    Out.vSpecCoord.y = dot(vReflect, g_avTexMat1[1]);\n"
		"    float fFogDist = lerp(vCamPos.z, length(vCamPos), g_vFogParams2.y);\n"
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n"
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    return Out;\n"
		"}\n";

	// Granny specular: stage0 MODULATE(tex, lit) with alpha = tex.a * TFACTOR.a,
	// stage1 MODULATEALPHA_ADDCOLOR(current, envmap), alpha = current.
	const char c_achLitSpecPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"sampler2D g_kSampler1 : register(s1);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vSpecCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float3 vColor = kTexel.rgb * vDiffuse.rgb;\n"
		"    float fAlpha = kTexel.a * g_kTFactor.a;\n"
		"    float3 vEnv = tex2D(g_kSampler1, vSpecCoord).rgb;\n"
		"    float4 kFinal = float4(vColor + fAlpha * vEnv, fAlpha);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";


	// XYZ|NORMAL|TEX1|TEX2 through WVP, both UV sets passed through (dungeon).
	const char c_achPNT2VertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		FOG_VS_DECLARATIONS
		"struct VS_INPUT { float3 vPosition : POSITION; float2 vTexCoord : TEXCOORD0; float2 vLightCoord : TEXCOORD1; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vPadDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vLightCoord : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		"    Out.vLightCoord = In.vLightCoord;\n"
		FOG_VS_BODY
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadDiffuse = 0;\n"
		"    return Out;\n"
		"}\n";

	// Dungeon cascade: stage0 SELECTARG1(TEXTURE), stage1 MODULATE(TEXTURE, CURRENT).
	const char c_achLightmapPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"sampler2D g_kSampler1 : register(s1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vLightCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = tex2D(g_kSampler0, vTexCoord) * tex2D(g_kSampler1, vLightCoord);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";


	// Lit PNT with the fixed-function CAMERASPACEPOSITION texgen on TEXCOORD1
	// (character-shadow projection through the TEXTURE1 transform).
	const char c_achPNTLitRecvVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avWorld[3] : register(c4);\n"
		"float4 g_vLightDirection : register(c8);\n"
		"float4 g_kLitDiffuse : register(c9);\n"
		"float4 g_kLitAmbient : register(c10);\n"
		"float4 g_avWorldView[3] : register(c11);\n"
		"float4 g_avTexMat1[2] : register(c15);\n"
		FOG_VS_PARAMS
		"struct VS_INPUT { float3 vPosition : POSITION; float3 vNormal : NORMAL; float2 vTexCoord : TEXCOORD0; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vShadowCoord : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float4 vNormal = float4(In.vNormal, 0.0f);\n"
		"    float3 vWorldNormal;\n"
		"    vWorldNormal.x = dot(vNormal, g_avWorld[0]);\n"
		"    vWorldNormal.y = dot(vNormal, g_avWorld[1]);\n"
		"    vWorldNormal.z = dot(vNormal, g_avWorld[2]);\n"
		"    float fDot = max(0.0f, dot(vWorldNormal, g_vLightDirection.xyz));\n"
		"    Out.vDiffuse = saturate(g_kLitAmbient + g_kLitDiffuse * fDot);\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		"    float4 vCamPos;\n"
		"    vCamPos.x = dot(vPosition, g_avWorldView[0]);\n"
		"    vCamPos.y = dot(vPosition, g_avWorldView[1]);\n"
		"    vCamPos.z = dot(vPosition, g_avWorldView[2]);\n"
		"    vCamPos.w = 1.0f;\n"
		"    Out.vShadowCoord.x = dot(vCamPos, g_avTexMat1[0]);\n"
		"    Out.vShadowCoord.y = dot(vCamPos, g_avTexMat1[1]);\n"
		"    float fFogDist = lerp(vCamPos.z, length(vCamPos.xyz), g_vFogParams2.y);\n"
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n"
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    return Out;\n"
		"}\n";

	// Shadow receiver: stage0 MODULATE(tex, lit), stage1 MODULATE(shadow, current).
	const char c_achLitShadowPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"sampler2D g_kSampler1 : register(s1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vShadowCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float3 vColor = tex2D(g_kSampler0, vTexCoord).rgb * vDiffuse.rgb;\n"
		"    vColor *= tex2D(g_kSampler1, vShadowCoord).rgb;\n"
		"    float4 kFinal = float4(vColor, 1.0f);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// XYZ|NORMAL|TEX1|TEX2 with the character-shadow projection on TEXCOORD1.
	// Position goes through the same WVP dot sequence as PNT2VertexProgram so
	// both dungeon-block passes rasterize bit-identical depths.
	const char c_achPNT2RecvVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avWorldView[3] : register(c11);\n"
		"float4 g_avTexMat1[2] : register(c15);\n"
		FOG_VS_PARAMS
		"struct VS_INPUT { float3 vPosition : POSITION; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vPadDiffuse : COLOR0; float2 vPadCoord0 : TEXCOORD0; float2 vShadowCoord : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float4 vCamPos;\n"
		"    vCamPos.x = dot(vPosition, g_avWorldView[0]);\n"
		"    vCamPos.y = dot(vPosition, g_avWorldView[1]);\n"
		"    vCamPos.z = dot(vPosition, g_avWorldView[2]);\n"
		"    vCamPos.w = 1.0f;\n"
		"    Out.vShadowCoord.x = dot(vCamPos, g_avTexMat1[0]);\n"
		"    Out.vShadowCoord.y = dot(vCamPos, g_avTexMat1[1]);\n"
		"    float fFogDist = lerp(vCamPos.z, length(vCamPos.xyz), g_vFogParams2.y);\n"
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n"
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadDiffuse = 0;\n"
		"    Out.vPadCoord0 = 0;\n"
		"    return Out;\n"
		"}\n";

	// Dungeon-block shadow receiver: stage0 SELECTARG1(TFACTOR), stage1
	// MODULATE(shadow, current); the ZERO/SRCCOLOR blend applies it to the scene.
	const char c_achTFactorShadowPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"Texture2D g_kTexture1 : register(t1);\n"
		"SamplerState g_kSampler1 : register(s1);\n"
		"float4 g_vTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vPadCoord0 : TEXCOORD0,\n"
		"    float2 vShadowCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = float4(g_vTFactor.rgb * g_kTexture1.Sample(g_kSampler1, vShadowCoord).rgb, 1.0f);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Terrain patches: XYZ|NORMAL stream, both UV sets generated from the
	// camera-space position through the TEXTURE0/TEXTURE1 transforms
	// (fixed-function TCI_CAMERASPACEPOSITION + COUNT2), rows in c4-c7.
	const char c_achTerrainSplatVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avTexMat0[2] : register(c4);\n"
		"float4 g_avTexMat1[2] : register(c6);\n"
		"float4 g_avWorldView[3] : register(c11);\n"
		"float4 g_vFogParams : register(c28);\n"
		"float4 g_vFogParams2 : register(c29);\n"
		"struct VS_INPUT { float3 vPosition : POSITION; float3 vNormal : NORMAL; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vPadDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vSplatCoord : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float4 vCamPos;\n"
		"    vCamPos.x = dot(vPosition, g_avWorldView[0]);\n"
		"    vCamPos.y = dot(vPosition, g_avWorldView[1]);\n"
		"    vCamPos.z = dot(vPosition, g_avWorldView[2]);\n"
		"    vCamPos.w = 1.0f;\n"
		"    Out.vTexCoord.x = dot(vCamPos, g_avTexMat0[0]);\n"
		"    Out.vTexCoord.y = dot(vCamPos, g_avTexMat0[1]);\n"
		"    Out.vSplatCoord.x = dot(vCamPos, g_avTexMat1[0]);\n"
		"    Out.vSplatCoord.y = dot(vCamPos, g_avTexMat1[1]);\n"
		"    float fFogDist = lerp(vCamPos.z, length(vCamPos.xyz), g_vFogParams2.y);\n"
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n"
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    Out.vPadDiffuse = 0;\n"
		"    return Out;\n"
		"}\n";

	// Splat layer: rgb = tile texture, a = splat-map alpha
	// (stage0 MODULATE(tex, white diffuse) + stage1 alpha SELECTARG1(TEXTURE)).
	const char c_achTerrainSplatPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"sampler2D g_kSampler1 : register(s1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vSplatCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = float4(tex2D(g_kSampler0, vTexCoord).rgb, tex2D(g_kSampler1, vSplatCoord).a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// First (base-coat) splat: stage1 ALPHAOP disabled, alpha = tile texture alpha.
	const char c_achTerrainSplatBasePixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = tex2D(g_kSampler0, vTexCoord);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Far-fog flat pass: solid TFACTOR (fog color), alpha stage disabled.
	const char c_achTerrainFogFlatPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vPadCoord0 : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = float4(g_kTFactor.rgb, 1.0f);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Terrain shadow overlay VS: identical position/texgen path as the splat
	// program (the multiply pass re-rasterizes the same patch and must produce
	// bit-identical depths) plus the fixed-function directional lighting the
	// overlay's CURRENT argument consumes; world rows in c14-c16.
	const char c_achTerrainLitShadowVertexProgram[] =
		"float4 g_avWVP[4] : register(c0);\n"
		"float4 g_avTexMat0[2] : register(c4);\n"
		"float4 g_avTexMat1[2] : register(c6);\n"
		"float4 g_vLightDirection : register(c8);\n"
		"float4 g_kLitDiffuse : register(c9);\n"
		"float4 g_kLitAmbient : register(c10);\n"
		"float4 g_avWorldView[3] : register(c11);\n"
		"float4 g_avWorld[3] : register(c14);\n"
		"float4 g_vFogParams : register(c28);\n"
		"float4 g_vFogParams2 : register(c29);\n"
		"struct VS_INPUT { float3 vPosition : POSITION; float3 vNormal : NORMAL; };\n"
		"struct VS_OUTPUT { float4 vPosition : POSITION; float4 vDiffuse : COLOR0; float2 vTexCoord : TEXCOORD0; float2 vShadowCoord : TEXCOORD1; float fFog : TEXCOORD7; float fFixedFog : FOG; };\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    float4 vPosition = float4(In.vPosition, 1.0f);\n"
		"    Out.vPosition.x = dot(vPosition, g_avWVP[0]);\n"
		"    Out.vPosition.y = dot(vPosition, g_avWVP[1]);\n"
		"    Out.vPosition.z = dot(vPosition, g_avWVP[2]);\n"
		"    Out.vPosition.w = dot(vPosition, g_avWVP[3]);\n"
		"    float4 vCamPos;\n"
		"    vCamPos.x = dot(vPosition, g_avWorldView[0]);\n"
		"    vCamPos.y = dot(vPosition, g_avWorldView[1]);\n"
		"    vCamPos.z = dot(vPosition, g_avWorldView[2]);\n"
		"    vCamPos.w = 1.0f;\n"
		"    Out.vTexCoord.x = dot(vCamPos, g_avTexMat0[0]);\n"
		"    Out.vTexCoord.y = dot(vCamPos, g_avTexMat0[1]);\n"
		"    Out.vShadowCoord.x = dot(vCamPos, g_avTexMat1[0]);\n"
		"    Out.vShadowCoord.y = dot(vCamPos, g_avTexMat1[1]);\n"
		"    float4 vNormal = float4(In.vNormal, 0.0f);\n"
		"    float3 vWorldNormal;\n"
		"    vWorldNormal.x = dot(vNormal, g_avWorld[0]);\n"
		"    vWorldNormal.y = dot(vNormal, g_avWorld[1]);\n"
		"    vWorldNormal.z = dot(vNormal, g_avWorld[2]);\n"
		"    float fDot = max(0.0f, dot(vWorldNormal, g_vLightDirection.xyz));\n"
		"    Out.vDiffuse = saturate(g_kLitAmbient + g_kLitDiffuse * fDot);\n"
		"    float fFogDist = lerp(vCamPos.z, length(vCamPos.xyz), g_vFogParams2.y);\n"
		"    Out.fFog = saturate(fFogDist * g_vFogParams.x + g_vFogParams.y) * g_vFogParams.z\n"
		"             + exp2(-fFogDist * g_vFogParams2.x) * g_vFogParams.w;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    return Out;\n"
		"}\n";

	// Static-shadow multiply overlay: lit diffuse times the shadow texture
	// (the ZERO/SRCCOLOR blend multiplies it into the scene).
	const char c_achTerrainShadowPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = float4(tex2D(g_kSampler0, vTexCoord).rgb * vDiffuse.rgb, vDiffuse.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Static + character shadow overlay: stage1 modulates the projected
	// character shadow map on top.
	const char c_achTerrainShadowChrPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"sampler2D g_kSampler1 : register(s1);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vShadowCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float3 vColor = tex2D(g_kSampler0, vTexCoord).rgb * vDiffuse.rgb;\n"
		"    vColor *= tex2D(g_kSampler1, vShadowCoord).rgb;\n"
		"    float4 kFinal = float4(vColor, vDiffuse.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Terrain attr/marked-area overlay: rgb = TFACTOR, alpha = TFACTOR
	// times the marked-splat texture sampled through the TEXTURE1 texgen.
	const char c_achTerrainAttrPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"Texture2D g_kTexture1 : register(t1);\n"
		"SamplerState g_kSampler1 : register(s1);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vPadCoord0 : TEXCOORD0,\n"
		"    float2 vSplatCoord : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kFinal = float4(g_kTFactor.rgb, g_kTFactor.a * g_kTexture1.Sample(g_kSampler1, vSplatCoord).a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Fixed-function D3DTOP_MODULATEINVALPHA_ADDCOLOR(TEXTURE, DIFFUSE), alpha = texture.
	const char c_achInvAlphaAddPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb + vDiffuse.rgb * (1.0f - kTexel.a), kTexel.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// Effect combiners: COLOROP(ARG1=TFACTOR, ARG2=TEXTURE), ALPHAOP = MODULATE.
	const char c_achTFactorModulatePixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb * g_kTFactor.rgb, kTexel.a * g_kTFactor.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";
	const char c_achTFactorAddPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb + g_kTFactor.rgb, kTexel.a * g_kTFactor.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";
	const char c_achTFactorOnlyPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(g_kTFactor.rgb, kTexel.a * g_kTFactor.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";
	const char c_achTexTFactorAlphaPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb, kTexel.a * g_kTFactor.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	// The remaining effect combiners shipped assets actually use
	// (coloroperationtype in .mse files): MODULATE2X, MODULATE4X, ADDSIGNED.
	const char c_achTFactorModulate2XPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb * g_kTFactor.rgb * 2.0f, kTexel.a * g_kTFactor.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";
	const char c_achTFactorModulate4XPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb * g_kTFactor.rgb * 4.0f, kTexel.a * g_kTFactor.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";
	const char c_achTFactorAddSignedPixelProgram[] =
		"#ifdef ALPHA_TEST\n"
		"float4 g_kAlphaRef : register(c2);\n"
		"#endif\n"
		"float4 g_kFogColor : register(c1);\n"
		"sampler2D g_kSampler0 : register(s0);\n"
		"float4 g_kTFactor : register(c0);\n"
		"float4 main(\n"
		"#ifdef SM5\n"
		"    float4 vScreenPosition : SV_POSITION,\n"
		"#endif\n"
		"    float4 vPadDiffuse : COLOR0,\n"
		"    float2 vTexCoord : TEXCOORD0,\n"
		"    float2 vPadCoord1 : TEXCOORD1,\n"
		"    float fFog : TEXCOORD7) : COLOR0\n"
		"{\n"
		"    float4 kTexel = tex2D(g_kSampler0, vTexCoord);\n"
		"    float4 kFinal = float4(kTexel.rgb + g_kTFactor.rgb - 0.5f, kTexel.a * g_kTFactor.a);\n"
		"    kFinal.rgb = lerp(g_kFogColor.rgb, kFinal.rgb, saturate(fFog));\n"
		"#ifdef ALPHA_TEST\n"
		"    clip(kFinal.a - g_kAlphaRef.x);\n"
		"#endif\n"
		"    return kFinal;\n"
		"}\n";

	const char c_achSpeedTreeLeafVertexProgram[] =
		"float4 g_avConstants[86] : register(c0);\n"
		"struct VS_INPUT\n"
		"{\n"
		"    float3 vPosition  : POSITION;\n"
		"    float4 vColor     : COLOR0;\n"
		"    float2 vTexCoord  : TEXCOORD0;\n"
		"    float4 vPlacement : TEXCOORD2;\n"
		"};\n"
		"struct VS_OUTPUT\n"
		"{\n"
		"    float4 vPosition : POSITION;\n"
		"    float4 vDiffuse  : COLOR0;\n"
		"    float2 vTexCoord : TEXCOORD0;\n"
		"    float2 vPadCoord1 : TEXCOORD1;\n"
		"    float  fFog      : TEXCOORD7;\n"
		"    float  fFixedFog : FOG;\n"
		"};\n"
		"VS_OUTPUT main(VS_INPUT In)\n"
		"{\n"
		"    VS_OUTPUT Out;\n"
		"    Out.vTexCoord = In.vTexCoord;\n"
		"    float4 r0 = float4(In.vPosition, 1.0f);\n"
		"    r0 += g_avConstants[(int)In.vPlacement.z] * In.vPlacement.w;\n"
		"    r0 += g_avConstants[52];\n"
		"    Out.vPosition.x = dot(r0, g_avConstants[0]);\n"
		"    Out.vPosition.y = dot(r0, g_avConstants[1]);\n"
		"    Out.vPosition.z = dot(r0, g_avConstants[2]);\n"
		"    Out.vPosition.w = dot(r0, g_avConstants[3]);\n"
		"    Out.fFog = (g_avConstants[85].y - dot(r0, g_avConstants[2])) * g_avConstants[85].z;\n"
		"    Out.vDiffuse = In.vColor;\n"
		"    Out.vPadCoord1 = 0;\n"
		"    Out.fFixedFog = 1.0f;\n"
		"    return Out;\n"
		"}\n";

	// Every pool program by name and source: the DX12 backend compiles
	// the same HLSL against SM5 targets from this table.
	const CGraphicShaderPool::TProgramInfo c_akProgramTable[] =
	{
		{ "SpeedTreeLeafVertexProgram", c_achSpeedTreeLeafVertexProgram, sizeof(c_achSpeedTreeLeafVertexProgram) - 1, true },
		{ "PDTVertexProgram", c_achPDTVertexProgram, sizeof(c_achPDTVertexProgram) - 1, true },
		{ "ModulatePixelProgram", c_achModulatePixelProgram, sizeof(c_achModulatePixelProgram) - 1, false },
		{ "ModulateSpecAlphaPixelProgram", c_achModulateSpecAlphaPixelProgram, sizeof(c_achModulateSpecAlphaPixelProgram) - 1, false },
		{ "ModulateNoFogPixelProgram", c_achModulateNoFogPixelProgram, sizeof(c_achModulateNoFogPixelProgram) - 1, false },
		{ "LitBlendPixelProgram", c_achLitBlendPixelProgram, sizeof(c_achLitBlendPixelProgram) - 1, false },
		{ "LitAddPixelProgram", c_achLitAddPixelProgram, sizeof(c_achLitAddPixelProgram) - 1, false },
		{ "DiffusePixelProgram", c_achDiffusePixelProgram, sizeof(c_achDiffusePixelProgram) - 1, false },
		{ "FlatTFactorPixelProgram", c_achFlatTFactorPixelProgram, sizeof(c_achFlatTFactorPixelProgram) - 1, false },
		{ "PTVertexProgram", c_achPTVertexProgram, sizeof(c_achPTVertexProgram) - 1, true },
		{ "WaterVertexProgram", c_achWaterVertexProgram, sizeof(c_achWaterVertexProgram) - 1, true },
		{ "WaterPixelProgram", c_achWaterPixelProgram, sizeof(c_achWaterPixelProgram) - 1, false },
		{ "TexturePixelProgram", c_achTexturePixelProgram, sizeof(c_achTexturePixelProgram) - 1, false },
		{ "ModulateTexAlphaPixelProgram", c_achModulateTexAlphaPixelProgram, sizeof(c_achModulateTexAlphaPixelProgram) - 1, false },
		{ "LitTwoTexPixelProgram", c_achLitTwoTexPixelProgram, sizeof(c_achLitTwoTexPixelProgram) - 1, false },
		{ "LitTFactorTintPixelProgram", c_achLitTFactorTintPixelProgram, sizeof(c_achLitTFactorTintPixelProgram) - 1, false },
		{ "LitProjectedAlphaPixelProgram", c_achLitProjectedAlphaPixelProgram, sizeof(c_achLitProjectedAlphaPixelProgram) - 1, false },
		{ "MiniMapVertexProgram", c_achMiniMapVertexProgram, sizeof(c_achMiniMapVertexProgram) - 1, true },
		{ "MiniMapPixelProgram", c_achMiniMapPixelProgram, sizeof(c_achMiniMapPixelProgram) - 1, false },
		{ "MiniMapTFactorPixelProgram", c_achMiniMapTFactorPixelProgram, sizeof(c_achMiniMapTFactorPixelProgram) - 1, false },
		{ "PDTTexMatVertexProgram", c_achPDTTexMatVertexProgram, sizeof(c_achPDTTexMatVertexProgram) - 1, true },
		{ "SpeedTreeBranchVertexProgram", c_achSpeedTreeBranchVertexProgram, sizeof(c_achSpeedTreeBranchVertexProgram) - 1, true },
		{ "SpeedTreeShadowPixelProgram", c_achSpeedTreeShadowPixelProgram, sizeof(c_achSpeedTreeShadowPixelProgram) - 1, false },
		{ "PNTLitVertexProgram", c_achPNTLitVertexProgram, sizeof(c_achPNTLitVertexProgram) - 1, true },
		{ "PNTLitOmniVertexProgram", c_achPNTLitOmniVertexProgram, sizeof(c_achPNTLitOmniVertexProgram) - 1, true },
		{ "PNTLitSpecVertexProgram", c_achPNTLitSpecVertexProgram, sizeof(c_achPNTLitSpecVertexProgram) - 1, true },
		{ "LitSpecPixelProgram", c_achLitSpecPixelProgram, sizeof(c_achLitSpecPixelProgram) - 1, false },
		{ "PNT2VertexProgram", c_achPNT2VertexProgram, sizeof(c_achPNT2VertexProgram) - 1, true },
		{ "LightmapPixelProgram", c_achLightmapPixelProgram, sizeof(c_achLightmapPixelProgram) - 1, false },
		{ "PNTLitRecvVertexProgram", c_achPNTLitRecvVertexProgram, sizeof(c_achPNTLitRecvVertexProgram) - 1, true },
		{ "LitShadowPixelProgram", c_achLitShadowPixelProgram, sizeof(c_achLitShadowPixelProgram) - 1, false },
		{ "PNT2RecvVertexProgram", c_achPNT2RecvVertexProgram, sizeof(c_achPNT2RecvVertexProgram) - 1, true },
		{ "TFactorShadowPixelProgram", c_achTFactorShadowPixelProgram, sizeof(c_achTFactorShadowPixelProgram) - 1, false },
		{ "TerrainSplatVertexProgram", c_achTerrainSplatVertexProgram, sizeof(c_achTerrainSplatVertexProgram) - 1, true },
		{ "TerrainSplatPixelProgram", c_achTerrainSplatPixelProgram, sizeof(c_achTerrainSplatPixelProgram) - 1, false },
		{ "TerrainSplatBasePixelProgram", c_achTerrainSplatBasePixelProgram, sizeof(c_achTerrainSplatBasePixelProgram) - 1, false },
		{ "TerrainFogFlatPixelProgram", c_achTerrainFogFlatPixelProgram, sizeof(c_achTerrainFogFlatPixelProgram) - 1, false },
		{ "TerrainLitShadowVertexProgram", c_achTerrainLitShadowVertexProgram, sizeof(c_achTerrainLitShadowVertexProgram) - 1, true },
		{ "TerrainShadowPixelProgram", c_achTerrainShadowPixelProgram, sizeof(c_achTerrainShadowPixelProgram) - 1, false },
		{ "TerrainShadowChrPixelProgram", c_achTerrainShadowChrPixelProgram, sizeof(c_achTerrainShadowChrPixelProgram) - 1, false },
		{ "TerrainAttrPixelProgram", c_achTerrainAttrPixelProgram, sizeof(c_achTerrainAttrPixelProgram) - 1, false },
		{ "InvAlphaAddPixelProgram", c_achInvAlphaAddPixelProgram, sizeof(c_achInvAlphaAddPixelProgram) - 1, false },
		{ "TFactorModulatePixelProgram", c_achTFactorModulatePixelProgram, sizeof(c_achTFactorModulatePixelProgram) - 1, false },
		{ "TFactorAddPixelProgram", c_achTFactorAddPixelProgram, sizeof(c_achTFactorAddPixelProgram) - 1, false },
		{ "TFactorOnlyPixelProgram", c_achTFactorOnlyPixelProgram, sizeof(c_achTFactorOnlyPixelProgram) - 1, false },
		{ "TexTFactorAlphaPixelProgram", c_achTexTFactorAlphaPixelProgram, sizeof(c_achTexTFactorAlphaPixelProgram) - 1, false },
		{ "TFactorModulate2XPixelProgram", c_achTFactorModulate2XPixelProgram, sizeof(c_achTFactorModulate2XPixelProgram) - 1, false },
		{ "TFactorModulate4XPixelProgram", c_achTFactorModulate4XPixelProgram, sizeof(c_achTFactorModulate4XPixelProgram) - 1, false },
		{ "TFactorAddSignedPixelProgram", c_achTFactorAddSignedPixelProgram, sizeof(c_achTFactorAddSignedPixelProgram) - 1, false },
	};
}

CGraphicShaderPool::CGraphicShaderPool()
	: m_bCreateFailed(false)
	, m_lpPDTVertexShader(NULL)
	, m_lpPTVertexShader(NULL)
	, m_lpWaterVertexShader(NULL)
	, m_lpPDTTexMatVertexShader(NULL)
	, m_lpMiniMapVertexShader(NULL)
	, m_lpPNTLitVertexShader(NULL)
	, m_lpSpeedTreeBranchVertexShader(NULL)
	, m_lpPNTLitSpecVertexShader(NULL)
	, m_lpPNTLitOmniVertexShader(NULL)
	, m_lpPNT2VertexShader(NULL)
	, m_lpPNT2RecvVertexShader(NULL)
	, m_lpPNTLitRecvVertexShader(NULL)
	, m_lpTerrainSplatVertexShader(NULL)
	, m_lpTerrainLitShadowVertexShader(NULL)
	, m_lpModulatePixelShader(NULL)
	, m_lpModulateSpecAlphaPixelShader(NULL)
	, m_lpModulateNoFogPixelShader(NULL)
	, m_lpLitBlendPixelShader(NULL)
	, m_lpLitAddPixelShader(NULL)
	, m_lpDiffusePixelShader(NULL)
	, m_lpFlatTFactorPixelShader(NULL)
	, m_lpTexturePixelShader(NULL)
	, m_lpWaterPixelShader(NULL)
	, m_lpModulateTexAlphaPixelShader(NULL)
	, m_lpLitTwoTexPixelShader(NULL)
	, m_lpLitTFactorTintPixelShader(NULL)
	, m_lpLitProjectedAlphaPixelShader(NULL)
	, m_lpMiniMapPixelShader(NULL)
	, m_lpMiniMapTFactorPixelShader(NULL)
	, m_lpInvAlphaAddPixelShader(NULL)
	, m_lpTFactorModulatePixelShader(NULL)
	, m_lpTFactorAddPixelShader(NULL)
	, m_lpTFactorOnlyPixelShader(NULL)
	, m_lpTexTFactorAlphaPixelShader(NULL)
	, m_lpTFactorModulate2XPixelShader(NULL)
	, m_lpTFactorModulate4XPixelShader(NULL)
	, m_lpTFactorAddSignedPixelShader(NULL)
	, m_lpLitSpecPixelShader(NULL)
	, m_lpSpeedTreeShadowPixelShader(NULL)
	, m_lpLightmapPixelShader(NULL)
	, m_lpLitShadowPixelShader(NULL)
	, m_lpTFactorShadowPixelShader(NULL)
	, m_lpTerrainSplatPixelShader(NULL)
	, m_lpTerrainSplatBasePixelShader(NULL)
	, m_lpTerrainFogFlatPixelShader(NULL)
	, m_lpTerrainAttrPixelShader(NULL)
	, m_lpTerrainShadowPixelShader(NULL)
	, m_lpTerrainShadowChrPixelShader(NULL)
	, m_lpPDTDeclaration(NULL)
	, m_lpPTDeclaration(NULL)
	, m_lpPDDeclaration(NULL)
	, m_lpPNTDeclaration(NULL)
	, m_lpPNT2Declaration(NULL)
	, m_lpPDT2Declaration(NULL)
	, m_lpPNDeclaration(NULL)
{
}

CGraphicShaderPool::~CGraphicShaderPool()
{
	Destroy();
}

void CGraphicShaderPool::Destroy()
{
	m_lpPDTVertexShader = NULL;
	m_lpPTVertexShader = NULL;
	m_lpWaterVertexShader = NULL;
	m_lpPDTTexMatVertexShader = NULL;
	m_lpMiniMapVertexShader = NULL;
	m_lpPNTLitVertexShader = NULL;
	m_lpSpeedTreeBranchVertexShader = NULL;
	m_lpPNTLitSpecVertexShader = NULL;
	m_lpPNTLitOmniVertexShader = NULL;
	m_lpPNT2VertexShader = NULL;
	m_lpPNT2RecvVertexShader = NULL;
	m_lpPNTLitRecvVertexShader = NULL;
	m_lpTerrainSplatVertexShader = NULL;
	m_lpTerrainLitShadowVertexShader = NULL;
	m_lpModulatePixelShader = NULL;
	m_lpModulateSpecAlphaPixelShader = NULL;
	m_lpModulateNoFogPixelShader = NULL;
	m_lpLitBlendPixelShader = NULL;
	m_lpLitAddPixelShader = NULL;
	m_lpDiffusePixelShader = NULL;
	m_lpFlatTFactorPixelShader = NULL;
	m_lpTexturePixelShader = NULL;
	m_lpWaterPixelShader = NULL;
	m_lpModulateTexAlphaPixelShader = NULL;
	m_lpLitTwoTexPixelShader = NULL;
	m_lpLitTFactorTintPixelShader = NULL;
	m_lpLitProjectedAlphaPixelShader = NULL;
	m_lpMiniMapPixelShader = NULL;
	m_lpMiniMapTFactorPixelShader = NULL;
	m_lpInvAlphaAddPixelShader = NULL;
	m_lpTFactorModulatePixelShader = NULL;
	m_lpTFactorAddPixelShader = NULL;
	m_lpTFactorOnlyPixelShader = NULL;
	m_lpTexTFactorAlphaPixelShader = NULL;
	m_lpTFactorModulate2XPixelShader = NULL;
	m_lpTFactorModulate4XPixelShader = NULL;
	m_lpTFactorAddSignedPixelShader = NULL;
	m_lpLitSpecPixelShader = NULL;
	m_lpSpeedTreeShadowPixelShader = NULL;
	m_lpLightmapPixelShader = NULL;
	m_lpLitShadowPixelShader = NULL;
	m_lpTFactorShadowPixelShader = NULL;
	m_lpTerrainSplatPixelShader = NULL;
	m_lpTerrainSplatBasePixelShader = NULL;
	m_lpTerrainFogFlatPixelShader = NULL;
	m_lpTerrainAttrPixelShader = NULL;
	m_lpTerrainShadowPixelShader = NULL;
	m_lpTerrainShadowChrPixelShader = NULL;
	m_lpPDTDeclaration = NULL;
	m_lpPTDeclaration = NULL;
	m_lpPDDeclaration = NULL;
	m_lpPNTDeclaration = NULL;
	m_lpPNT2Declaration = NULL;
	m_lpPDT2Declaration = NULL;
	m_lpPNDeclaration = NULL;

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

	m_lpPDTVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpModulatePixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpModulateSpecAlphaPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpModulateNoFogPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpLitBlendPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpLitAddPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpDiffusePixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpFlatTFactorPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);

	if (NULL == (m_lpPDTDeclaration = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(akPDTElements)))
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

	m_lpPTVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpTexturePixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpModulateTexAlphaPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpLitTwoTexPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpLitTFactorTintPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpLitProjectedAlphaPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpMiniMapVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpMiniMapPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpMiniMapTFactorPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpPDTTexMatVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpInvAlphaAddPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTFactorModulatePixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTFactorAddPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTFactorOnlyPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTexTFactorAlphaPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTFactorModulate2XPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTFactorModulate4XPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTFactorAddSignedPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);

	if (NULL == (m_lpPTDeclaration = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(akPTElements)))
	{
		TraceError("CGraphicShaderPool: failed to create PT vertex declaration.");
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	const D3DVERTEXELEMENT9 akPDElements[] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
		D3DDECL_END()
	};

	if (NULL == (m_lpPDDeclaration = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(akPDElements)))
	{
		TraceError("CGraphicShaderPool: failed to create PD vertex declaration.");
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	m_lpWaterVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpWaterPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);

	const D3DVERTEXELEMENT9 akPNTElements[] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	m_lpPNTLitVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpSpeedTreeBranchVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpSpeedTreeShadowPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);

	const D3DVERTEXELEMENT9 akPDT2Elements[] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END()
	};

	if (NULL == (m_lpPDT2Declaration = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(akPDT2Elements)))
	{
		TraceError("CGraphicShaderPool: failed to create PDT2 vertex declaration.");
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	m_lpPNTLitSpecVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpPNTLitOmniVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpLitSpecPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpPNT2VertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpLightmapPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpPNTLitRecvVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpLitShadowPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpPNT2RecvVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpTFactorShadowPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);

	const D3DVERTEXELEMENT9 akPNT2Elements[] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END()
	};

	if (NULL == (m_lpPNT2Declaration = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(akPNT2Elements)))
	{
		TraceError("CGraphicShaderPool: failed to create PNT2 vertex declaration.");
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	const D3DVERTEXELEMENT9 akPNElements[] =
	{
		{ 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
		D3DDECL_END()
	};

	if (NULL == (m_lpPNDeclaration = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(akPNElements)))
	{
		TraceError("CGraphicShaderPool: failed to create PN vertex declaration.");
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	m_lpTerrainSplatVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpTerrainSplatPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTerrainSplatBasePixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTerrainFogFlatPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTerrainAttrPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTerrainLitShadowVertexShader = (LPDIRECT3DVERTEXSHADER9)STATEMANAGER.CreateVertexShader(NULL);
	m_lpTerrainShadowPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);
	m_lpTerrainShadowChrPixelShader = (LPDIRECT3DPIXELSHADER9)STATEMANAGER.CreatePixelShader(NULL);

	if (NULL == (m_lpPNTDeclaration = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(akPNTElements)))
	{
		TraceError("CGraphicShaderPool: failed to create PNT vertex declaration.");
		Destroy();
		m_bCreateFailed = true;
		return false;
	}

	// DX12 mirror: map every created shader to its registry program.
	if (CStateManager::InstancePtr())
	{
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPDTVertexShader, "PDTVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpModulatePixelShader, "ModulatePixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpModulateSpecAlphaPixelShader, "ModulateSpecAlphaPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpModulateNoFogPixelShader, "ModulateNoFogPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpLitBlendPixelShader, "LitBlendPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpLitAddPixelShader, "LitAddPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpDiffusePixelShader, "DiffusePixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpFlatTFactorPixelShader, "FlatTFactorPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPTVertexShader, "PTVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTexturePixelShader, "TexturePixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpModulateTexAlphaPixelShader, "ModulateTexAlphaPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpLitTwoTexPixelShader, "LitTwoTexPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpLitTFactorTintPixelShader, "LitTFactorTintPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpLitProjectedAlphaPixelShader, "LitProjectedAlphaPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpMiniMapVertexShader, "MiniMapVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpMiniMapPixelShader, "MiniMapPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpMiniMapTFactorPixelShader, "MiniMapTFactorPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPDTTexMatVertexShader, "PDTTexMatVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpInvAlphaAddPixelShader, "InvAlphaAddPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTFactorModulatePixelShader, "TFactorModulatePixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTFactorAddPixelShader, "TFactorAddPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTFactorOnlyPixelShader, "TFactorOnlyPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTexTFactorAlphaPixelShader, "TexTFactorAlphaPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTFactorModulate2XPixelShader, "TFactorModulate2XPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTFactorModulate4XPixelShader, "TFactorModulate4XPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTFactorAddSignedPixelShader, "TFactorAddSignedPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpWaterVertexShader, "WaterVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpWaterPixelShader, "WaterPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPNTLitVertexShader, "PNTLitVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpSpeedTreeBranchVertexShader, "SpeedTreeBranchVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpSpeedTreeShadowPixelShader, "SpeedTreeShadowPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPNTLitSpecVertexShader, "PNTLitSpecVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPNTLitOmniVertexShader, "PNTLitOmniVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpLitSpecPixelShader, "LitSpecPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPNT2VertexShader, "PNT2VertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpLightmapPixelShader, "LightmapPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPNTLitRecvVertexShader, "PNTLitRecvVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpLitShadowPixelShader, "LitShadowPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpPNT2RecvVertexShader, "PNT2RecvVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTFactorShadowPixelShader, "TFactorShadowPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTerrainSplatVertexShader, "TerrainSplatVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTerrainSplatPixelShader, "TerrainSplatPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTerrainSplatBasePixelShader, "TerrainSplatBasePixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTerrainFogFlatPixelShader, "TerrainFogFlatPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTerrainAttrPixelShader, "TerrainAttrPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTerrainLitShadowVertexShader, "TerrainLitShadowVertexProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTerrainShadowPixelShader, "TerrainShadowPixelProgram");
	STATEMANAGER.RegisterShaderProgramDX12(m_lpTerrainShadowChrPixelShader, "TerrainShadowChrPixelProgram");
	}

	return true;
}

bool CGraphicShaderPool::__Bind(LPDIRECT3DVERTEXDECLARATION9 lpDeclaration, LPDIRECT3DVERTEXSHADER9 lpVertexShader, LPDIRECT3DPIXELSHADER9 lpPixelShader)
{
	if (!m_lpPDTDeclaration && !__Create())
		return false;

	// The caller reads its member arguments BEFORE the create above runs, so on
	// the very first bind they are still the pre-creation NULLs; binding those
	// would draw with no declaration/shader at all. Fall back to fixed-function
	// for this draw - the next bind picks up the freshly created objects.
	if (!lpDeclaration || !lpVertexShader || !lpPixelShader)
		return false;

	D3DXMATRIX matWorld, matView, matProj, matWorldView, matWVP;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	STATEMANAGER.GetTransform(D3DTS_VIEW, &matView);
	STATEMANAGER.GetTransform(D3DTS_PROJECTION, &matProj);
	D3DXMatrixMultiply(&matWorldView, &matWorld, &matView);
	D3DXMatrixMultiply(&matWVP, &matWorldView, &matProj);
	D3DXMatrixTranspose(&matWVP, &matWVP);
	STATEMANAGER.SetVertexShaderConstant(0, &matWVP, 4);
	D3DXMatrixTranspose(&matWorldView, &matWorldView);
	STATEMANAGER.SetVertexShaderConstant(11, &matWorldView, 3);

	// Mirror the fixed-function vertex-fog states into c28/c29: a LINEAR ramp
	// (d * x + y, also covering the fog-off case as a constant 1) or an EXP
	// curve, each gated by its select flag, over the plain or radial distance.
	D3DXVECTOR4 avFogParams[2];
	avFogParams[0] = D3DXVECTOR4(0.0f, 1.0f, 1.0f, 0.0f);
	avFogParams[1] = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);
	if (STATEMANAGER.GetRenderState(D3DRS_FOGENABLE))
	{
		const DWORD dwFogMode = STATEMANAGER.GetRenderState(D3DRS_FOGVERTEXMODE);
		if (D3DFOG_LINEAR == dwFogMode)
		{
			const DWORD dwStart = STATEMANAGER.GetRenderState(D3DRS_FOGSTART);
			const DWORD dwEnd = STATEMANAGER.GetRenderState(D3DRS_FOGEND);
			const float fStart = *reinterpret_cast<const float*>(&dwStart);
			const float fEnd = *reinterpret_cast<const float*>(&dwEnd);
			// A degenerate range degrades to a hard cutoff at the end distance.
			const float fRange = (fEnd - fStart > 0.0001f) ? (fEnd - fStart) : 0.0001f;
			avFogParams[0].x = -1.0f / fRange;
			avFogParams[0].y = fEnd / fRange;
		}
		else if (D3DFOG_EXP == dwFogMode)
		{
			const DWORD dwDensity = STATEMANAGER.GetRenderState(D3DRS_FOGDENSITY);
			avFogParams[0].z = 0.0f;
			avFogParams[0].w = 1.0f;
			// exp(-d * density) evaluated as exp2: fold in log2(e).
			avFogParams[1].x = *reinterpret_cast<const float*>(&dwDensity) * 1.442695f;
		}
		avFogParams[1].y = STATEMANAGER.GetRenderState(D3DRS_RANGEFOGENABLE) ? 1.0f : 0.0f;
	}
	STATEMANAGER.SetVertexShaderConstant(28, avFogParams, 2);

	STATEMANAGER.SetVertexDeclaration(lpDeclaration);
	STATEMANAGER.SetVertexShader(lpVertexShader);
	STATEMANAGER.SetPixelShader(lpPixelShader);
	return true;
}

bool CGraphicShaderPool::BindPDTModulate()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpModulatePixelShader);
}

bool CGraphicShaderPool::BindPDTTFactorModulate()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpTFactorModulatePixelShader);
}

bool CGraphicShaderPool::BindPDTDiffuse()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpDiffusePixelShader);
}

bool CGraphicShaderPool::BindPTTexture()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTexturePixelShader);
}

bool CGraphicShaderPool::BindWater(bool bTexture)
{
	if (!__Bind(m_lpPDDeclaration, m_lpWaterVertexShader,
				bTexture ? m_lpWaterPixelShader : m_lpDiffusePixelShader))
		return false;

	D3DXMATRIX matTexture;
	STATEMANAGER.GetTransform(D3DTS_TEXTURE0, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(4, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindPDTTexture()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpTexturePixelShader);
}

bool CGraphicShaderPool::BindPDTModulateTexAlpha()
{
	return __Bind(m_lpPDTDeclaration, m_lpPDTVertexShader, m_lpModulateTexAlphaPixelShader);
}

bool CGraphicShaderPool::BindMiniMap(bool bTexture)
{
	if (!__Bind(m_lpPTDeclaration, m_lpMiniMapVertexShader,
				bTexture ? m_lpMiniMapPixelShader : m_lpMiniMapTFactorPixelShader))
		return false;

	D3DXMATRIX matTexture;
	STATEMANAGER.GetTransform(D3DTS_TEXTURE1, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(6, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindPDTTexMatInvAlphaAdd()
{
	if (!__Bind(m_lpPDTDeclaration, m_lpPDTTexMatVertexShader, m_lpInvAlphaAddPixelShader))
		return false;

	D3DXMATRIX matTexture;
	STATEMANAGER.GetTransform(D3DTS_TEXTURE0, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(4, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindPTTFactorModulate()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTFactorModulatePixelShader);
}

bool CGraphicShaderPool::BindPTTFactorAdd()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTFactorAddPixelShader);
}

bool CGraphicShaderPool::BindPTTFactorOnly()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTFactorOnlyPixelShader);
}

bool CGraphicShaderPool::BindPTTexTFactorAlpha()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTexTFactorAlphaPixelShader);
}

bool CGraphicShaderPool::BindPTTFactorModulate2X()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTFactorModulate2XPixelShader);
}

bool CGraphicShaderPool::BindPTTFactorModulate4X()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTFactorModulate4XPixelShader);
}

bool CGraphicShaderPool::BindPTTFactorAddSigned()
{
	return __Bind(m_lpPTDeclaration, m_lpPTVertexShader, m_lpTFactorAddSignedPixelShader);
}

bool CGraphicShaderPool::BindPNTLitTexAlpha()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitVertexShader, m_lpModulateTexAlphaPixelShader))
		return false;

	D3DXMATRIX matWorld;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	return true;
}

bool CGraphicShaderPool::BindPNTLitTwoTexture()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitVertexShader, m_lpLitTwoTexPixelShader))
		return false;

	D3DXMATRIX matWorld;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	return true;
}

bool CGraphicShaderPool::BindPNTLitTFactorTint()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitVertexShader, m_lpLitTFactorTintPixelShader))
		return false;

	D3DXMATRIX matWorld;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	return true;
}

bool CGraphicShaderPool::BindPNTLit()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitVertexShader, m_lpModulatePixelShader))
		return false;

	D3DXMATRIX matWorld;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	return true;
}

bool CGraphicShaderPool::BindSpeedTreeBranch(bool bSelfShadow)
{
	return __Bind(m_lpPDT2Declaration, m_lpSpeedTreeBranchVertexShader,
				  bSelfShadow ? m_lpSpeedTreeShadowPixelShader : m_lpModulatePixelShader);
}

bool CGraphicShaderPool::BindPNTLitBlend()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitVertexShader, m_lpLitBlendPixelShader))
		return false;

	D3DXMATRIX matWorld;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	return true;
}

bool CGraphicShaderPool::BindPNTLitAdd()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitVertexShader, m_lpLitAddPixelShader))
		return false;

	D3DXMATRIX matWorld;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	return true;
}

bool CGraphicShaderPool::BindPNTFlatTFactor()
{
	return __Bind(m_lpPNTDeclaration, m_lpPNTLitVertexShader, m_lpFlatTFactorPixelShader);
}

bool CGraphicShaderPool::BindPNTLitSpecular()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitSpecVertexShader, m_lpLitSpecPixelShader))
		return false;

	D3DXMATRIX matWorld, matTexture;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	STATEMANAGER.GetTransform(D3DTS_TEXTURE1, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(15, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindPNTLitOmni(bool bSpecular)
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitOmniVertexShader,
				bSpecular ? m_lpModulateSpecAlphaPixelShader : m_lpModulatePixelShader))
		return false;

	D3DXMATRIX matWorld;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	return true;
}

bool CGraphicShaderPool::BindPNT2Lightmap()
{
	return __Bind(m_lpPNT2Declaration, m_lpPNT2VertexShader, m_lpLightmapPixelShader);
}

bool CGraphicShaderPool::BindPNTLitShadowReceiver()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitRecvVertexShader, m_lpLitShadowPixelShader))
		return false;

	D3DXMATRIX matWorld, matTexture;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	STATEMANAGER.GetTransform(D3DTS_TEXTURE1, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(15, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindPNTLitProjectedAlpha()
{
	if (!__Bind(m_lpPNTDeclaration, m_lpPNTLitRecvVertexShader, m_lpLitProjectedAlphaPixelShader))
		return false;

	D3DXMATRIX matWorld, matTexture;
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matWorld);
	D3DXMatrixTranspose(&matWorld, &matWorld);
	STATEMANAGER.SetVertexShaderConstant(4, &matWorld, 3);
	STATEMANAGER.GetTransform(D3DTS_TEXTURE1, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(15, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindPNT2ShadowReceiver()
{
	if (!__Bind(m_lpPNT2Declaration, m_lpPNT2RecvVertexShader, m_lpTFactorShadowPixelShader))
		return false;

	D3DXMATRIX matTexture;
	STATEMANAGER.GetTransform(D3DTS_TEXTURE1, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(15, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindTerrainSplat(bool bBase)
{
	if (!__Bind(m_lpPNDeclaration, m_lpTerrainSplatVertexShader,
				bBase ? m_lpTerrainSplatBasePixelShader : m_lpTerrainSplatPixelShader))
		return false;

	D3DXMATRIX matTexture;
	STATEMANAGER.GetTransform(D3DTS_TEXTURE0, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(4, &matTexture, 2);
	STATEMANAGER.GetTransform(D3DTS_TEXTURE1, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(6, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindTerrainFogFlat()
{
	return __Bind(m_lpPNDeclaration, m_lpTerrainSplatVertexShader, m_lpTerrainFogFlatPixelShader);
}

bool CGraphicShaderPool::BindTerrainAttr()
{
	if (!__Bind(m_lpPNDeclaration, m_lpTerrainSplatVertexShader, m_lpTerrainAttrPixelShader))
		return false;

	D3DXMATRIX matTexture;
	STATEMANAGER.GetTransform(D3DTS_TEXTURE1, &matTexture);
	D3DXMatrixTranspose(&matTexture, &matTexture);
	STATEMANAGER.SetVertexShaderConstant(6, &matTexture, 2);
	return true;
}

bool CGraphicShaderPool::BindTerrainShadow(bool bChrShadow)
{
	if (!__Bind(m_lpPNDeclaration, m_lpTerrainLitShadowVertexShader,
				bChrShadow ? m_lpTerrainShadowChrPixelShader : m_lpTerrainShadowPixelShader))
		return false;

	D3DXMATRIX matTemp;
	STATEMANAGER.GetTransform(D3DTS_TEXTURE0, &matTemp);
	D3DXMatrixTranspose(&matTemp, &matTemp);
	STATEMANAGER.SetVertexShaderConstant(4, &matTemp, 2);
	STATEMANAGER.GetTransform(D3DTS_TEXTURE1, &matTemp);
	D3DXMatrixTranspose(&matTemp, &matTemp);
	STATEMANAGER.SetVertexShaderConstant(6, &matTemp, 2);
	STATEMANAGER.GetTransform(D3DTS_WORLD, &matTemp);
	D3DXMatrixTranspose(&matTemp, &matTemp);
	STATEMANAGER.SetVertexShaderConstant(14, &matTemp, 3);
	return true;
}


bool CGraphicShaderPool::BindPixelOnlyModulate()
{
	if (!m_lpPDTDeclaration && !__Create())
		return false;

	if (!m_lpModulatePixelShader)
		return false;

	// The SpeedTree leaf vertices already carry world-space positions plus the
	// tree offset in c52, so c0-c3 must hold VIEW*PROJECTION without the world
	// transform the branch/frond binds leave behind. The leaf vertex program
	// feeds its own fog factor through TEXCOORD7, so the fogged modulate
	// pixel program applies distance fog like the legacy path.
	D3DXMATRIX matView, matProj, matViewProj;
	STATEMANAGER.GetTransform(D3DTS_VIEW, &matView);
	STATEMANAGER.GetTransform(D3DTS_PROJECTION, &matProj);
	D3DXMatrixMultiply(&matViewProj, &matView, &matProj);
	D3DXMatrixTranspose(&matViewProj, &matViewProj);
	STATEMANAGER.SetVertexShaderConstant(0, &matViewProj, 4);

	STATEMANAGER.SetPixelShader(m_lpModulatePixelShader);
	return true;
}

void CGraphicShaderPool::UnbindPixelOnly()
{
	STATEMANAGER.SetPixelShader(NULL);
}

void CGraphicShaderPool::Unbind()
{
	STATEMANAGER.SetVertexShader(NULL);
	STATEMANAGER.SetPixelShader(NULL);
}

UINT CGraphicShaderPool::GetProgramCount()
{
	return sizeof(c_akProgramTable) / sizeof(c_akProgramTable[0]);
}

const CGraphicShaderPool::TProgramInfo* CGraphicShaderPool::GetProgramInfo(UINT uIndex)
{
	if (uIndex >= GetProgramCount())
		return NULL;
	return &c_akProgramTable[uIndex];
}
