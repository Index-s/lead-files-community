#pragma once

#include <d3d12.h>
#include <vector>

#include "GrpBase.h"

class CGraphicVertexBuffer : public CGraphicBase
{
	public:
		CGraphicVertexBuffer();
		virtual ~CGraphicVertexBuffer();

		void	Destroy();
		virtual bool	Create(int vtxCount, DWORD fvf, DWORD usage, D3DPOOL d3dPool);

		bool	CreateDeviceObjects();
		void	DestroyDeviceObjects();

		bool	Copy(int bufSize, const void* srcVertices);

		bool	LockRange(unsigned count, void** pretVertices) const;
		bool	Lock(void** pretVertices) const;
		bool	Unlock() const;

		bool	LockDynamic(void** pretVertices);
		virtual bool	Lock(void** pretVertices);
		bool	Unlock();

		void	SetStream(int stride, int layer=0) const;
			
		int		GetVertexCount() const;
		int		GetVertexStride() const;
		DWORD	GetFlexibleVertexFormat() const;

		inline	LPDIRECT3DVERTEXBUFFER9 GetD3DVertexBuffer() const	{ return m_lpd3dVB; }
		inline	DWORD GetBufferSize() const	{ return m_dwBufferSize; }

		bool	IsEmpty() const;

	protected:
		void	Initialize();

		// DX12 twin: source data is captured while a lock is open and the
		// twin rebuilds on unlock (const paths write through the engine's
		// loose lock constness, hence the mutable state).
		void	__CaptureLockDX12(void* pvLocked, UINT uLockedBytes) const;
		void	__RefreshTwinDX12() const;
		void	__DestroyTwinDX12() const;

	protected:
		LPDIRECT3DVERTEXBUFFER9 m_lpd3dVB;

		DWORD					m_dwBufferSize;
		DWORD					m_dwFVF;
		DWORD					m_dwUsage;
		D3DPOOL					m_d3dPool;
		int						m_vtxCount;
		DWORD					m_dwLockFlag;

		mutable ID3D12Resource*	m_pkBufferDX12 = NULL;
		mutable void*			m_pvLockedDX12 = NULL;
		mutable UINT			m_uLockedBytesDX12 = 0;
};
