#include "StdAfx.h"
#include "../eterBase/Stl.h"
#include "GrpCPUDescriptorsDX12.h"

CGraphicCPUDescriptorsDX12::CGraphicCPUDescriptorsDX12()
	: m_pkDevice(NULL)
	, m_eHeapType(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
	, m_uIncrementSize(0)
	, m_uAllocatedCount(0)
	, m_uNextFreshSlot(0)
{
}

CGraphicCPUDescriptorsDX12::~CGraphicCPUDescriptorsDX12()
{
	Destroy();
}

bool CGraphicCPUDescriptorsDX12::Create(ID3D12Device* pkDevice, D3D12_DESCRIPTOR_HEAP_TYPE eHeapType)
{
	Destroy();

	m_pkDevice = pkDevice;
	m_eHeapType = eHeapType;
	m_uIncrementSize = pkDevice->GetDescriptorHandleIncrementSize(eHeapType);
	return __AddHeap();
}

void CGraphicCPUDescriptorsDX12::Destroy()
{
	for (size_t uPos = 0; uPos != m_kHeaps.size(); ++uPos)
		safe_release(m_kHeaps[uPos]);
	m_kHeaps.clear();
	m_kFreeList.clear();
	m_pkDevice = NULL;
	m_uIncrementSize = 0;
	m_uAllocatedCount = 0;
	m_uNextFreshSlot = 0;
}

bool CGraphicCPUDescriptorsDX12::__AddHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC kHeapDesc = {};
	kHeapDesc.Type = m_eHeapType;
	kHeapDesc.NumDescriptors = HEAP_CAPACITY;

	ID3D12DescriptorHeap* pkHeap = NULL;
	if (FAILED(m_pkDevice->CreateDescriptorHeap(&kHeapDesc, IID_PPV_ARGS(&pkHeap))))
	{
		TraceError("CGraphicCPUDescriptorsDX12: heap creation failed (type %d).",
				   static_cast<int>(m_eHeapType));
		return false;
	}

	m_kHeaps.push_back(pkHeap);
	m_uNextFreshSlot = 0;
	return true;
}

bool CGraphicCPUDescriptorsDX12::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* pkHandleOut)
{
	if (!m_pkDevice)
		return false;

	if (!m_kFreeList.empty())
	{
		pkHandleOut->ptr = m_kFreeList.back();
		m_kFreeList.pop_back();
		++m_uAllocatedCount;
		return true;
	}

	if (m_uNextFreshSlot >= HEAP_CAPACITY && !__AddHeap())
		return false;

	pkHandleOut->ptr = m_kHeaps.back()->GetCPUDescriptorHandleForHeapStart().ptr
		+ static_cast<SIZE_T>(m_uNextFreshSlot) * m_uIncrementSize;
	++m_uNextFreshSlot;
	++m_uAllocatedCount;
	return true;
}

void CGraphicCPUDescriptorsDX12::Free(D3D12_CPU_DESCRIPTOR_HANDLE kHandle)
{
	if (!kHandle.ptr)
		return;

	m_kFreeList.push_back(kHandle.ptr);
	if (m_uAllocatedCount > 0)
		--m_uAllocatedCount;
}

UINT CGraphicCPUDescriptorsDX12::GetAllocatedCount() const
{
	return m_uAllocatedCount;
}
