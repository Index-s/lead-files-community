#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpVertexBuffer.h"
#include "GrpBackendDX12.h"
#include "StateManager.h"
#include "StateManager.h"

// Local replacement for D3DXGetFVFVertexSize.
static UINT GetFVFVertexSize(DWORD dwFVF)
{
	UINT uSize = 0;

	switch (dwFVF & D3DFVF_POSITION_MASK)
	{
		case D3DFVF_XYZ:	uSize += 3 * sizeof(float); break;
		case D3DFVF_XYZRHW:	uSize += 4 * sizeof(float); break;
		case D3DFVF_XYZW:	uSize += 4 * sizeof(float); break;
		case D3DFVF_XYZB1:	uSize += 4 * sizeof(float); break;
		case D3DFVF_XYZB2:	uSize += 5 * sizeof(float); break;
		case D3DFVF_XYZB3:	uSize += 6 * sizeof(float); break;
		case D3DFVF_XYZB4:	uSize += 7 * sizeof(float); break;
		case D3DFVF_XYZB5:	uSize += 8 * sizeof(float); break;
	}

	if (dwFVF & D3DFVF_NORMAL)
		uSize += 3 * sizeof(float);
	if (dwFVF & D3DFVF_PSIZE)
		uSize += sizeof(float);
	if (dwFVF & D3DFVF_DIFFUSE)
		uSize += sizeof(DWORD);
	if (dwFVF & D3DFVF_SPECULAR)
		uSize += sizeof(DWORD);

	const UINT uTexCount = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
	for (UINT i = 0; i < uTexCount; ++i)
	{
		switch ((dwFVF >> (16 + i * 2)) & 3)
		{
			case D3DFVF_TEXTUREFORMAT1:	uSize += 1 * sizeof(float); break;
			case D3DFVF_TEXTUREFORMAT2:	uSize += 2 * sizeof(float); break;
			case D3DFVF_TEXTUREFORMAT3:	uSize += 3 * sizeof(float); break;
			case D3DFVF_TEXTUREFORMAT4:	uSize += 4 * sizeof(float); break;
		}
	}

	return uSize;
}

int	CGraphicVertexBuffer::GetVertexStride() const
{
	int retSize = GetFVFVertexSize(m_dwFVF);
	return retSize;
}

DWORD CGraphicVertexBuffer::GetFlexibleVertexFormat() const
{
	return m_dwFVF;
}

int CGraphicVertexBuffer::GetVertexCount() const
{
	return m_vtxCount;
}

void CGraphicVertexBuffer::SetStream(int stride, int layer) const
{
	assert(IsDeviceCreated());
	STATEMANAGER.SetStreamSource(layer, m_lpd3dVB, stride);	
}

bool CGraphicVertexBuffer::LockRange(unsigned count, void** pretVertices) const
{
	if (!m_lpd3dVB || m_kStorage.empty())
		return false;

	DWORD dwLockSize=GetVertexStride() * count;
	if (dwLockSize > m_dwBufferSize)
	{
		TraceError("CGraphicVertexBuffer::LockRange: size %u exceeds buffer %u", dwLockSize, m_dwBufferSize);
		return false;
	}

	*pretVertices = &m_kStorage[0];
	__CaptureLockDX12(*pretVertices, dwLockSize);
	return true;
}

bool CGraphicVertexBuffer::Lock(void ** pretVertices) const
{
	if (!m_lpd3dVB || m_kStorage.empty())
		return false;

	DWORD dwLockSize=GetVertexStride()*GetVertexCount();
	if (dwLockSize > m_dwBufferSize)
		dwLockSize = m_dwBufferSize;

	*pretVertices = &m_kStorage[0];
	__CaptureLockDX12(*pretVertices, dwLockSize);
	return true;
}

bool CGraphicVertexBuffer::Unlock() const
{
	if (!m_lpd3dVB)
		return false;

	__RefreshTwinDX12();
	return true;
}

bool CGraphicVertexBuffer::IsEmpty() const
{
	if (m_lpd3dVB)
		return true;
	else
		return false;
}

bool CGraphicVertexBuffer::LockDynamic(void** pretVertices)
{
	if (!m_lpd3dVB || m_kStorage.empty())
		return false;

	*pretVertices = &m_kStorage[0];
	__CaptureLockDX12(*pretVertices, m_dwBufferSize);
	return true;
}

bool CGraphicVertexBuffer::Lock(void ** pretVertices)
{
	if (!m_lpd3dVB || m_kStorage.empty())
		return false;

	*pretVertices = &m_kStorage[0];
	__CaptureLockDX12(*pretVertices, m_dwBufferSize);
	return true;
}

bool CGraphicVertexBuffer::Unlock()
{
	if (!m_lpd3dVB)
		return false;

	__RefreshTwinDX12();
	return true;
}

