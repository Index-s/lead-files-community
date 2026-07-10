#include "StdAfx.h"
#include "GrpDevice.h"
#include "GrpBackendDX12.h"
#include "../eterBase/Stl.h"
#include "../eterBase/Debug.h"

bool GRAPHICS_CAPS_CAN_NOT_DRAW_LINE = false;
bool GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW = false;
bool GRAPHICS_CAPS_HALF_SIZE_IMAGE = false;
bool GRAPHICS_CAPS_CAN_NOT_TEXTURE_ADDRESS_BORDER = false;
bool GRAPHICS_CAPS_SOFTWARE_TILING = false;

bool g_isBrowserMode=false;
RECT g_rcBrowser;

namespace
{
	struct SDebugMeshVertex
	{
		float px, py, pz;
		float nx, ny, nz;
	};

	bool CreateDebugMeshBuffers(const std::vector<SDebugMeshVertex>& c_rVertices,
								const std::vector<WORD>& c_rIndices,
								LPDIRECT3DVERTEXBUFFER9* ppVB,
								LPDIRECT3DINDEXBUFFER9* ppIB,
								UINT* puVtxCount,
								UINT* puFaceCount)
	{
		*ppVB = NULL;
		*ppIB = NULL;
		*puVtxCount = 0;
		*puFaceCount = 0;

		if (c_rVertices.empty() || c_rIndices.empty() || !CStateManager::InstancePtr())
			return false;

		*ppVB = (LPDIRECT3DVERTEXBUFFER9)ppVB;
		*ppIB = (LPDIRECT3DINDEXBUFFER9)ppIB;

		STATEMANAGER.RegisterVertexData(*ppVB, &c_rVertices[0],
										UINT(c_rVertices.size() * sizeof(SDebugMeshVertex)), sizeof(SDebugMeshVertex));
		STATEMANAGER.RegisterIndexData(*ppIB, &c_rIndices[0], UINT(c_rIndices.size()));

		*puVtxCount = UINT(c_rVertices.size());
		*puFaceCount = UINT(c_rIndices.size() / 3);
		return true;
	}

	void CreateDebugSphereMesh(float fRadius, UINT uSlices, UINT uStacks,
							   LPDIRECT3DVERTEXBUFFER9* ppVB, LPDIRECT3DINDEXBUFFER9* ppIB,
							   UINT* puVtxCount, UINT* puFaceCount)
	{
		std::vector<SDebugMeshVertex> vertices;
		std::vector<WORD> indices;
		vertices.reserve((uStacks - 1) * uSlices + 2);
		indices.reserve(uStacks * uSlices * 6);

		SDebugMeshVertex v;
		v.px = 0.0f; v.py = 0.0f; v.pz = fRadius;
		v.nx = 0.0f; v.ny = 0.0f; v.nz = 1.0f;
		vertices.push_back(v);

		for (UINT i = 1; i < uStacks; ++i)
		{
			float fPhi = D3DX_PI * float(i) / float(uStacks);
			float fZ = cosf(fPhi);
			float fR = sinf(fPhi);
			for (UINT j = 0; j < uSlices; ++j)
			{
				float fTheta = 2.0f * D3DX_PI * float(j) / float(uSlices);
				v.nx = fR * cosf(fTheta);
				v.ny = fR * sinf(fTheta);
				v.nz = fZ;
				v.px = v.nx * fRadius;
				v.py = v.ny * fRadius;
				v.pz = v.nz * fRadius;
				vertices.push_back(v);
			}
		}

		v.px = 0.0f; v.py = 0.0f; v.pz = -fRadius;
		v.nx = 0.0f; v.ny = 0.0f; v.nz = -1.0f;
		vertices.push_back(v);
		WORD wBottomPole = WORD(vertices.size() - 1);

		auto ring = [uSlices](UINT i, UINT j) -> WORD { return WORD(1 + (i - 1) * uSlices + (j % uSlices)); };

		for (UINT j = 0; j < uSlices; ++j)
		{
			indices.push_back(0);
			indices.push_back(ring(1, j + 1));
			indices.push_back(ring(1, j));
		}
		for (UINT i = 1; i + 1 < uStacks; ++i)
		{
			for (UINT j = 0; j < uSlices; ++j)
			{
				indices.push_back(ring(i, j));
				indices.push_back(ring(i, j + 1));
				indices.push_back(ring(i + 1, j));
				indices.push_back(ring(i + 1, j));
				indices.push_back(ring(i, j + 1));
				indices.push_back(ring(i + 1, j + 1));
			}
		}
		for (UINT j = 0; j < uSlices; ++j)
		{
			indices.push_back(wBottomPole);
			indices.push_back(ring(uStacks - 1, j));
			indices.push_back(ring(uStacks - 1, j + 1));
		}

		CreateDebugMeshBuffers(vertices, indices, ppVB, ppIB, puVtxCount, puFaceCount);
	}

