#include "StdAfx.h"
#include "StateManager.h"
#include "GrpBackendDX12.h"
#include "GraphicShaderPool.h"

#define StateManager_Assert(a) assert(a)

static uintptr_t gs_nextHandle = 1;

struct SLightData
{
	enum
	{
		LIGHT_NUM = 8,
	};
	D3DLIGHT9 m_akD3DLight[LIGHT_NUM];
	BOOL m_abLightEnable[LIGHT_NUM];
} m_kLightData;



void CStateManager::SetLight(DWORD index, CONST D3DLIGHT9* pLight)
{
	assert(index<SLightData::LIGHT_NUM);
	m_kLightData.m_akD3DLight[index]=*pLight;
}

void CStateManager::LightEnable(DWORD index, BOOL bEnable)
{
	assert(index<SLightData::LIGHT_NUM);
	m_kLightData.m_abLightEnable[index]=bEnable;
}

HRESULT CStateManager::Clear(DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
		pkBackend->ClearTargets(Flags, Color, Z, Stencil);
	return D3D_OK;
}

HRESULT CStateManager::SetViewport(const D3DVIEWPORT9* pViewport)
{
	if (pViewport)
	{
		if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
			pkBackend->SetViewport(*pViewport);

		m_kViewport = *pViewport;
	}
	return D3D_OK;
}

HRESULT CStateManager::GetViewport(D3DVIEWPORT9* pViewport)
{
	if (pViewport)
		*pViewport = m_kViewport;
	return D3D_OK;
}

void CStateManager::SaveViewport()
{
	m_SavedViewport = m_kViewport;
}

void CStateManager::RestoreViewport()
{
	SetViewport(&m_SavedViewport);
}

const void* CStateManager::GetRenderTarget(DWORD)
{
	return NULL;
}

HRESULT CStateManager::SetRenderTarget(DWORD RenderTargetIndex, const void* pRenderTarget)
{
	if (0 != RenderTargetIndex)
		return D3D_OK;

	if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
	{
		if (!pRenderTarget)
			pkBackend->RestoreDefaultTarget();
		else if (!pkBackend->SetRenderTargetTexture(pRenderTarget))
			pkBackend->RestoreDefaultTarget();
	}
	return D3D_OK;
}

const void* CStateManager::GetDepthStencilSurface()
{
	return NULL;
}

HRESULT CStateManager::SetDepthStencilSurface(const void*)
{
	return D3D_OK;
}

const void* CStateManager::CreateVertexShader(CONST DWORD*)
{
	return reinterpret_cast<const void*>(gs_nextHandle++);
}

const void* CStateManager::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements)
{
	const void* pkDecl = reinterpret_cast<const void*>(gs_nextHandle++);

	if (CGraphicBackendDX12::GetInstance())
	{
		TInputLayoutDX12 kLayout;
		if (CGraphicInputLayoutDX12::Build(pVertexElements, kLayout.akElements, 16, &kLayout.uElementCount))
		{
			kLayout.uLayoutID = m_uNextLayoutIDDX12++;
			m_kDeclLayoutMapDX12[pkDecl] = kLayout;
		}
	}
	return pkDecl;
}

const void* CStateManager::CreatePixelShader(CONST DWORD*)
{
	return reinterpret_cast<const void*>(gs_nextHandle++);
}

void CStateManager::GetLight(DWORD index, D3DLIGHT9* pLight)
{
	assert(index<8);
	*pLight=m_kLightData.m_akD3DLight[index];
}

BOOL CStateManager::GetLightEnable(DWORD index)
{
	assert(index<SLightData::LIGHT_NUM);
	return m_kLightData.m_abLightEnable[index];
}

bool CStateManager::BeginScene()
{
	m_bScene=true;

	D3DXMATRIX m4Proj;
	D3DXMATRIX m4View;
	D3DXMATRIX m4World;
	GetTransform(D3DTS_WORLD, &m4World);
	GetTransform(D3DTS_PROJECTION, &m4Proj);
	GetTransform(D3DTS_VIEW, &m4View);
	SetTransform(D3DTS_WORLD, &m4World);
	SetTransform(D3DTS_PROJECTION, &m4Proj);
	SetTransform(D3DTS_VIEW, &m4View);

	return true;
}

void CStateManager::EndScene()
{
	m_bScene=false;
}

CStateManager::CStateManager()
{
	Initialize();
}

CStateManager::~CStateManager()
{
}

void CStateManager::Initialize()
{
	m_bScene = false;
	m_bForce = false;
	m_dwBestMinFilter = D3DTEXF_ANISOTROPIC;
	m_dwBestMagFilter = D3DTEXF_ANISOTROPIC;

	m_kViewport.X = 0;
	m_kViewport.Y = 0;
	m_kViewport.Width = 0;
	m_kViewport.Height = 0;
	m_kViewport.MinZ = 0.0f;
	m_kViewport.MaxZ = 1.0f;
	m_SavedViewport = m_kViewport;

	SetDefaultState();

	for (int i = 0; i < 8; ++i)
		SetSamplerState(i, D3DSAMP_MAXANISOTROPY, 4);
}

