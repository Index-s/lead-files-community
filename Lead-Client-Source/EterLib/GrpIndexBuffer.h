#pragma once

#include <d3d12.h>
#include <vector>

#include "GrpBase.h"

class CGraphicIndexBuffer : public CGraphicBase
{
	public:
		CGraphicIndexBuffer();
		virtual ~CGraphicIndexBuffer();

		void Destroy();
		bool Create(int idxCount, D3DFORMAT d3dFmt);
		bool Create(int faceCount, TFace* faces);

		bool CreateDeviceObjects();
		void DestroyDeviceObjects();

		bool Copy(int bufSize, const void* srcIndices);

		bool Lock(void** pretIndices) const;
		void Unlock() const;

		bool Lock(void** pretIndices);
		void Unlock();

		void SetIndices(int startIndex=0) const;		

		LPDIRECT3DINDEXBUFFER9 GetD3DIndexBuffer() const;

		int GetIndexCount() const {return m_iidxCount;}

	protected:
		void Initialize();

		void __CaptureLockDX12(void* pvLocked, UINT uLockedBytes) const;
		void __RefreshTwinDX12() const;
		void __DestroyTwinDX12() const;

	protected:
		LPDIRECT3DINDEXBUFFER9	m_lpd3dIdxBuf;
		DWORD					m_dwBufferSize;
		D3DFORMAT				m_d3dFmt;
		int						m_iidxCount;

		mutable std::vector<BYTE>	m_kStorage;

		mutable ID3D12Resource*	m_pkBufferDX12 = NULL;
		mutable void*			m_pvLockedDX12 = NULL;
		mutable UINT			m_uLockedBytesDX12 = 0;
};
