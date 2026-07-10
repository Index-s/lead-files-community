#include "StdAfx.h"
#include "SnowEnvironment.h"

#include "../EterLib/StateManager.h"
#include "../EterLib/Camera.h"
#include "../EterLib/ResourceManager.h"
#include "SnowParticle.h"

void CSnowEnvironment::Enable()
{
	if (!m_bSnowEnable)
	{
		Create();
	}

	m_bSnowEnable = TRUE;
}

void CSnowEnvironment::Disable()
{
	m_bSnowEnable = FALSE;
}

void CSnowEnvironment::Update(const D3DXVECTOR3 & c_rv3Pos)
{
	if (!m_bSnowEnable)
	{
		if (m_kVct_pkParticleSnow.empty())
			return;
	}

	m_v3Center=c_rv3Pos;
}

void CSnowEnvironment::Deform()
{
	if (!m_bSnowEnable)
	{
		if (m_kVct_pkParticleSnow.empty())
			return;
	}

	const D3DXVECTOR3 & c_rv3Pos=m_v3Center;

	static long s_lLastTime = CTimer::Instance().GetCurrentMillisecond();
	long lcurTime = CTimer::Instance().GetCurrentMillisecond();
	float fElapsedTime = float(lcurTime - s_lLastTime) / 1000.0f;
	s_lLastTime = lcurTime;

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	const D3DXVECTOR3 & c_rv3View = pCamera->GetView();

	D3DXVECTOR3 v3ChangedPos = c_rv3View * 3500.0f + c_rv3Pos;
	v3ChangedPos.z = c_rv3Pos.z;

	std::vector<CSnowParticle*>::iterator itor = m_kVct_pkParticleSnow.begin();
	for (; itor != m_kVct_pkParticleSnow.end();)
	{
		CSnowParticle * pSnow = *itor;
		pSnow->Update(fElapsedTime, v3ChangedPos);

		if (!pSnow->IsActivate())
		{
			CSnowParticle::Delete(pSnow);

			itor = m_kVct_pkParticleSnow.erase(itor);
		}
		else
		{
			++itor;
		}
	}

	if (m_bSnowEnable)
	{
		for (int p = 0; p < min(10, m_dwParticleMaxNum - m_kVct_pkParticleSnow.size()); ++p)
		{
			CSnowParticle * pSnowParticle = CSnowParticle::New();
			pSnowParticle->Init(v3ChangedPos);
			m_kVct_pkParticleSnow.push_back(pSnowParticle);
		}
	}
}

void CSnowEnvironment::Render()
{
	if (!m_bSnowEnable)
	{
		if (m_kVct_pkParticleSnow.empty())
			return;
	}

	if (m_kVertexData.empty() || m_kIndexData.empty())
		return;

	DWORD dwParticleCount = static_cast<DWORD>(min(static_cast<size_t>(m_dwParticleMaxNum), m_kVct_pkParticleSnow.size()));

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	const D3DXVECTOR3 & c_rv3Up = pCamera->GetUp();
	const D3DXVECTOR3 & c_rv3Cross = pCamera->GetCross();

	if (dwParticleCount > 0)
	{
		SParticleVertex * pv3Verticies = reinterpret_cast<SParticleVertex*>(&m_kVertexData[0]);
		DWORD i = 0;
		std::vector<CSnowParticle*>::iterator itor = m_kVct_pkParticleSnow.begin();
		for (; i < dwParticleCount && itor != m_kVct_pkParticleSnow.end(); ++i, ++itor)
		{
			CSnowParticle * pSnow = *itor;
			pSnow->SetCameraVertex(c_rv3Up, c_rv3Cross);
			pSnow->GetVerticies(pv3Verticies[i*4+0],
								pv3Verticies[i*4+1],
								pv3Verticies[i*4+2],
								pv3Verticies[i*4+3]);
		}
		STATEMANAGER.RegisterVertexData(&m_kVertexData[0], &m_kVertexData[0],
										sizeof(SParticleVertex)*dwParticleCount*4, sizeof(SParticleVertex));
	}

	STATEMANAGER.SaveRenderState(D3DRS_ZWRITEENABLE, FALSE);
	STATEMANAGER.SaveRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	STATEMANAGER.SaveRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	STATEMANAGER.SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
	STATEMANAGER.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	STATEMANAGER.SetTexture(1, NULL);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	STATEMANAGER.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	m_pImageInstance->GetGraphicImagePointer()->GetTextureReference().SetTextureStage(0);
	STATEMANAGER.SetIndices(&m_kIndexData[0], 0);
	STATEMANAGER.SetStreamSource(0, &m_kVertexData[0], sizeof(SParticleVertex));
	if (CGraphicBase::BeginPTTextureShader())
	{
		STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, dwParticleCount*4, 0, dwParticleCount*2);
		CGraphicBase::EndPDTShader();
	}
	else
	{
		STATEMANAGER.SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
		STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, dwParticleCount*4, 0, dwParticleCount*2);
	}
	STATEMANAGER.RestoreRenderState(D3DRS_ALPHABLENDENABLE);
	STATEMANAGER.RestoreRenderState(D3DRS_ZWRITEENABLE);
	STATEMANAGER.RestoreRenderState(D3DRS_CULLMODE);
}

bool CSnowEnvironment::__CreateGeometry()
{
	m_kVertexData.resize(sizeof(SParticleVertex)*m_dwParticleMaxNum*4);
	m_kIndexData.resize(m_dwParticleMaxNum*6);

	const WORD c_awFillRectIndices[6] = { 0, 2, 1, 2, 3, 1, };
	for (DWORD i = 0; i < m_dwParticleMaxNum; ++i)
	{
		for (int j = 0; j < 6; ++j)
		{
			m_kIndexData[i*6 + j] = static_cast<WORD>(i*4 + c_awFillRectIndices[j]);
		}
	}

	STATEMANAGER.RegisterIndexData(&m_kIndexData[0], &m_kIndexData[0], m_dwParticleMaxNum*6);
	return true;
}

bool CSnowEnvironment::Create()
{
	Destroy();

	if (!__CreateGeometry())
		return false;

	CGraphicImage * pImage = (CGraphicImage *)CResourceManager::Instance().GetResourcePointer("d:/ymir work/special/snow.dds");
	m_pImageInstance = CGraphicImageInstance::New();
	m_pImageInstance->SetImagePointer(pImage);

	return true;
}

void CSnowEnvironment::Destroy()
{
	if (CStateManager::InstancePtr())
	{
		if (!m_kVertexData.empty())
			STATEMANAGER.UnregisterVertexData(&m_kVertexData[0]);
		if (!m_kIndexData.empty())
			STATEMANAGER.UnregisterIndexData(&m_kIndexData[0]);
	}

	m_kVertexData.clear();
	m_kIndexData.clear();

	stl_wipe(m_kVct_pkParticleSnow);
	CSnowParticle::DestroyPool();

	if (m_pImageInstance)
	{
		CGraphicImageInstance::Delete(m_pImageInstance);
		m_pImageInstance = NULL;
	}

	__Initialize();
}

void CSnowEnvironment::__Initialize()
{
	m_bSnowEnable = FALSE;
	m_pImageInstance = NULL;

	m_kVct_pkParticleSnow.reserve(m_dwParticleMaxNum);
}

CSnowEnvironment::CSnowEnvironment()
{
	m_dwParticleMaxNum = 3000;

	__Initialize();
}
CSnowEnvironment::~CSnowEnvironment()
{
	Destroy();
}
