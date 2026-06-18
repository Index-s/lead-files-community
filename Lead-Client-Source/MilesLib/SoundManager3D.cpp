#include "Stdafx.h"
#include "SoundManager3D.h"

CSoundManager3D::CSoundManager3D()
{
	m_bInit = false;
}

CSoundManager3D::~CSoundManager3D()
{
}

bool CSoundManager3D::Initialize()
{
	CSoundBase::Initialize();

	if (m_bInit)
		return true;

	// Miles 9.3: 3D is built into the digital driver; the legacy provider/
	// listener-object model is gone. Reuse the shared digital driver (opened
	// here if no other manager has done so yet).
	if (!ms_DIGDriver)
		ms_DIGDriver = AIL_open_digital_driver(44100, 16, 2, 0);

	if (!ms_DIGDriver)
	{
		CSoundBase::Destroy();
		return false;
	}

	for (int i = 0; i < INSTANCE_MAX_COUNT; ++i)
	{
		m_Instances[i].Initialize();
		m_bLockingFlag[i] = false;
	}

	AIL_set_listener_3D_position(ms_DIGDriver, 0.0f, 0.0f, 0.0f);

	m_bInit = true;
	return true;
}


void CSoundManager3D::Destroy()
{
	if (!m_bInit)
		return;

	for (int i = 0; i < INSTANCE_MAX_COUNT; ++i)
		m_Instances[i].Destroy();

	CSoundBase::Destroy();
	m_bInit = false;
}

void CSoundManager3D::SetListenerDirection(float fxDir, float fyDir, float fzDir, float fxUp, float fyUp, float fzUp)
{
	if (NULL == ms_DIGDriver)
		return;
	AIL_set_listener_3D_orientation(ms_DIGDriver, fxDir, fyDir, -fzDir, fxUp, fyUp, -fzUp);
}

void CSoundManager3D::SetListenerPosition(float fX, float fY, float fZ)
{
	if (NULL == ms_DIGDriver)
		return;
	AIL_set_listener_3D_position(ms_DIGDriver, fX, fY, -fZ);
}

void CSoundManager3D::SetListenerVelocity(float fDistanceX, float fDistanceY, float fDistanceZ, float fNagnitude)
{
	if (NULL == ms_DIGDriver)
		return;
	AIL_set_listener_3D_velocity(ms_DIGDriver, fDistanceX, fDistanceY, -fDistanceZ, fNagnitude);
}

int CSoundManager3D::SetInstance(const char * c_pszFileName)
{
	DWORD dwFileCRC = GetFileCRC(c_pszFileName);
	TSoundDataMap::iterator itor = ms_dataMap.find(dwFileCRC);

	CSoundData * pkSoundData;

	if (itor == ms_dataMap.end())
		pkSoundData = AddFile(dwFileCRC, c_pszFileName); // CSoundBase::AddFile
	else
		pkSoundData = itor->second;

	assert(pkSoundData != NULL);

	static DWORD k = 0;

	DWORD start = k++;
	DWORD end = start + INSTANCE_MAX_COUNT;

	while (start < end)
	{
		CSoundInstance3D * pkInst = &m_Instances[start % INSTANCE_MAX_COUNT];

		if (pkInst->IsDone())
		{
			if (!pkInst->SetSound(pkSoundData))
			{
				TraceError("CSoundManager3D::GetInstance (filename: %s)", c_pszFileName);
				// NOTE: If there is no sound, Failed to set. return NULL. - [levites]
				return -1;
			}

			return (start % INSTANCE_MAX_COUNT);
		}

		++start;

		// It's unlikely that it will exceed the DWORD limit... but... just in case... - [levites]
		if (start > 50000)
		{
			start = 0;
			return -1;
		}
	}

	return -1;
}

ISoundInstance * CSoundManager3D::GetInstance(DWORD dwIndex)
{
	if (dwIndex >= INSTANCE_MAX_COUNT)
	{
		assert(dwIndex < INSTANCE_MAX_COUNT);
		return NULL;
	}
	return &m_Instances[dwIndex];
}

__forceinline bool CSoundManager3D::IsValidInstanceIndex(int iIndex)
{
	if (iIndex >= 0 && iIndex < INSTANCE_MAX_COUNT)
		return true;

	return false;
}

void CSoundManager3D::Lock(int iIndex)
{
	if (!IsValidInstanceIndex(iIndex))
		return;

	m_bLockingFlag[iIndex] = true;
}

void CSoundManager3D::Unlock(int iIndex)
{
	if (!IsValidInstanceIndex(iIndex))
		return;

	m_bLockingFlag[iIndex] = false;
}