void CStateManager::SetBestFiltering(DWORD dwStage)
{
	SetSamplerState(dwStage, D3DSAMP_MINFILTER,	m_dwBestMinFilter);
	SetSamplerState(dwStage, D3DSAMP_MAGFILTER, m_dwBestMagFilter);
	SetSamplerState(dwStage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
}

void CStateManager::Restore()
{
	int i, j;

	m_bForce = true;

	for (i = 0; i < STATEMANAGER_MAX_RENDERSTATES; ++i)
		SetRenderState(D3DRENDERSTATETYPE(i), m_CurrentState.m_RenderStates[i]);

	for (i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
		for (j = 0; j < STATEMANAGER_MAX_TEXTURESTATES; ++j)
			SetTextureStageState(i, D3DTEXTURESTAGESTATETYPE(j), m_CurrentState.m_TextureStates[i][j]);

	for (i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
		for (j = 0; j < STATEMANAGER_MAX_TEXTURESTATES; ++j)
			SetSamplerState(i, D3DSAMPLERSTATETYPE(j), m_CurrentState.m_SamplerStates[i][j]);

	for (i = 0; i < STATEMANAGER_MAX_STAGES; ++i)
		SetTexture(i, m_CurrentState.m_Textures[i]);

	m_bForce = false;
}

void CStateManager::SetDefaultState()
{
	m_CurrentState.ResetState();
	m_CopyState.ResetState();
	m_ChipState.ResetState();

	m_bScene = false;
	m_bForce = true;

	D3DXMATRIX Identity;
	D3DXMatrixIdentity(&Identity);

	SetTransform(D3DTS_WORLD, &Identity);
	SetTransform(D3DTS_VIEW, &Identity);
	SetTransform(D3DTS_PROJECTION, &Identity);

	D3DMATERIAL9 DefaultMat;
	ZeroMemory(&DefaultMat, sizeof(D3DMATERIAL9));

	DefaultMat.Diffuse.r = 1.0f;
	DefaultMat.Diffuse.g = 1.0f;
	DefaultMat.Diffuse.b = 1.0f;
	DefaultMat.Diffuse.a = 1.0f;
	DefaultMat.Ambient.r = 1.0f;
	DefaultMat.Ambient.g = 1.0f;
	DefaultMat.Ambient.b = 1.0f;
	DefaultMat.Ambient.a = 1.0f;
	DefaultMat.Emissive.r = 0.0f;
	DefaultMat.Emissive.g = 0.0f;
	DefaultMat.Emissive.b = 0.0f;
	DefaultMat.Emissive.a = 0.0f;
	DefaultMat.Specular.r = 0.0f;
	DefaultMat.Specular.g = 0.0f;
	DefaultMat.Specular.b = 0.0f;
	DefaultMat.Specular.a = 0.0f;
	DefaultMat.Power = 0.0f;

	SetMaterial(&DefaultMat);

	SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);

	SetRenderState(D3DRS_LASTPIXEL, FALSE);
	SetRenderState(D3DRS_ALPHAREF, 1);
	SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
	SetRenderState(D3DRS_FOGSTART, 0);
	SetRenderState(D3DRS_FOGEND, 0);
	SetRenderState(D3DRS_FOGDENSITY, 0);
	SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	SetRenderState(D3DRS_AMBIENT, 0x00000000);
	SetRenderState(D3DRS_LOCALVIEWER, FALSE);
	SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
	SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
	SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
	SaveVertexProcessing(FALSE);
	SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);
	SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
	SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
	SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
	SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
	SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	SetRenderState(D3DRS_FOGENABLE, FALSE);
	SetRenderState(D3DRS_FOGCOLOR, 0xFF000000);
	SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
	SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
	SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
	SetRenderState(D3DRS_ZENABLE, TRUE);
	SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	SetRenderState(D3DRS_DEPTHBIAS, 0);
	SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, 0);
	SetRenderState(D3DRS_DITHERENABLE, TRUE);
	SetRenderState(D3DRS_STENCILENABLE, FALSE);
	SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	SetRenderState(D3DRS_CLIPPING, TRUE);
	SetRenderState(D3DRS_LIGHTING, FALSE);
	SetRenderState(D3DRS_SPECULARENABLE, FALSE);
	SetRenderState(D3DRS_COLORVERTEX, FALSE);
	SetRenderState(D3DRS_WRAP0, 0);
	SetRenderState(D3DRS_WRAP1, 0);
	SetRenderState(D3DRS_WRAP2, 0);
	SetRenderState(D3DRS_WRAP3, 0);
	SetRenderState(D3DRS_WRAP4, 0);
	SetRenderState(D3DRS_WRAP5, 0);
	SetRenderState(D3DRS_WRAP6, 0);
	SetRenderState(D3DRS_WRAP7, 0);

	SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
	SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

	SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(2, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(2, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(2, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(2, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(3, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(3, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(3, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(3, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(3, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(3, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(4, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(4, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(4, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(4, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(4, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(4, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(5, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(5, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(5, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(5, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(5, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(5, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(6, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(6, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(6, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(6, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(6, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(6, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(7, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(7, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(7, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(7, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(7, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(7, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
	SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
	SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2);
	SetTextureStageState(3, D3DTSS_TEXCOORDINDEX, 3);
	SetTextureStageState(4, D3DTSS_TEXCOORDINDEX, 4);
	SetTextureStageState(5, D3DTSS_TEXCOORDINDEX, 5);
	SetTextureStageState(6, D3DTSS_TEXCOORDINDEX, 6);
	SetTextureStageState(7, D3DTSS_TEXCOORDINDEX, 7);

	SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(4, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(4, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(5, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(5, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(5, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(6, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(6, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(6, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(7, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(7, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(7, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(5, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(5, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(6, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(6, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(7, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(7, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(3, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(4, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(5, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(6, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(7, D3DTSS_TEXTURETRANSFORMFLAGS, 0);

	SetTexture(0, NULL);
	SetTexture(1, NULL);
	SetTexture(2, NULL);
	SetTexture(3, NULL);
	SetTexture(4, NULL);
	SetTexture(5, NULL);
	SetTexture(6, NULL);
	SetTexture(7, NULL);

	SetPixelShader(NULL);
	SetFVF(D3DFVF_XYZ);

	D3DXVECTOR4 av4Null[STATEMANAGER_MAX_VCONSTANTS];
	memset(av4Null, 0, sizeof(av4Null));
	SetVertexShaderConstant(0, av4Null, STATEMANAGER_MAX_VCONSTANTS);
	SetPixelShaderConstant(0, av4Null, STATEMANAGER_MAX_PCONSTANTS);

	m_bForce = false;

#ifdef _DEBUG
	int i, j;
	for (i = 0; i < STATEMANAGER_MAX_RENDERSTATES; i++)
		m_bRenderStateSavingFlag[i] = FALSE;

	for (j = 0; j < STATEMANAGER_MAX_TRANSFORMSTATES; j++)
		m_bTransformSavingFlag[j] = FALSE;

	for (j = 0; j < STATEMANAGER_MAX_STAGES; ++j)
		for (i = 0; i < STATEMANAGER_MAX_TEXTURESTATES; ++i)
		{
			m_bTextureStageStateSavingFlag[j][i] = FALSE;
			m_bSamplerStateSavingFlag[j][i] = FALSE;
		}
#endif
}

void CStateManager::SaveMaterial()
{
	m_CopyState.m_D3DMaterial = m_CurrentState.m_D3DMaterial;
}

void CStateManager::SaveMaterial(const D3DMATERIAL9 * pMaterial)
{
	m_CopyState.m_D3DMaterial = m_CurrentState.m_D3DMaterial;
	SetMaterial(pMaterial);
}

void CStateManager::RestoreMaterial()
{
	SetMaterial(&m_CopyState.m_D3DMaterial);
}

void CStateManager::SetMaterial(const D3DMATERIAL9 * pMaterial)
{
	m_CurrentState.m_D3DMaterial = *pMaterial;
}

void CStateManager::GetMaterial(D3DMATERIAL9 * pMaterial)
{
	*pMaterial = m_CurrentState.m_D3DMaterial;
}

DWORD CStateManager::GetRenderState(D3DRENDERSTATETYPE Type)
{
	return m_CurrentState.m_RenderStates[Type];
}

void CStateManager::SaveRenderState(D3DRENDERSTATETYPE Type, DWORD dwValue)
{
#ifdef _DEBUG
	if (m_bRenderStateSavingFlag[Type])
	{
		Tracef(" CStateManager::SaveRenderState - This render state is already saved [%d, %d]\n", Type, dwValue);
		StateManager_Assert(!" This render state is already saved!");
	}
	m_bRenderStateSavingFlag[Type] = TRUE;
#endif

	m_CopyState.m_RenderStates[Type] = m_CurrentState.m_RenderStates[Type];
	SetRenderState(Type, dwValue);
}

void CStateManager::RestoreRenderState(D3DRENDERSTATETYPE Type)
{
#ifdef _DEBUG
	if (!m_bRenderStateSavingFlag[Type])
	{
		Tracef(" CStateManager::SaveRenderState - This render state was not saved [%d, %d]\n", Type);
		StateManager_Assert(!" This render state was not saved!");
	}
	m_bRenderStateSavingFlag[Type] = FALSE;
#endif

	SetRenderState(Type, m_CopyState.m_RenderStates[Type]);
}

void CStateManager::SetRenderState(D3DRENDERSTATETYPE Type, DWORD Value)
{
	if (D3DRS_TEXTUREFACTOR == Type)
	{
		const float c_fInv255 = 1.0f / 255.0f;
		const float afColor[4] =
		{
			((Value >> 16) & 0xff) * c_fInv255,
			((Value >> 8) & 0xff) * c_fInv255,
			(Value & 0xff) * c_fInv255,
			((Value >> 24) & 0xff) * c_fInv255,
		};
		SetPixelShaderConstant(0, afColor, 1);
	}

	if (D3DRS_FOGCOLOR == Type)
	{
		const float c_fInv255 = 1.0f / 255.0f;
		const float afColor[4] =
		{
			((Value >> 16) & 0xff) * c_fInv255,
			((Value >> 8) & 0xff) * c_fInv255,
			(Value & 0xff) * c_fInv255,
			((Value >> 24) & 0xff) * c_fInv255,
		};
		SetPixelShaderConstant(1, afColor, 1);
	}

	if ((D3DRS_ALPHAREF == Type || D3DRS_ALPHAFUNC == Type) && CGraphicBackendDX12::GetInstance())
	{
		const DWORD dwReference = (D3DRS_ALPHAREF == Type) ? Value : m_CurrentState.m_RenderStates[D3DRS_ALPHAREF];
		const DWORD dwFunction = (D3DRS_ALPHAFUNC == Type) ? Value : m_CurrentState.m_RenderStates[D3DRS_ALPHAFUNC];
		// The shader clip() implements GREATEREQUAL; strict compares bias the
		// reference upward so pixels equal to it are discarded as well.
		float fReference = (dwReference & 0xff) / 255.0f;
		if (D3DCMP_GREATER == dwFunction || D3DCMP_NOTEQUAL == dwFunction)
			fReference += 0.5f / 255.0f;
		const float afAlphaRef[4] = { fReference, 0.0f, 0.0f, 0.0f };
		SetPixelShaderConstant(2, afAlphaRef, 1);
	}

	if (m_CurrentState.m_RenderStates[Type] == Value)
		return;

	m_CurrentState.m_RenderStates[Type] = Value;
}

void CStateManager::GetRenderState(D3DRENDERSTATETYPE Type, DWORD * pdwValue)
{
	*pdwValue = m_CurrentState.m_RenderStates[Type];
}

void CStateManager::SaveTexture(DWORD dwStage, const void* pTexture)
{
	m_CopyState.m_Textures[dwStage] = m_CurrentState.m_Textures[dwStage];
	SetTexture(dwStage, pTexture);
}

void CStateManager::RestoreTexture(DWORD dwStage)
{
	SetTexture(dwStage, m_CopyState.m_Textures[dwStage]);
}

void CStateManager::SetTexture(DWORD dwStage, const void* pTexture)
{
	m_CurrentState.m_Textures[dwStage] = pTexture;
}

void CStateManager::GetTexture(DWORD dwStage, const void** ppTexture)
{
	*ppTexture = m_CurrentState.m_Textures[dwStage];
}

void CStateManager::SaveTextureStageState(DWORD dwStage,D3DTEXTURESTAGESTATETYPE Type, DWORD dwValue)
{
#ifdef _DEBUG
	if (m_bTextureStageStateSavingFlag[dwStage][Type])
		Tracef(" CStateManager::SaveTextureStageState - This texture stage state is already saved [%d, %d]\n", dwStage, Type);
	m_bTextureStageStateSavingFlag[dwStage][Type] = TRUE;
#endif
	m_CopyState.m_TextureStates[dwStage][Type] = m_CurrentState.m_TextureStates[dwStage][Type];
	SetTextureStageState(dwStage, Type, dwValue);
}

void CStateManager::RestoreTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type)
{
#ifdef _DEBUG
	if (!m_bTextureStageStateSavingFlag[dwStage][Type])
		Tracef(" CStateManager::RestoreTextureStageState - This texture stage state was not saved [%d, %d]\n", dwStage, Type);
	m_bTextureStageStateSavingFlag[dwStage][Type] = FALSE;
#endif
	SetTextureStageState(dwStage, Type, m_CopyState.m_TextureStates[dwStage][Type]);
}

void CStateManager::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type, DWORD dwValue)
{
	if (m_CurrentState.m_TextureStates[dwStage][Type] == dwValue)
		return;

	m_CurrentState.m_TextureStates[dwStage][Type] = dwValue;
}

void CStateManager::GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type, DWORD * pdwValue)
{
	*pdwValue = m_CurrentState.m_TextureStates[dwStage][Type];
}

void CStateManager::SaveSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD dwValue)
{
#ifdef _DEBUG
	if (m_bSamplerStateSavingFlag[dwStage][Type])
		Tracef(" CStateManager::SaveSamplerState - This sampler state is already saved [%d, %d]\n", dwStage, Type);
	m_bSamplerStateSavingFlag[dwStage][Type] = TRUE;
#endif
	m_CopyState.m_SamplerStates[dwStage][Type] = m_CurrentState.m_SamplerStates[dwStage][Type];
	SetSamplerState(dwStage, Type, dwValue);
}
void CStateManager::RestoreSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type)
{
#ifdef _DEBUG
	if (!m_bSamplerStateSavingFlag[dwStage][Type])
		Tracef(" CStateManager::RestoreSamplerState - This sampler state was not saved [%d, %d]\n", dwStage, Type);
	m_bSamplerStateSavingFlag[dwStage][Type] = FALSE;
#endif
	SetSamplerState(dwStage, Type, m_CopyState.m_SamplerStates[dwStage][Type]);
}
void CStateManager::SetSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD dwValue)
{
	if (m_CurrentState.m_SamplerStates[dwStage][Type] == dwValue)
		return;
	m_CurrentState.m_SamplerStates[dwStage][Type] = dwValue;
}
void CStateManager::GetSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD * pdwValue)
{
	*pdwValue = m_CurrentState.m_SamplerStates[dwStage][Type];
}

void CStateManager::SaveVertexShader(const void* pShader)
{
	m_CopyState.m_dwVertexShader = m_CurrentState.m_dwVertexShader;
	SetVertexShader(pShader);
}

void CStateManager::RestoreVertexShader()
{
	SetVertexShader(m_CopyState.m_dwVertexShader);
}

void CStateManager::SetVertexShader(const void* pShader)
{
	if (m_CurrentState.m_dwVertexShader == pShader)
		return;

	m_CurrentState.m_dwVertexShader = pShader;

	if (CGraphicBackendDX12::GetInstance())
	{
		std::unordered_map<const void*, UINT>::const_iterator it = m_kShaderProgramMapDX12.find(pShader);
		m_uVertexProgramDX12 = (it != m_kShaderProgramMapDX12.end()) ? it->second : 0xFFFFFFFF;
	}
}

void CStateManager::GetVertexShader(const void** ppShader)
{
	*ppShader = m_CurrentState.m_dwVertexShader;
}

void CStateManager::SaveVertexProcessing(BOOL IsON)
{
	m_CopyState.m_bVertexProcessing = m_CurrentState.m_bVertexProcessing;
	m_CurrentState.m_bVertexProcessing = IsON;
}
void CStateManager::RestoreVertexProcessing()
{
	m_CurrentState.m_bVertexProcessing = m_CopyState.m_bVertexProcessing;
}

void CStateManager::SaveVertexDeclaration(const void* pDeclaration)
{
	m_CopyState.m_dwVertexDeclaration = m_CurrentState.m_dwVertexDeclaration;
	SetVertexDeclaration(pDeclaration);
}
void CStateManager::RestoreVertexDeclaration()
{
	SetVertexDeclaration(m_CopyState.m_dwVertexDeclaration);
}
void CStateManager::SetVertexDeclaration(const void* pDeclaration)
{
	m_CurrentState.m_dwVertexDeclaration = pDeclaration;
}
void CStateManager::GetVertexDeclaration(const void** ppDeclaration)
{
	*ppDeclaration = m_CurrentState.m_dwVertexDeclaration;
}

void CStateManager::SaveFVF(DWORD dwFVF)
{
	m_CopyState.m_dwFVF = m_CurrentState.m_dwFVF;
	SetFVF(dwFVF);
}
void CStateManager::RestoreFVF()
{
	SetFVF(m_CopyState.m_dwFVF);
}
void CStateManager::SetFVF(DWORD dwFVF)
{
	m_CurrentState.m_dwFVF = dwFVF;
}
void CStateManager::GetFVF(DWORD * pdwFVF)
{
	*pdwFVF = m_CurrentState.m_dwFVF;
}

void CStateManager::SavePixelShader(const void* pShader)
{
	m_CopyState.m_dwPixelShader = m_CurrentState.m_dwPixelShader;
	SetPixelShader(pShader);
}

void CStateManager::RestorePixelShader()
{
	SetPixelShader(m_CopyState.m_dwPixelShader);
}

void CStateManager::SetPixelShader(const void* pShader)
{
	if (m_CurrentState.m_dwPixelShader == pShader)
		return;

	m_CurrentState.m_dwPixelShader = pShader;

	if (CGraphicBackendDX12::GetInstance())
	{
		std::unordered_map<const void*, UINT>::const_iterator it = m_kShaderProgramMapDX12.find(pShader);
		m_uPixelProgramDX12 = (it != m_kShaderProgramMapDX12.end()) ? it->second : 0xFFFFFFFF;
	}
}

void CStateManager::GetPixelShader(const void** ppShader)
{
	*ppShader = m_CurrentState.m_dwPixelShader;
}

void CStateManager::SaveTransform(D3DTRANSFORMSTATETYPE Type, const D3DMATRIX* pMatrix)
{
#ifdef _DEBUG
	if (m_bTransformSavingFlag[Type])
	{
		Tracef(" CStateManager::SaveTransform - This transform is already saved [%d]\n", Type);
		StateManager_Assert(!" This trasform is already saved!");
	}
	m_bTransformSavingFlag[Type] = TRUE;
#endif

	m_CopyState.m_Matrices[Type] = m_CurrentState.m_Matrices[Type];
	SetTransform(Type, (D3DXMATRIX *)pMatrix);
}

void CStateManager::RestoreTransform(D3DTRANSFORMSTATETYPE Type)
{
#ifdef _DEBUG
	if (!m_bTransformSavingFlag[Type])
	{
		Tracef(" CStateManager::RestoreTransform - This transform was not saved [%d]\n", Type);
		StateManager_Assert(!" This render state was not saved!");
	}
	m_bTransformSavingFlag[Type] = FALSE;
#endif

	SetTransform(Type, &m_CopyState.m_Matrices[Type]);
}

void CStateManager::SetTransform(D3DTRANSFORMSTATETYPE Type, const D3DMATRIX* pMatrix)
{
	m_CurrentState.m_Matrices[Type] = *pMatrix;
}

void CStateManager::GetTransform(D3DTRANSFORMSTATETYPE Type, D3DMATRIX * pMatrix)
{
	*pMatrix = m_CurrentState.m_Matrices[Type];
}

void CStateManager::SaveVertexShaderConstant(DWORD dwRegister,CONST void* pConstantData,DWORD dwConstantCount)
{
	DWORD i;

	for (i = 0; i < dwConstantCount; i++)
	{
		StateManager_Assert((dwRegister + i) < STATEMANAGER_MAX_VCONSTANTS);
		m_CopyState.m_VertexShaderConstants[dwRegister + i] = m_CurrentState.m_VertexShaderConstants[dwRegister + i];
	}

	SetVertexShaderConstant(dwRegister, pConstantData, dwConstantCount);
}

void CStateManager::RestoreVertexShaderConstant(DWORD dwRegister, DWORD dwConstantCount)
{
	SetVertexShaderConstant(dwRegister, &m_CopyState.m_VertexShaderConstants[dwRegister], dwConstantCount);
}

void CStateManager::SetVertexShaderConstant(DWORD dwRegister,CONST void* pConstantData,DWORD dwConstantCount)
{
	for (DWORD i = 0; i < dwConstantCount; i++)
	{
		StateManager_Assert((dwRegister + i) < STATEMANAGER_MAX_VCONSTANTS);
		m_CurrentState.m_VertexShaderConstants[dwRegister + i] = *(((D3DXVECTOR4*)pConstantData) + i);
	}

	if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
		pkBackend->SetVSConstants(dwRegister, (const float*)pConstantData, dwConstantCount);
}

void CStateManager::SavePixelShaderConstant(DWORD dwRegister,CONST void* pConstantData,DWORD dwConstantCount)
{
	DWORD i;

	for (i = 0; i < dwConstantCount; i++)
	{
		StateManager_Assert((dwRegister + i) < STATEMANAGER_MAX_PCONSTANTS);
		m_CopyState.m_PixelShaderConstants[dwRegister + i] = *(((D3DXVECTOR4*)pConstantData) + i);
	}

	SetPixelShaderConstant(dwRegister, pConstantData, dwConstantCount);
}

void CStateManager::RestorePixelShaderConstant(DWORD dwRegister, DWORD dwConstantCount)
{
	SetPixelShaderConstant(dwRegister, &m_CopyState.m_PixelShaderConstants[dwRegister], dwConstantCount);
}

void CStateManager::SetPixelShaderConstant(DWORD dwRegister,CONST void* pConstantData,DWORD dwConstantCount)
{
	for (DWORD i = 0; i < dwConstantCount; i++)
	{
		StateManager_Assert((dwRegister + i) < STATEMANAGER_MAX_PCONSTANTS);
		m_CurrentState.m_PixelShaderConstants[dwRegister + i] = *(((D3DXVECTOR4*)pConstantData) + i);
	}

	if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
		pkBackend->SetPSConstants(dwRegister, (const float*)pConstantData, dwConstantCount);
}

void CStateManager::SaveStreamSource(UINT StreamNumber, const void* pStreamData,UINT Stride)
{
	m_CopyState.m_StreamData[StreamNumber] = m_CurrentState.m_StreamData[StreamNumber];
	SetStreamSource(StreamNumber, pStreamData, Stride);
}

void CStateManager::RestoreStreamSource(UINT StreamNumber)
{
	SetStreamSource(StreamNumber,
					m_CopyState.m_StreamData[StreamNumber].m_lpStreamData,
					m_CopyState.m_StreamData[StreamNumber].m_Stride);
}

void CStateManager::SetStreamSource(UINT StreamNumber, const void* pStreamData, UINT Stride)
{
	CStreamData kStreamData(pStreamData, Stride);
	if (m_CurrentState.m_StreamData[StreamNumber] == kStreamData)
		return;

	m_CurrentState.m_StreamData[StreamNumber] = kStreamData;

	if (0 == StreamNumber)
	{
		m_pvStream0DX12 = pStreamData;
		m_uStream0StrideDX12 = Stride;
		m_bTransientStream0 = false;
	}
}

void CStateManager::SaveIndices(const void* pIndexData, UINT BaseVertexIndex)
{
	m_CopyState.m_IndexData = m_CurrentState.m_IndexData;
	SetIndices(pIndexData, BaseVertexIndex);
}

void CStateManager::RestoreIndices()
{
	SetIndices(m_CopyState.m_IndexData.m_lpIndexData, m_CopyState.m_IndexData.m_BaseVertexIndex);
}

void CStateManager::SetIndices(const void* pIndexData, UINT BaseVertexIndex)
{
	CIndexData kIndexData(pIndexData, BaseVertexIndex);

	if (m_CurrentState.m_IndexData == kIndexData)
		return;

	m_CurrentState.m_IndexData = kIndexData;

	m_pvIndicesDX12 = pIndexData;
}

HRESULT CStateManager::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	if (CGraphicBackendDX12::GetInstance())
	{
		UINT uVertexCount = 0;
		switch (PrimitiveType)
		{
			case D3DPT_POINTLIST:		uVertexCount = PrimitiveCount; break;
			case D3DPT_LINELIST:		uVertexCount = PrimitiveCount * 2; break;
			case D3DPT_LINESTRIP:		uVertexCount = PrimitiveCount + 1; break;
			case D3DPT_TRIANGLELIST:	uVertexCount = PrimitiveCount * 3; break;
			case D3DPT_TRIANGLESTRIP:	uVertexCount = PrimitiveCount + 2; break;
			default: break;
		}
		if (uVertexCount)
		{
			const BYTE* pbyVertices = NULL;
			UINT uStreamVertexCount = 0;
			UINT uStride = 0;
			if (m_bTransientStream0)
			{
				if (__GetStream0Data(&pbyVertices, &uStreamVertexCount, &uStride) &&
					StartVertex + uVertexCount <= uStreamVertexCount)
					__MirrorDrawDX12(PrimitiveType,
									 pbyVertices + static_cast<size_t>(StartVertex) * uStride,
									 uVertexCount, uStride, NULL, 0);
			}
			else if (__GetStream0Data(&pbyVertices, &uStreamVertexCount, &uStride))
			{
				if (!__MirrorDrawRegisteredDX12(PrimitiveType, StartVertex, uVertexCount, 0, 0, 0))
					__MirrorDrawBuffersDX12(PrimitiveType, StartVertex, uVertexCount, 0, 0, 0);
			}
			else
				__MirrorDrawBuffersDX12(PrimitiveType, StartVertex, uVertexCount, 0, 0, 0);
		}
	}

	return D3D_OK;
}

HRESULT CStateManager::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	m_CurrentState.m_StreamData[0] = NULL;
	m_bTransientStream0 = false;

	if (CGraphicBackendDX12::GetInstance())
	{
		UINT uVertexCount = 0;
		switch (PrimitiveType)
		{
			case D3DPT_POINTLIST:		uVertexCount = PrimitiveCount; break;
			case D3DPT_LINELIST:		uVertexCount = PrimitiveCount * 2; break;
			case D3DPT_LINESTRIP:		uVertexCount = PrimitiveCount + 1; break;
			case D3DPT_TRIANGLELIST:	uVertexCount = PrimitiveCount * 3; break;
			case D3DPT_TRIANGLESTRIP:	uVertexCount = PrimitiveCount + 2; break;
			default: break;
		}
		if (uVertexCount)
			__MirrorDrawDX12(PrimitiveType, pVertexStreamZeroData, uVertexCount, VertexStreamZeroStride, NULL, 0);
	}

	return D3D_OK;
}

HRESULT CStateManager::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
	if (CGraphicBackendDX12::GetInstance())
	{
		UINT uIndexCount = 0;
		switch (PrimitiveType)
		{
			case D3DPT_LINELIST:		uIndexCount = primCount * 2; break;
			case D3DPT_TRIANGLELIST:	uIndexCount = primCount * 3; break;
			case D3DPT_TRIANGLESTRIP:	uIndexCount = primCount + 2; break;
			default: break;
		}
		if (uIndexCount)
		{
			const BYTE* pbyVertices = NULL;
			UINT uStreamVertexCount = 0;
			UINT uStride = 0;
			if (m_bTransientStream0)
			{
				if (__GetStream0Data(&pbyVertices, &uStreamVertexCount, &uStride))
				{
					std::unordered_map<const void*, TIndexData>::const_iterator itIndices =
						m_pvIndicesDX12 ? m_kIndexDataMap.find(m_pvIndicesDX12) : m_kIndexDataMap.end();
					if (itIndices != m_kIndexDataMap.end() &&
						startIndex + uIndexCount <= itIndices->second.kIndices.size())
					{
						const WORD* awSource = &itIndices->second.kIndices[startIndex];
						if (minIndex)
						{
							std::vector<WORD> kRebased(awSource, awSource + uIndexCount);
							for (UINT u = 0; u != uIndexCount; ++u)
								kRebased[u] = static_cast<WORD>(kRebased[u] + minIndex);
							__MirrorDrawDX12(PrimitiveType, pbyVertices, uStreamVertexCount,
											 uStride, &kRebased[0], uIndexCount);
						}
						else
							__MirrorDrawDX12(PrimitiveType, pbyVertices, uStreamVertexCount,
											 uStride, awSource, uIndexCount);
					}
				}
			}
			else if (__GetStream0Data(&pbyVertices, &uStreamVertexCount, &uStride))
			{
				if (!__MirrorDrawRegisteredDX12(PrimitiveType, 0, 0, startIndex, uIndexCount, static_cast<INT>(minIndex)))
					__MirrorDrawBuffersDX12(PrimitiveType, 0, 0, startIndex, uIndexCount, static_cast<INT>(minIndex));
			}
			else
				__MirrorDrawBuffersDX12(PrimitiveType, 0, 0, startIndex, uIndexCount, static_cast<INT>(minIndex));
		}
	}

	return D3D_OK;
}

HRESULT CStateManager::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, CONST void * pIndexData, D3DFORMAT IndexDataFormat, CONST void * pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
	m_CurrentState.m_IndexData = NULL;
	m_CurrentState.m_StreamData[0] = NULL;
	m_bTransientStream0 = false;

	if (CGraphicBackendDX12::GetInstance() && D3DFMT_INDEX16 == IndexDataFormat)
	{
		UINT uIndexCount = 0;
		switch (PrimitiveType)
		{
			case D3DPT_LINELIST:		uIndexCount = PrimitiveCount * 2; break;
			case D3DPT_TRIANGLELIST:	uIndexCount = PrimitiveCount * 3; break;
			case D3DPT_TRIANGLESTRIP:	uIndexCount = PrimitiveCount + 2; break;
			default: break;
		}
		if (uIndexCount)
			__MirrorDrawDX12(PrimitiveType, pVertexStreamZeroData, MinVertexIndex + NumVertexIndices, VertexStreamZeroStride,
							 (const WORD*)pIndexData, uIndexCount);
	}

	return D3D_OK;
}

void CStateManager::RegisterShaderProgramDX12(const void* pkShader, const char* c_szProgramName)
{
	if (!pkShader)
		return;

	for (UINT u = 0; u < CGraphicShaderPool::GetProgramCount(); ++u)
	{
		if (0 == strcmp(CGraphicShaderPool::GetProgramInfo(u)->c_szName, c_szProgramName))
		{
			m_kShaderProgramMapDX12[pkShader] = u;
			return;
		}
	}

	TraceError("CStateManager: unknown shader program %s.", c_szProgramName);
}

void CStateManager::RegisterTextureSRVDX12(const void* pkTexture, D3D12_CPU_DESCRIPTOR_HANDLE kSRVHandle)
{
	if (pkTexture && kSRVHandle.ptr)
		m_kTextureSRVMapDX12[pkTexture] = kSRVHandle.ptr;
}

void CStateManager::UnregisterTextureSRVDX12(const void* pkTexture)
{
	if (pkTexture)
		m_kTextureSRVMapDX12.erase(pkTexture);
}

bool CStateManager::__MirrorPrepareDX12(D3DPRIMITIVETYPE ePrimitiveType, D3D_PRIMITIVE_TOPOLOGY* peTopologyOut)
{
	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (!pkBackend)
		return false;

	if (0xFFFFFFFF == m_uVertexProgramDX12 || 0xFFFFFFFF == m_uPixelProgramDX12)
		return false;

	std::unordered_map<const void*, TInputLayoutDX12>::const_iterator itLayout =
		m_kDeclLayoutMapDX12.find(m_CurrentState.m_dwVertexDeclaration);
	if (itLayout == m_kDeclLayoutMapDX12.end())
		return false;

	switch (ePrimitiveType)
	{
		case D3DPT_POINTLIST:		*peTopologyOut = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
		case D3DPT_LINELIST:		*peTopologyOut = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
		case D3DPT_LINESTRIP:		*peTopologyOut = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
		case D3DPT_TRIANGLELIST:	*peTopologyOut = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
		case D3DPT_TRIANGLESTRIP:	*peTopologyOut = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
		default:
			return false;
	}

	CGraphicPipelineKeyDX12 kPipelineKey;
	kPipelineKey.CaptureRenderStates(*this);
	pkBackend->SetPipelineStates(kPipelineKey);

	CGraphicSamplerKeyDX12 kSamplerKey;
	for (UINT u = 0; u < CGraphicBackendDX12::TEXTURE_STAGE_COUNT; ++u)
	{
		kSamplerKey.Capture(*this, u);
		pkBackend->SetSamplerKey(u, kSamplerKey);

		const void* pkTexture = m_CurrentState.m_Textures[u];
		std::unordered_map<const void*, SIZE_T>::const_iterator itTexture =
			pkTexture ? m_kTextureSRVMapDX12.find(pkTexture) : m_kTextureSRVMapDX12.end();
		if (itTexture != m_kTextureSRVMapDX12.end())
		{
			D3D12_CPU_DESCRIPTOR_HANDLE kSRVHandle;
			kSRVHandle.ptr = itTexture->second;
			pkBackend->SetTextureSRV(u, kSRVHandle);
		}
		else
			pkBackend->ClearTextureSRV(u);
	}

	if (!pkBackend->SetProgram(m_uVertexProgramDX12, m_uPixelProgramDX12))
		return false;

	pkBackend->SetInputLayout(itLayout->second.uLayoutID, itLayout->second.akElements,
							  itLayout->second.uElementCount);

	return true;
}

bool CStateManager::__MirrorDrawDX12(D3DPRIMITIVETYPE ePrimitiveType,
									 const void* pvVertices, UINT uVertexCount, UINT uStrideBytes,
									 const WORD* awIndices, UINT uIndexCount)
{
	D3D_PRIMITIVE_TOPOLOGY eTopology;
	if (!__MirrorPrepareDX12(ePrimitiveType, &eTopology))
		return false;

	return CGraphicBackendDX12::GetInstance()->DrawTransient(eTopology, pvVertices, uVertexCount,
															  uStrideBytes, awIndices, uIndexCount);
}

bool CStateManager::__MirrorDrawBuffersDX12(D3DPRIMITIVETYPE ePrimitiveType,
											UINT uStartVertex, UINT uVertexCount,
											UINT uStartIndex, UINT uIndexCount, INT nBaseVertex)
{
	std::unordered_map<const void*, TBufferDX12>::const_iterator itVertexBuffer =
		m_pvStream0DX12 ? m_kBufferMapDX12.find(m_pvStream0DX12) : m_kBufferMapDX12.end();
	if (itVertexBuffer == m_kBufferMapDX12.end())
		return false;

	D3D_PRIMITIVE_TOPOLOGY eTopology;
	if (!__MirrorPrepareDX12(ePrimitiveType, &eTopology))
		return false;

	D3D12_VERTEX_BUFFER_VIEW kVertexView;
	kVertexView.BufferLocation = itVertexBuffer->second.pkResource->GetGPUVirtualAddress();
	kVertexView.SizeInBytes = static_cast<UINT>(itVertexBuffer->second.pkResource->GetDesc().Width);
	kVertexView.StrideInBytes = m_uStream0StrideDX12;

	if (uIndexCount)
	{
		std::unordered_map<const void*, TBufferDX12>::const_iterator itIndexBuffer =
			m_pvIndicesDX12 ? m_kBufferMapDX12.find(m_pvIndicesDX12) : m_kBufferMapDX12.end();
		if (itIndexBuffer == m_kBufferMapDX12.end() || DXGI_FORMAT_UNKNOWN == itIndexBuffer->second.eIndexFormat)
			return false;

		D3D12_INDEX_BUFFER_VIEW kIndexView;
		kIndexView.BufferLocation = itIndexBuffer->second.pkResource->GetGPUVirtualAddress();
		kIndexView.SizeInBytes = static_cast<UINT>(itIndexBuffer->second.pkResource->GetDesc().Width);
		kIndexView.Format = itIndexBuffer->second.eIndexFormat;

		return CGraphicBackendDX12::GetInstance()->DrawBuffers(eTopology, kVertexView, 0, 0,
																&kIndexView, uStartIndex, uIndexCount, nBaseVertex);
	}

	return CGraphicBackendDX12::GetInstance()->DrawBuffers(eTopology, kVertexView, uStartVertex, uVertexCount,
															NULL, 0, 0, 0);
}

bool CStateManager::__MirrorDrawRegisteredDX12(D3DPRIMITIVETYPE ePrimitiveType,
											   UINT uStartVertex, UINT uVertexCount,
											   UINT uStartIndex, UINT uIndexCount, INT nBaseVertex)
{
	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (!pkBackend || !m_pvStream0DX12)
		return false;

	std::unordered_map<const void*, TVertexData>::iterator itVertexData = m_kVertexDataMap.find(m_pvStream0DX12);
	if (itVertexData == m_kVertexDataMap.end() ||
		!itVertexData->second.uStride || itVertexData->second.kBytes.empty())
		return false;

	TVertexData& rkVertexData = itVertexData->second;
	// The stride bound with the stream wins over the registered one: several
	// callers create buffers with an approximate FVF but draw with the packed
	// stride of their real vertex struct.
	const UINT uStride = m_uStream0StrideDX12 ? m_uStream0StrideDX12 : rkVertexData.uStride;
	const UINT uTotalVertexCount = static_cast<UINT>(rkVertexData.kBytes.size() / uStride);
	if (!uIndexCount && uStartVertex + uVertexCount > uTotalVertexCount)
		return false;

	const UINT64 uFrameOrdinal = pkBackend->GetFrameOrdinal();
	if (rkVertexData.uFrameStamp != uFrameOrdinal)
	{
		if (!pkBackend->UploadVertices(&rkVertexData.kBytes[0], uStride, uTotalVertexCount,
									   &rkVertexData.kRingView))
			return false;
		rkVertexData.uFrameStamp = uFrameOrdinal;
	}
	rkVertexData.kRingView.StrideInBytes = uStride;

	D3D_PRIMITIVE_TOPOLOGY eTopology;

	if (uIndexCount)
	{
		std::unordered_map<const void*, TIndexData>::iterator itIndexData =
			m_pvIndicesDX12 ? m_kIndexDataMap.find(m_pvIndicesDX12) : m_kIndexDataMap.end();
		if (itIndexData == m_kIndexDataMap.end() ||
			uStartIndex + uIndexCount > itIndexData->second.kIndices.size())
			return false;

		TIndexData& rkIndexData = itIndexData->second;
		if (rkIndexData.uFrameStamp != uFrameOrdinal)
		{
			if (!pkBackend->UploadIndices(&rkIndexData.kIndices[0],
										  static_cast<UINT>(rkIndexData.kIndices.size()),
										  &rkIndexData.kRingView))
				return false;
			rkIndexData.uFrameStamp = uFrameOrdinal;
		}

		if (!__MirrorPrepareDX12(ePrimitiveType, &eTopology))
			return false;

		return pkBackend->DrawBuffers(eTopology, rkVertexData.kRingView, 0, 0,
									  &rkIndexData.kRingView, uStartIndex, uIndexCount, nBaseVertex);
	}

	if (!__MirrorPrepareDX12(ePrimitiveType, &eTopology))
		return false;

	return pkBackend->DrawBuffers(eTopology, rkVertexData.kRingView, uStartVertex, uVertexCount,
								  NULL, 0, 0, 0);
}

void CStateManager::SetTransientStream(const void* pvVertices, UINT uVertexCount, UINT uStride)
{
	if (!CGraphicBackendDX12::GetInstance() || !pvVertices || !uVertexCount || !uStride)
		return;

	const BYTE* pbySource = static_cast<const BYTE*>(pvVertices);
	m_kTransientVertexData.assign(pbySource, pbySource + static_cast<size_t>(uVertexCount) * uStride);
	m_uTransientVertexCount = uVertexCount;
	m_uTransientVertexStride = uStride;
	m_bTransientStream0 = true;
}

void CStateManager::RegisterIndexData(const void* pkIndexBuffer, const WORD* awIndices, UINT uIndexCount)
{
	if (!pkIndexBuffer || !awIndices || !uIndexCount)
		return;

	TIndexData& rkEntry = m_kIndexDataMap[pkIndexBuffer];
	rkEntry.kIndices.assign(awIndices, awIndices + uIndexCount);
	rkEntry.uFrameStamp = 0;
}

void CStateManager::UnregisterIndexData(const void* pkIndexBuffer)
{
	if (pkIndexBuffer)
		m_kIndexDataMap.erase(pkIndexBuffer);
}

void CStateManager::RegisterVertexData(const void* pkVertexBuffer, const void* pvVertices, UINT uByteCount, UINT uStride)
{
	if (!pkVertexBuffer || !pvVertices || !uByteCount || !uStride)
		return;

	const BYTE* pbySource = static_cast<const BYTE*>(pvVertices);
	TVertexData& rkEntry = m_kVertexDataMap[pkVertexBuffer];
	rkEntry.kBytes.assign(pbySource, pbySource + uByteCount);
	rkEntry.uStride = uStride;
	rkEntry.uFrameStamp = 0;
}

void CStateManager::UnregisterVertexData(const void* pkVertexBuffer)
{
	if (pkVertexBuffer)
		m_kVertexDataMap.erase(pkVertexBuffer);
}

bool CStateManager::__GetStream0Data(const BYTE** ppbyVertices, UINT* puVertexCount, UINT* puStride) const
{
	if (m_bTransientStream0)
	{
		if (m_kTransientVertexData.empty())
			return false;
		*ppbyVertices = &m_kTransientVertexData[0];
		*puVertexCount = m_uTransientVertexCount;
		*puStride = m_uTransientVertexStride;
		return true;
	}

	if (m_pvStream0DX12)
	{
		std::unordered_map<const void*, TVertexData>::const_iterator it = m_kVertexDataMap.find(m_pvStream0DX12);
		if (it != m_kVertexDataMap.end() && it->second.uStride && !it->second.kBytes.empty())
		{
			*ppbyVertices = &it->second.kBytes[0];
			*puVertexCount = static_cast<UINT>(it->second.kBytes.size() / it->second.uStride);
			*puStride = it->second.uStride;
			return true;
		}
	}

	return false;
}

void CStateManager::RegisterBufferDX12(const void* pkBuffer, ID3D12Resource* pkResource, DXGI_FORMAT eIndexFormat)
{
	if (!pkBuffer || !pkResource)
		return;

	TBufferDX12 kEntry;
	kEntry.pkResource = pkResource;
	kEntry.eIndexFormat = eIndexFormat;
	m_kBufferMapDX12[pkBuffer] = kEntry;
}

void CStateManager::UnregisterBufferDX12(const void* pkBuffer)
{
	if (pkBuffer)
		m_kBufferMapDX12.erase(pkBuffer);
}
