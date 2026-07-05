#pragma once

// Free-list allocator over non-shader-visible descriptor heaps: every texture
// keeps its SRV here for the backend to copy into the per-draw ring. Grows by
// whole heaps; slots recycle through the free list on texture destruction.

#include <d3d12.h>
#include <vector>

class CGraphicCPUDescriptorsDX12
{
	public:
		enum { HEAP_CAPACITY = 1024 };

		CGraphicCPUDescriptorsDX12();
		~CGraphicCPUDescriptorsDX12();

		bool	Create(ID3D12Device* pkDevice, D3D12_DESCRIPTOR_HEAP_TYPE eHeapType);
		void	Destroy();

		// 0 in *pkHandleOut->ptr never happens on success.
		bool	Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* pkHandleOut);
		void	Free(D3D12_CPU_DESCRIPTOR_HANDLE kHandle);

		UINT	GetAllocatedCount() const;

	private:
		bool	__AddHeap();

		ID3D12Device*						m_pkDevice;
		D3D12_DESCRIPTOR_HEAP_TYPE			m_eHeapType;
		UINT								m_uIncrementSize;
		UINT								m_uAllocatedCount;
		UINT								m_uNextFreshSlot;
		std::vector<ID3D12DescriptorHeap*>	m_kHeaps;
		std::vector<SIZE_T>					m_kFreeList;
};