	void CreateDebugCylinderMesh(float fRadius, float fLength, UINT uSlices, UINT uStacks,
								 LPDIRECT3DVERTEXBUFFER9* ppVB, LPDIRECT3DINDEXBUFFER9* ppIB,
								 UINT* puVtxCount, UINT* puFaceCount)
	{
		// Open tube along +z from -fLength/2 to +fLength/2 (matches D3DXCreateCylinder).
		std::vector<SDebugMeshVertex> vertices;
		std::vector<WORD> indices;
		vertices.reserve((uStacks + 1) * uSlices);
		indices.reserve(uStacks * uSlices * 6);

		for (UINT i = 0; i <= uStacks; ++i)
		{
			float fZ = fLength * (float(i) / float(uStacks) - 0.5f);
			for (UINT j = 0; j < uSlices; ++j)
			{
				float fTheta = 2.0f * D3DX_PI * float(j) / float(uSlices);
				SDebugMeshVertex v;
				v.nx = cosf(fTheta);
				v.ny = sinf(fTheta);
				v.nz = 0.0f;
				v.px = v.nx * fRadius;
				v.py = v.ny * fRadius;
				v.pz = fZ;
				vertices.push_back(v);
			}
		}

		auto ring = [uSlices](UINT i, UINT j) -> WORD { return WORD(i * uSlices + (j % uSlices)); };

		for (UINT i = 0; i < uStacks; ++i)
		{
			for (UINT j = 0; j < uSlices; ++j)
			{
				indices.push_back(ring(i, j));
				indices.push_back(ring(i, j + 1));
				indices.push_back(ring(i + 1, j));
				indices.push_back(ring(i + 1, j));
				indices.push_back(ring(i, j + 1));
				indices.push_back(ring(i + 1, j + 1));
			}
		}

		CreateDebugMeshBuffers(vertices, indices, ppVB, ppIB, puVtxCount, puFaceCount);
	}
}

CGraphicDevice::CGraphicDevice()
: m_uBackBufferCount(0)
{
	__Initialize();
}

CGraphicDevice::~CGraphicDevice()
{
	Destroy();
}

void CGraphicDevice::__Initialize()
{
	ms_lpd3dMatStack	= NULL;

	ms_dwWavingEndTime = 0;
	ms_dwFlashingEndTime = 0;

	m_pStateManager		= NULL;

	__InitializeDefaultIndexBufferList();
	__InitializePDTVertexBufferList();
}

void CGraphicDevice::RegisterWarningString(UINT uiMsg, const char * c_szString)
{
	m_kMap_strWarningMessage[uiMsg] = c_szString;
}

void CGraphicDevice::__WarningMessage(HWND hWnd, UINT uiMsg)
{
	if (m_kMap_strWarningMessage.end() == m_kMap_strWarningMessage.find(uiMsg))
		return;
	MessageBox(hWnd, m_kMap_strWarningMessage[uiMsg].c_str(), "Warning", MB_OK|MB_TOPMOST);
}

