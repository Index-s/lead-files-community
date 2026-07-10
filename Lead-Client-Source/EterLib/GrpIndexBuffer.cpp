#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpIndexBuffer.h"
#include "GrpBackendDX12.h"
#include "StateManager.h"
#include "StateManager.h"

LPDIRECT3DINDEXBUFFER9 CGraphicIndexBuffer::GetD3DIndexBuffer() const
{
	assert(m_lpd3dIdxBuf!=NULL);
	return m_lpd3dIdxBuf;
}

void CGraphicIndexBuffer::SetIndices(int startIndex) const
{
	assert(IsDeviceCreated());
	STATEMANAGER.SetIndices(m_lpd3dIdxBuf, startIndex);	
}


bool CGraphicIndexBuffer::Lock(void** pretIndices) const
{
	assert(m_lpd3dIdxBuf!=NULL);

	if (!m_lpd3dIdxBuf || m_kStorage.empty())
		return false;

	*pretIndices = &m_kStorage[0];
	__CaptureLockDX12(*pretIndices, m_dwBufferSize);
	return true;
}

void CGraphicIndexBuffer::Unlock() const
{
	assert(m_lpd3dIdxBuf!=NULL);

	__RefreshTwinDX12();
}

bool CGraphicIndexBuffer::Lock(void** pretIndices)
{
	assert(m_lpd3dIdxBuf!=NULL);

	if (!m_lpd3dIdxBuf || m_kStorage.empty())
		return false;

	*pretIndices = &m_kStorage[0];
	__CaptureLockDX12(*pretIndices, m_dwBufferSize);
	return true;
}

void CGraphicIndexBuffer::Unlock()
{
	assert(m_lpd3dIdxBuf!=NULL);

	__RefreshTwinDX12();
}

bool CGraphicIndexBuffer::Copy(int bufSize, const void* srcIndices)
{
	assert(m_lpd3dIdxBuf!=NULL);

	BYTE* dstIndices;
	if (!Lock((void**)&dstIndices))
		return false;

	memcpy(dstIndices, srcIndices, bufSize);

	Unlock();
	return true;
}

bool CGraphicIndexBuffer::Create(int faceCount, TFace* faces)
{
	int idxCount = faceCount * 3;
	m_iidxCount = idxCount;
	if (!Create(idxCount, D3DFMT_INDEX16))
		return false;

	WORD* dstIndices;
	if (!Lock((void**)&dstIndices))
		return false;

	for (int i = 0; i<faceCount; ++i, dstIndices+=3)
	{
		TFace * curFace=faces+i;
		dstIndices[0]=curFace->indices[0];
		dstIndices[1]=curFace->indices[1];
		dstIndices[2]=curFace->indices[2];
	}

	Unlock();
	return true;
}

bool CGraphicIndexBuffer::CreateDeviceObjects()
{
	if (0 == m_dwBufferSize)
		return false;

	m_kStorage.assign(m_dwBufferSize, 0);
	m_lpd3dIdxBuf = (LPDIRECT3DINDEXBUFFER9)this;
	return true;
}

void CGraphicIndexBuffer::__CaptureLockDX12(void* pvLocked, UINT uLockedBytes) const
{
	if (CGraphicBackendDX12::GetInstance())
	{
		m_pvLockedDX12 = pvLocked;
		m_uLockedBytesDX12 = uLockedBytes;
	}
}

void CGraphicIndexBuffer::__RefreshTwinDX12() const
{
	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (!pkBackend || !m_pvLockedDX12)
	{
		m_pvLockedDX12 = NULL;
		return;
	}

	__DestroyTwinDX12();

	m_pkBufferDX12 = pkBackend->GetUploader().CreateStaticBuffer(
		pkBackend->GetDevice().GetCommandQueue(), m_pvLockedDX12, m_uLockedBytesDX12,
		D3D12_RESOURCE_STATE_INDEX_BUFFER);
	if (m_pkBufferDX12 && CStateManager::InstancePtr())
		STATEMANAGER.RegisterBufferDX12(m_lpd3dIdxBuf, m_pkBufferDX12,
										D3DFMT_INDEX16 == m_d3dFmt ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT);

	if (D3DFMT_INDEX16 == m_d3dFmt && !m_kStorage.empty() && CStateManager::InstancePtr())
		STATEMANAGER.RegisterIndexData(m_lpd3dIdxBuf,
									   reinterpret_cast<const WORD*>(&m_kStorage[0]),
									   static_cast<UINT>(m_kStorage.size() / sizeof(WORD)));

	m_pvLockedDX12 = NULL;
}

void CGraphicIndexBuffer::__DestroyTwinDX12() const
{
	if (!m_pkBufferDX12)
		return;

	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (pkBackend)
	{
		if (CStateManager::InstancePtr())
			STATEMANAGER.UnregisterBufferDX12(m_lpd3dIdxBuf);
		pkBackend->RetireResource(m_pkBufferDX12);
		m_pkBufferDX12 = NULL;
	}
	else
		safe_release(m_pkBufferDX12);
}

void CGraphicIndexBuffer::DestroyDeviceObjects()
{
	__DestroyTwinDX12();
	if (m_lpd3dIdxBuf && CStateManager::InstancePtr())
		STATEMANAGER.UnregisterIndexData(m_lpd3dIdxBuf);
	std::vector<BYTE>().swap(m_kStorage);
	m_lpd3dIdxBuf = NULL;
}

bool CGraphicIndexBuffer::Create(int idxCount, D3DFORMAT d3dFmt)
{	
	Destroy();
	
	m_iidxCount = idxCount;
	m_dwBufferSize = sizeof(WORD) * idxCount;
	m_d3dFmt = d3dFmt;

	return CreateDeviceObjects();
}

void CGraphicIndexBuffer::Destroy()
{
	DestroyDeviceObjects();
}

void CGraphicIndexBuffer::Initialize()
{
	m_lpd3dIdxBuf=NULL;	
}

CGraphicIndexBuffer::CGraphicIndexBuffer()
{
	Initialize();
}

CGraphicIndexBuffer::~CGraphicIndexBuffer()
{
	Destroy();
}
