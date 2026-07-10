#include "StdAfx.h"
#include "../eterLib/StateManager.h"
#include "../eterlib/Camera.h"
#include "../eterlib/GrpBackendDX12.h"

#include "MapOutdoor.h"

static int recreate = false;

void CMapOutdoor::SetShadowTextureSize(WORD size)
{
	if (m_wShadowMapSize != size)
	{
		recreate = true;
		Tracenf("ShadowTextureSize changed %d -> %d", m_wShadowMapSize, size);
	}

	m_wShadowMapSize = size;
}

void CMapOutdoor::CreateCharacterShadowTexture()
{
	recreate = false;
	ReleaseCharacterShadowTexture();

	if (IsLowTextureMemory())
		SetShadowTextureSize(128);

	m_ShadowMapViewport.X = 1;
	m_ShadowMapViewport.Y = 1;
	m_ShadowMapViewport.Width = m_wShadowMapSize - 2;
	m_ShadowMapViewport.Height = m_wShadowMapSize - 2;
	m_ShadowMapViewport.MinZ = 0.0f;
	m_ShadowMapViewport.MaxZ = 1.0f;

	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (!pkBackend)
		return;

	LPDIRECT3DTEXTURE9 pkKey = (LPDIRECT3DTEXTURE9)&m_lpCharacterShadowMapTexture;
	if (!pkBackend->RegisterRenderTarget(pkKey, m_wShadowMapSize, m_wShadowMapSize))
	{
		TraceError("CMapOutdoor::CreateCharacterShadowTexture - RegisterRenderTarget failed size=%u", static_cast<UINT>(m_wShadowMapSize));
		return;
	}

	if (CStateManager::InstancePtr())
		STATEMANAGER.RegisterTextureSRVDX12(pkKey, pkBackend->GetRenderTargetSRV(pkKey));

	m_lpCharacterShadowMapTexture = pkKey;
}

void CMapOutdoor::ReleaseCharacterShadowTexture()
{
	if (m_lpCharacterShadowMapTexture)
	{
		if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
			pkBackend->UnregisterRenderTarget(m_lpCharacterShadowMapTexture);
		if (CStateManager::InstancePtr())
			STATEMANAGER.UnregisterTextureSRVDX12(m_lpCharacterShadowMapTexture);
	}

	m_lpCharacterShadowMapTexture = NULL;
}

DWORD dwLightEnable = FALSE;

bool CMapOutdoor::BeginRenderCharacterShadowToTexture()
{
	CCamera* pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();

	if (!pCurrentCamera)
		return false;

	if (recreate)
		CreateCharacterShadowTexture();

	D3DXMATRIX matLightView, matLightProj;

	const D3DXVECTOR3 v3Target = pCurrentCamera->GetTarget();

	const D3DXVECTOR3 v3Eye(
		v3Target.x - 1.732f * 1250.0f,
		v3Target.y - 1250.0f,
		v3Target.z + 2.0f * 1.732f * 1250.0f
	);

	const D3DXVECTOR3 v3Up(0.0f, 0.0f, 1.0f);

	D3DXMatrixLookAtRH(
		&matLightView,
		&v3Eye,
		&v3Target,
		&v3Up
	);

	D3DXMatrixOrthoRH(&matLightProj, 2550.0f, 2550.0f, 1.0f, 15000.0f);

	STATEMANAGER.SaveTransform(D3DTS_VIEW, &matLightView);
	STATEMANAGER.SaveTransform(D3DTS_PROJECTION, &matLightProj);

	dwLightEnable = STATEMANAGER.GetRenderState(D3DRS_LIGHTING);
	STATEMANAGER.SetRenderState(D3DRS_LIGHTING, FALSE);

	STATEMANAGER.SaveRenderState(D3DRS_TEXTUREFACTOR, 0xFF808080);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	bool bSuccess = true;

	if (FAILED(STATEMANAGER.SetRenderTarget(0, m_lpCharacterShadowMapTexture)))
	{
		TraceError("CMapOutdoor::BeginRenderCharacterShadowToTexture : Unable to Set Shadow Map Render Target\n");
		bSuccess = false;
	}

	if (FAILED(STATEMANAGER.Clear(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(0xFF, 0xFF, 0xFF), 1.0f, 0)))
	{
		TraceError("CMapOutdoor::BeginRenderCharacterShadowToTexture : Unable to Clear Render Target");
		bSuccess = false;
	}

	if (FAILED(STATEMANAGER.GetViewport(&m_BackupViewport)))
	{
		TraceError("CMapOutdoor::BeginRenderCharacterShadowToTexture : Unable to Save Window Viewport\n");
		bSuccess = false;
	}

	if (FAILED(STATEMANAGER.SetViewport(&m_ShadowMapViewport)))
	{
		TraceError("CMapOutdoor::BeginRenderCharacterShadowToTexture : Unable to Set Shadow Map viewport\n");
		bSuccess = false;
	}

	return bSuccess;
}

void CMapOutdoor::EndRenderCharacterShadowToTexture()
{
	STATEMANAGER.SetViewport(&m_BackupViewport);

	STATEMANAGER.SetDepthStencilSurface(NULL);
	STATEMANAGER.SetRenderTarget(0, NULL);

	STATEMANAGER.RestoreTransform(D3DTS_VIEW);
	STATEMANAGER.RestoreTransform(D3DTS_PROJECTION);

	// Restore Device Context
	STATEMANAGER.SetRenderState(D3DRS_LIGHTING, dwLightEnable);
	STATEMANAGER.RestoreRenderState(D3DRS_TEXTUREFACTOR);
}