void CGraphicDevice::MoveWebBrowserRect(const RECT& c_rcWebPage)
{
	g_rcBrowser=c_rcWebPage;
}

void CGraphicDevice::EnableWebBrowserMode(const RECT& c_rcWebPage)
{
	g_isBrowserMode=true;
	g_rcBrowser=c_rcWebPage;
}

void CGraphicDevice::DisableWebBrowserMode()
{
	g_isBrowserMode=false;
}

bool CGraphicDevice::ResizeBackBuffer(UINT uWidth, UINT uHeight)
{
	if (!CStateManager::InstancePtr())
		return false;

	if (int(uWidth)!=ms_iWidth || int(uHeight)!=ms_iHeight)
	{
		ms_iWidth = uWidth;
		ms_iHeight = uHeight;

		if (CGraphicBackendDX12* pkBackend = CGraphicBackendDX12::GetInstance())
		{
			if (!pkBackend->GetDevice().Resize(uWidth, uHeight))
				TraceError("CGraphicDevice: DX12 backbuffer resize failed (%ux%u).", uWidth, uHeight);
		}

		STATEMANAGER.SetDefaultState();

		D3DVIEWPORT9 kViewport;
		kViewport.X = 0;
		kViewport.Y = 0;
		kViewport.Width = uWidth;
		kViewport.Height = uHeight;
		kViewport.MinZ = 0.0f;
		kViewport.MaxZ = 1.0f;
		STATEMANAGER.SetViewport(&kViewport);
		ms_Viewport = kViewport;
	}

	return true;
}

LPDIRECT3DVERTEXDECLARATION9 CGraphicDevice::CreatePNTStreamVertexDeclaration()
{
	LPDIRECT3DVERTEXDECLARATION9 dwShader = NULL;

	D3DVERTEXELEMENT9 pShaderDecl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	if (NULL == (dwShader = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(pShaderDecl)))
	{
		char szError[1024];
		sprintf_s(szError, sizeof(szError), "Failed to create CreatePNTStreamVertexDeclaration");
		MessageBox(NULL, szError, "Vertex Shader Error", MB_ICONSTOP);
	}

	return dwShader;
}

