#include "stdafx.h"
#include "SoundManager3D.h"
#include "../eterBase/Timer.h"

CSoundInstance3D::CSoundInstance3D() : m_sample(NULL), m_pSoundData(NULL)
{
}

CSoundInstance3D::~CSoundInstance3D()
{
	Destroy();
}

void CSoundInstance3D::Destroy()
{
	SAFE_RELEASE(m_pSoundData);

	if (m_sample)
	{
		AIL_release_sample_handle(m_sample);
		m_sample = NULL;
	}
}

bool CSoundInstance3D::Initialize()
{
	if (m_sample)
		return true;

	m_sample = AIL_allocate_sample_handle(CSoundBase::ms_DIGDriver);

	if (m_sample)
		AIL_set_sample_is_3D(m_sample, 1);

	return m_sample ? true : false;
}

bool CSoundInstance3D::SetSound(CSoundData* pSoundData)
{
	assert(m_sample != NULL && pSoundData != NULL);

	// It must be loaded when the reference count becomes 1 to return the correct size.
	// Therefore, you must call Get before proceeding.
	// Also, m_pSoundData is the same as pSoundData and is a reference to m_pSoundData.
	// If the counter is 1, unnecessary loading occurs, so reference is required in advance.
	// A counter must be put up.
	LPVOID lpData = pSoundData->Get();
	
	if (m_pSoundData != NULL)
	{
		m_pSoundData->Release();
		m_pSoundData = NULL;
	}
	
	if (AIL_set_sample_file(m_sample, lpData, -1) == 0)
	{
		TraceError("%s: %s", AIL_last_error(), pSoundData->GetFileName());
		pSoundData->Release();
		return false;
	}

	m_pSoundData = pSoundData;

	AIL_set_sample_3D_position(m_sample, 0.0F, 0.0F, 0.0F);
	return true;
}

bool CSoundInstance3D::IsDone() const
{
	return AIL_sample_status(m_sample) == SMP_DONE;
}

void CSoundInstance3D::Play(int iLoopCount, DWORD dwPlayCycleTimeLimit) const
{
	if (!m_pSoundData)
		return;

	DWORD dwCurTime = ELTimer_GetMSec();

	if (dwCurTime - m_pSoundData->GetPlayTime() < dwPlayCycleTimeLimit)
		return;

	m_pSoundData->SetPlayTime(dwCurTime);

	AIL_set_sample_loop_count(m_sample, iLoopCount);
	AIL_start_sample(m_sample);
}

void CSoundInstance3D::Pause() const
{
	AIL_stop_sample(m_sample);
}

void CSoundInstance3D::Resume() const
{
	AIL_resume_sample(m_sample);
}

void CSoundInstance3D::Stop()
{
	AIL_end_sample(m_sample);
//	m_sample = NULL;
// NOTE: m_sample must be alive to check IsDone - [levites]
}

void CSoundInstance3D::GetVolume(float& rfVolume) const
{
	F32 volume = 0.0f;
	F32 pan = 0.0f;
	AIL_sample_volume_pan(m_sample, &volume, &pan);
	rfVolume = volume;
}

void CSoundInstance3D::SetVolume(float volume) const
{
	volume = max(0.0f, min(1.0f, volume));
	AIL_set_sample_volume_pan(m_sample, volume, 0.5f);
}

void CSoundInstance3D::SetPosition(float x, float y, float z) const
{
	AIL_set_sample_3D_position(m_sample, x, y, -z);
}

void CSoundInstance3D::SetOrientation(float x_face, float y_face, float z_face, 
									  float x_normal, float y_normal, float z_normal) const
{
	assert(!"CSoundInstance3D::SetOrientation - Deprecated functionunctionunctionunction");
//	AIL_set_3D_orientation(m_sample, 
//						   x_face, y_face, z_face,
//						   x_normal, y_normal, z_normal);
}

void CSoundInstance3D::SetVelocity(float fDistanceX, float fDistanceY, float fDistanceZ, float fNagnitude) const
{
	AIL_set_sample_3D_velocity(m_sample, fDistanceX, fDistanceY, fDistanceZ, fNagnitude);
}

void CSoundInstance3D::UpdatePosition(float fElapsedTime)
{
	AIL_update_sample_3D_position(m_sample, fElapsedTime);
}