bool CGraphicVertexBuffer::Copy(int bufSize, const void* srcVertices)
{
	void * dstVertices;

	if (!Lock(&dstVertices))
		return false;

	memcpy(dstVertices, srcVertices, bufSize);
	
	Unlock();
	return true;
}

bool CGraphicVertexBuffer::CreateDeviceObjects()
{
	assert(IsDeviceCreated());
	assert(m_lpd3dVB == NULL);

	if (0 == m_dwBufferSize)
		return false;

	m_kStorage.assign(m_dwBufferSize, 0);
	m_lpd3dVB = (LPDIRECT3DVERTEXBUFFER9)this;
	return true;
}

void CGraphicVertexBuffer::__CaptureLockDX12(void* pvLocked, UINT uLockedBytes) const
{
	if (CGraphicBackendDX12::GetInstance())
	{
		m_pvLockedDX12 = pvLocked;
		m_uLockedBytesDX12 = uLockedBytes;
	}
}

void CGraphicVertexBuffer::__RefreshTwinDX12() const
{
	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (!pkBackend || !m_pvLockedDX12)
	{
		m_pvLockedDX12 = NULL;
		return;
	}

	++m_uRefreshCount;
	if ((m_uRefreshCount > 1 || D3DUSAGE_DYNAMIC == m_dwUsage) &&
		!m_kStorage.empty() && GetVertexStride() > 0)
	{
		if (m_pkBufferDX12)
			__DestroyTwinDX12();
		if (CStateManager::InstancePtr())
			STATEMANAGER.RegisterVertexData(m_lpd3dVB, &m_kStorage[0],
											static_cast<UINT>(m_kStorage.size()),
											static_cast<UINT>(GetVertexStride()));
		m_pvLockedDX12 = NULL;
		return;
	}

	__DestroyTwinDX12();

	m_pkBufferDX12 = pkBackend->GetUploader().CreateStaticBuffer(
		pkBackend->GetDevice().GetCommandQueue(), m_pvLockedDX12, m_uLockedBytesDX12,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	if (m_pkBufferDX12 && CStateManager::InstancePtr())
		STATEMANAGER.RegisterBufferDX12(m_lpd3dVB, m_pkBufferDX12, DXGI_FORMAT_UNKNOWN);
	else if (!m_pkBufferDX12 && !m_kStorage.empty() && GetVertexStride() > 0 && CStateManager::InstancePtr())
		STATEMANAGER.RegisterVertexData(m_lpd3dVB, &m_kStorage[0],
										static_cast<UINT>(m_kStorage.size()),
										static_cast<UINT>(GetVertexStride()));

	m_pvLockedDX12 = NULL;
}

void CGraphicVertexBuffer::__DestroyTwinDX12() const
{
	if (!m_pkBufferDX12)
		return;

	CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance();
	if (pkBackend)
	{
		if (CStateManager::InstancePtr())
			STATEMANAGER.UnregisterBufferDX12(m_lpd3dVB);
		pkBackend->RetireResource(m_pkBufferDX12);
		m_pkBufferDX12 = NULL;
	}
	else
		safe_release(m_pkBufferDX12);
}

void CGraphicVertexBuffer::DestroyDeviceObjects()
{
	__DestroyTwinDX12();
	if (m_lpd3dVB && CStateManager::InstancePtr())
		STATEMANAGER.UnregisterVertexData(m_lpd3dVB);
	m_uRefreshCount = 0;
	std::vector<BYTE>().swap(m_kStorage);
	m_lpd3dVB = NULL;
}

bool CGraphicVertexBuffer::Create(int vtxCount, DWORD fvf, DWORD usage, D3DPOOL d3dPool)
{
	assert(IsDeviceCreated());
	assert(vtxCount > 0);

	Destroy();

	m_vtxCount = vtxCount;
	m_dwBufferSize = GetFVFVertexSize(fvf) * m_vtxCount;
	m_d3dPool = d3dPool;
	m_dwUsage = usage;
	m_dwFVF = fvf;

	if (usage == D3DUSAGE_WRITEONLY || usage == D3DUSAGE_DYNAMIC)
		m_dwLockFlag = 0;
	else
		m_dwLockFlag = D3DLOCK_READONLY;

	return CreateDeviceObjects();
}

void CGraphicVertexBuffer::Destroy()
{
	DestroyDeviceObjects();
}

void CGraphicVertexBuffer::Initialize()
{
	m_lpd3dVB = NULL;
	m_vtxCount = 0;
	m_dwBufferSize = 0;
}

CGraphicVertexBuffer::CGraphicVertexBuffer()
{
	Initialize();
}

CGraphicVertexBuffer::~CGraphicVertexBuffer()
{
	Destroy();
}