LPDIRECT3DVERTEXDECLARATION9 CGraphicDevice::CreatePNT2StreamVertexDeclaration()
{
	LPDIRECT3DVERTEXDECLARATION9 dwShader = NULL;

	D3DVERTEXELEMENT9 pShaderDecl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
		{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		{ 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
		D3DDECL_END()
	};

	if (NULL == (dwShader = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(pShaderDecl)))
	{
		char szError[1024];
		sprintf_s(szError, sizeof(szError), "Failed to create CreatePNT2StreamVertexDeclaration");
		MessageBox(NULL, szError, "Vertex Shader Error", MB_ICONSTOP);
	}

	return dwShader;
}

LPDIRECT3DVERTEXDECLARATION9 CGraphicDevice::CreatePTStreamVertexDeclaration()
{
	LPDIRECT3DVERTEXDECLARATION9 dwShader = NULL;

	D3DVERTEXELEMENT9 pShaderDecl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
		{ 1, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	if (NULL == (dwShader = (LPDIRECT3DVERTEXDECLARATION9)STATEMANAGER.CreateVertexDeclaration(pShaderDecl)))
	{
		char szError[1024];
		sprintf_s(szError, sizeof(szError), "Failed to create CreatePTStreamVertexDeclaration");
		MessageBox(NULL, szError, "Vertex Shader Error", MB_ICONSTOP);
	}

	return dwShader;
}

DWORD GetMaxTextureWidth()
{
	return 16384;
}

DWORD GetMaxTextureHeight()
{
	return 16384;
}

static CGraphicBackendDX12 gs_kBackendDX12;

int CGraphicDevice::Create(HWND hWnd, int iHres, int iVres, bool Windowed, int /*iBit*/, int /*iReflashRate*/)
{
	Destroy();

	ms_iWidth	= iHres;
	ms_iHeight	= iVres;

	ms_hWnd		= hWnd;
	ms_hDC		= GetDC(hWnd);

	if (!gs_kBackendDX12.Create(hWnd, iHres, iVres, Windowed))
		return CREATE_DEVICE;

	m_pStateManager = new CStateManager;

	ms_ptDecl	= CreatePTStreamVertexDeclaration();
	ms_pntDecl = CreatePNTStreamVertexDeclaration();
	ms_pnt2Decl = CreatePNT2StreamVertexDeclaration();

	D3DXCreateMatrixStack(0, &ms_lpd3dMatStack);
	ms_lpd3dMatStack->LoadIdentity();

	D3DXMatrixIdentity(&ms_matIdentity);
	D3DXMatrixIdentity(&ms_matView);
	D3DXMatrixIdentity(&ms_matProj);
	D3DXMatrixIdentity(&ms_matInverseView);
	D3DXMatrixIdentity(&ms_matInverseViewYAxis);
	D3DXMatrixIdentity(&ms_matScreen0);
	D3DXMatrixIdentity(&ms_matScreen1);
	D3DXMatrixIdentity(&ms_matScreen2);

	ms_matScreen0._11 = 1;
	ms_matScreen0._22 = -1;	

	ms_matScreen1._41 = 1;
	ms_matScreen1._42 = 1;

	ms_matScreen2._11 = (float) iHres / 2;
	ms_matScreen2._22 = (float) iVres / 2;
	
	CreateDebugSphereMesh(1.0f, 32, 32, &ms_lpSphereVB, &ms_lpSphereIB, &ms_uSphereVtxCount, &ms_uSphereFaceCount);
	CreateDebugCylinderMesh(1.0f, 1.0f, 8, 8, &ms_lpCylinderVB, &ms_lpCylinderIB, &ms_uCylinderVtxCount, &ms_uCylinderFaceCount);

	if (!__CreateDefaultIndexBufferList())
		return false;

	if (!__CreatePDTVertexBufferList())
		return false;

	D3DVIEWPORT9 kViewport;
	kViewport.X = 0;
	kViewport.Y = 0;
	kViewport.Width = (DWORD)iHres;
	kViewport.Height = (DWORD)iVres;
	kViewport.MinZ = 0.0f;
	kViewport.MaxZ = 1.0f;
	STATEMANAGER.SetViewport(&kViewport);
	ms_Viewport = kViewport;

	ms_bSupportDXT = true;
	ms_isLowTextureMemory = false;
	ms_isHighTextureMemory = true;
	GRAPHICS_CAPS_CAN_NOT_TEXTURE_ADDRESS_BORDER = false;

	return CREATE_OK;
}

void CGraphicDevice::__InitializePDTVertexBufferList()
{
	ms_smallPdtVertexBuffer = NULL;
	ms_largePdtVertexBuffer = NULL;
}
		
void CGraphicDevice::__DestroyPDTVertexBufferList()
{
	ms_smallPdtVertexBuffer = NULL;
	ms_largePdtVertexBuffer = NULL;
}

bool CGraphicDevice::__CreatePDTVertexBufferList()
{
	ms_smallPdtVertexBuffer = (LPDIRECT3DVERTEXBUFFER9)&ms_smallPdtVertexBuffer;
	ms_largePdtVertexBuffer = (LPDIRECT3DVERTEXBUFFER9)&ms_largePdtVertexBuffer;
	return true;
}

void CGraphicDevice::__InitializeDefaultIndexBufferList()
{
	for (UINT i=0; i<DEFAULT_IB_NUM; ++i)
		ms_alpd3dDefIB[i]=NULL;
}

void CGraphicDevice::__DestroyDefaultIndexBufferList()
{
	for (UINT i=0; i<DEFAULT_IB_NUM; ++i)
		if (ms_alpd3dDefIB[i])
		{
			if (CStateManager::InstancePtr())
				STATEMANAGER.UnregisterIndexData(ms_alpd3dDefIB[i]);
			ms_alpd3dDefIB[i]=NULL;
		}
}

bool CGraphicDevice::__CreateDefaultIndexBuffer(UINT eDefIB, UINT uIdxCount, const WORD* c_awIndices)
{
	assert(ms_alpd3dDefIB[eDefIB]==NULL);

	ms_alpd3dDefIB[eDefIB] = (LPDIRECT3DINDEXBUFFER9)&ms_alpd3dDefIB[eDefIB];

	if (CStateManager::InstancePtr())
		STATEMANAGER.RegisterIndexData(ms_alpd3dDefIB[eDefIB], c_awIndices, uIdxCount);

	return true;
}

bool CGraphicDevice::__CreateDefaultIndexBufferList()
{
	static const WORD c_awLineIndices[2] = { 0, 1, };
	static const WORD c_awLineTriIndices[6] = { 0, 1, 0, 2, 1, 2, };
	static const WORD c_awLineRectIndices[8] = { 0, 1, 0, 2, 1, 3, 2, 3,};
	static const WORD c_awLineCubeIndices[24] = { 
		0, 1, 0, 2, 1, 3, 2, 3,
		0, 4, 1, 5, 2, 6, 3, 7,
		4, 5, 4, 6, 5, 7, 6, 7,
	};
	static const WORD c_awFillTriIndices[3]= { 0, 1, 2, };
	static const WORD c_awFillRectIndices[6] = { 0, 2, 1, 2, 3, 1, };
	static const WORD c_awFillCubeIndices[36] = { 
		0, 1, 2, 1, 3, 2,
		2, 0, 6, 0, 4, 6,
		0, 1, 4, 1, 5, 4,
		1, 3, 5, 3, 7, 5,
		3, 2, 7, 2, 6, 7,
		4, 5, 6, 5, 7, 6,
	};
	
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE, 2, c_awLineIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_TRI, 6, c_awLineTriIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_RECT, 8, c_awLineRectIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_LINE_CUBE, 24, c_awLineCubeIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_TRI, 3, c_awFillTriIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_RECT, 6, c_awFillRectIndices))
		return false;
	if (!__CreateDefaultIndexBuffer(DEFAULT_IB_FILL_CUBE, 36, c_awFillCubeIndices))
		return false;
	
	return true;
}

