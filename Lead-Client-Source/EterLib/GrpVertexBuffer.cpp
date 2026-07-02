#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpVertexBuffer.h"
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
	if (!m_lpd3dVB)
		return false;

	DWORD dwLockSize=GetVertexStride() * count;
	HRESULT hr = m_lpd3dVB->Lock(0, dwLockSize, pretVertices, m_dwLockFlag);
	if (FAILED(hr))
	{
		TraceError("CGraphicVertexBuffer::LockRange: hr=0x%08X usage=%u lock=%u pool=%u", hr, m_dwUsage, m_dwLockFlag, m_d3dPool);
		return false;
	}

	return true;
}

bool CGraphicVertexBuffer::Lock(void ** pretVertices) const
{
	if (!m_lpd3dVB)
		return false;

	DWORD dwLockSize=GetVertexStride()*GetVertexCount();
	HRESULT hr = m_lpd3dVB->Lock(0, dwLockSize, pretVertices, m_dwLockFlag);
	if (FAILED(hr))
	{
		TraceError("CGraphicVertexBuffer::Lock: hr=0x%08X usage=%u lock=%u pool=%u", hr, m_dwUsage, m_dwLockFlag, m_d3dPool);
		return false;
	}

	return true;
}

bool CGraphicVertexBuffer::Unlock() const
{
	if (!m_lpd3dVB)
		return false;

	if ( FAILED(m_lpd3dVB->Unlock()) )
		return false;
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
	if (!m_lpd3dVB)
		return false;

	HRESULT hr = m_lpd3dVB->Lock(0, 0, pretVertices, 0);
	if (FAILED(hr))
	{
		TraceError("CGraphicVertexBuffer::LockDynamic: hr=0x%08X usage=%u lock=0 pool=%u", hr, m_dwUsage, m_d3dPool);
		return false;
	}

	return true;
}

bool CGraphicVertexBuffer::Lock(void ** pretVertices)
{
	if (!m_lpd3dVB)
		return false;

	HRESULT hr = m_lpd3dVB->Lock(0, 0, pretVertices, m_dwLockFlag);
	if (FAILED(hr))
	{
		TraceError("CGraphicVertexBuffer::Lock: hr=0x%08X usage=%u lock=%u pool=%u", hr, m_dwUsage, m_dwLockFlag, m_d3dPool);
		return false;
	}

	return true;
}

bool CGraphicVertexBuffer::Unlock()
{
	if (!m_lpd3dVB)
		return false;

	if ( FAILED(m_lpd3dVB->Unlock()) )
		return false;
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

	if (FAILED(
		CreateDeviceVertexBuffer(
		m_dwBufferSize,
		m_dwUsage,
		m_dwFVF,
		m_d3dPool,
		&m_lpd3dVB)
		))
		return false;

	return true;
}

void CGraphicVertexBuffer::DestroyDeviceObjects()
{
	safe_release(m_lpd3dVB);
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
