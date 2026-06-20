#include "stdafx.h"
#include "SoundManager3D.h"
#include "../eterBase/Timer.h"

#include <math.h>

CSoundInstance3D::CSoundInstance3D()
	: m_sample(NULL), m_pSoundData(NULL),
	  m_fBaseVolume(1.0f), m_fDistanceScale(1.0f), m_fPan(0.5f)
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

	// Plain 2D sample handle on the shared digital driver -- deliberately NOT
	// AIL_set_sample_is_3D(). The x64 Miles 9.3 digital-driver 3D positioner does
	// not mix manually-allocated 3D samples to the output (samples load and report
	// PLAYING, but are inaudible), whereas ordinary 2D samples and streamed music on
	// the SAME driver play fine. We therefore spatialise by hand in SetPosition().
	m_sample = AIL_allocate_sample_handle(CSoundBase::ms_DIGDriver);
	return m_sample ? true : false;
}

bool CSoundInstance3D::SetSound(CSoundData* pSoundData)
{
	assert(m_sample != NULL && pSoundData != NULL);

	// Get() before Release() so a shared CSoundData whose refcount would otherwise
	// drop to 1 is not reloaded unnecessarily.
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

	// Fresh waveform bound: play centred at full volume until SetPosition()/
	// SetVolume() reconfigure the manual spatialisation for this play.
	m_fDistanceScale = 1.0f;
	m_fPan = 0.5f;
	__ApplyVolumePan();
	return true;
}

void CSoundInstance3D::__ApplyVolumePan() const
{
	if (!m_sample)
		return;

	float fVolume = m_fBaseVolume * m_fDistanceScale;
	fVolume = max(0.0f, min(1.0f, fVolume));
	AIL_set_sample_volume_pan(m_sample, fVolume, m_fPan);
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
	rfVolume = m_fBaseVolume;
}

void CSoundInstance3D::SetVolume(float volume) const
{
	m_fBaseVolume = max(0.0f, min(1.0f, volume));
	__ApplyVolumePan();
}

void CSoundInstance3D::SetPosition(float x, float y, float z) const
{
	// Manual spatialisation. x/y/z are listener-relative world units already divided
	// by CSoundManager::m_fSoundScale (200); PlayCharacterSound3D() discards anything
	// past ~25 relative units (5000 world). Keep near sounds at full volume and fall
	// off only to a floor so every in-range sound stays clearly audible. Pan follows
	// the left/right (X) offset.
	float fDist = sqrtf(x * x + y * y + z * z);

	const float fMinDist = 6.0f;	// full volume within this radius (~1200 world units)
	const float fMaxDist = 30.0f;	// reach the floor by here (~6000 world units)
	const float fFloor   = 0.35f;	// never fade fully -- in-range sounds must be heard

	if (fDist <= fMinDist)
		m_fDistanceScale = 1.0f;
	else if (fDist >= fMaxDist)
		m_fDistanceScale = fFloor;
	else
		m_fDistanceScale = 1.0f - (1.0f - fFloor) * ((fDist - fMinDist) / (fMaxDist - fMinDist));

	const float fPanRange = 12.0f;	// |X| beyond this is hard left/right
	float fPanOffset = x / fPanRange;
	fPanOffset = max(-1.0f, min(1.0f, fPanOffset));
	m_fPan = 0.5f + 0.5f * fPanOffset;

	__ApplyVolumePan();
}

void CSoundInstance3D::SetOrientation(float x_face, float y_face, float z_face,
									  float x_normal, float y_normal, float z_normal) const
{
	// Listener orientation is not modelled in the manual 2D-pan spatialisation.
}

void CSoundInstance3D::SetVelocity(float fDistanceX, float fDistanceY, float fDistanceZ, float fNagnitude) const
{
	// Doppler is not modelled in the manual 2D-pan spatialisation.
}

void CSoundInstance3D::UpdatePosition(float fElapsedTime)
{
	// Position is applied immediately in SetPosition(); nothing to advance per-frame.
}
