#include "stdafx.h"
#include "SoundManager2D.h"

CSoundInstance2D::CSoundInstance2D() : m_sample(NULL), m_pSoundData(NULL)
{
}

CSoundInstance2D::~CSoundInstance2D()
{
	Destroy();
}

void CSoundInstance2D::Destroy()
{
	SAFE_RELEASE(m_pSoundData);

	if (m_sample)
	{
		AIL_release_sample_handle(m_sample);
		m_sample = NULL;
	}
}

bool CSoundInstance2D::Initialize()
{
	if (m_sample)
		return true;
	
	m_sample = AIL_allocate_sample_handle(ms_DIGDriver);
	return m_sample ? true : false;
}

bool CSoundInstance2D::SetSound(CSoundData * pSoundData)
{
	assert(m_sample != NULL && pSoundData != NULL);

	// It must be loaded when the reference count becomes 1 to return the correct size.
	// Therefore, you must call Get before proceeding.
	// Also, m_pSoundData is the same as pSoundData and is a reference to m_pSoundData.
	// If the counter is 1, unnecessary loading occurs, so reference is required in advance.
	// A counter must be put up.
	LPVOID lpData = pSoundData->Get();

    if (AIL_set_sample_file(m_sample, lpData, -1) == 0)
	{
		if (m_pSoundData != NULL)
		{
			m_pSoundData->Release();
			m_pSoundData = NULL;
		}

		pSoundData->Release();
		TraceError("%s: %s", AIL_last_error(), pSoundData->GetFileName());
		return false;
	}

	if (m_pSoundData != NULL)
	{
		m_pSoundData->Release();
		m_pSoundData = NULL;
	}
	
	m_pSoundData = pSoundData;
	return true;
}

bool CSoundInstance2D::IsDone() const
{
	return AIL_sample_status(m_sample) == SMP_DONE;
}

void CSoundInstance2D::Play(int iLoopCount, DWORD dwPlayCycleTimeLimit) const
{
    AIL_set_sample_loop_count(m_sample, iLoopCount);
	AIL_start_sample(m_sample);
}

void CSoundInstance2D::Pause() const
{
	AIL_stop_sample(m_sample);
}

void CSoundInstance2D::Resume() const
{
	AIL_resume_sample(m_sample);
}

void CSoundInstance2D::Stop()
{
	AIL_end_sample(m_sample);
	m_sample = NULL;
}

void CSoundInstance2D::GetVolume(float& rfVolume) const
{
	AIL_sample_volume_pan(m_sample, &rfVolume, NULL);
}

void CSoundInstance2D::SetVolume(float volume) const
{
	volume = max(0.0f, min(1.0f, volume));
	AIL_set_sample_volume_pan(m_sample, volume, 0.5f);
}

void CSoundInstance2D::SetPosition(float x, float y, float z) const
{
	assert(!"must not call this method");
}

void CSoundInstance2D::SetOrientation(float x_face, float y_face, float z_face, 
									  float x_normal, float y_normal, float z_normal) const
{
	assert(!"must not call this method");
}

void CSoundInstance2D::SetVelocity(float fx, float fy, float fz, float fMagnitude) const
{
	assert(!"must not call this method");
}