void CGraphicDevice::InitBackBufferCount(UINT uBackBufferCount)
{
	m_uBackBufferCount=uBackBufferCount;
}

void CGraphicDevice::Destroy()
{
	gs_kBackendDX12.Destroy();

	DestroyShaderPool();
	__DestroyPDTVertexBufferList();
	__DestroyDefaultIndexBufferList();

	if (ms_hDC)
	{
		ReleaseDC(ms_hWnd, ms_hDC);
		ms_hDC = NULL;
	}

	ms_ptDecl = 0;
	ms_pntDecl = 0;
	ms_pnt2Decl = 0;

	if (CStateManager::InstancePtr())
	{
		STATEMANAGER.UnregisterVertexData(ms_lpSphereVB);
		STATEMANAGER.UnregisterIndexData(ms_lpSphereIB);
		STATEMANAGER.UnregisterVertexData(ms_lpCylinderVB);
		STATEMANAGER.UnregisterIndexData(ms_lpCylinderIB);
	}
	ms_lpSphereVB = NULL;
	ms_lpSphereIB = NULL;
	ms_lpCylinderVB = NULL;
	ms_lpCylinderIB = NULL;

	safe_release(ms_lpd3dMatStack);

	if (m_pStateManager)
	{
		delete m_pStateManager;
		m_pStateManager = NULL;
	}

	__Initialize();
}
