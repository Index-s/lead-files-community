#ifndef __CSTATEMANAGER_H
#define __CSTATEMANAGER_H

#include <d3d9.h>
#include <d3dx9math_shim.h>

#include <vector>
#include <unordered_map>

#include "GrpInputLayoutDX12.h"

#include "../eterBase/Singleton.h"

static const DWORD STATEMANAGER_MAX_RENDERSTATES = 256;
static const DWORD STATEMANAGER_MAX_TEXTURESTATES = 128;
static const DWORD STATEMANAGER_MAX_STAGES = 8;
static const DWORD STATEMANAGER_MAX_VCONSTANTS = 96;
static const DWORD STATEMANAGER_MAX_PCONSTANTS = 8;
static const DWORD STATEMANAGER_MAX_TRANSFORMSTATES = 300;
static const DWORD STATEMANAGER_MAX_STREAMS = 16;

class CStreamData
{
	public:
		CStreamData(const void* pStreamData = NULL, UINT Stride = 0) : m_lpStreamData(pStreamData), m_Stride(Stride)
		{
		}

		bool operator == (const CStreamData& rhs) const
		{
			return ((m_lpStreamData == rhs.m_lpStreamData) && (m_Stride == rhs.m_Stride));
		}

		const void*				m_lpStreamData;
		UINT					m_Stride;
};

class CIndexData
{
	public:
		CIndexData(const void* pIndexData = NULL, UINT BaseVertexIndex = 0)
			: m_lpIndexData(pIndexData),
		m_BaseVertexIndex(BaseVertexIndex)
		{
		}

		bool operator == (const CIndexData& rhs) const
		{
			return ((m_lpIndexData == rhs.m_lpIndexData) && (m_BaseVertexIndex == rhs.m_BaseVertexIndex));
		}

		const void*				m_lpIndexData;
		UINT					m_BaseVertexIndex;
};

typedef enum eStateType
{
	STATE_MATERIAL = 0,
	STATE_RENDER,
	STATE_TEXTURE,
	STATE_TEXTURESTAGE,
	STATE_VSHADER,
	STATE_PSHADER,
	STATE_TRANSFORM,
	STATE_VCONSTANT,
	STATE_PCONSTANT,
	STATE_STREAM,
	STATE_INDEX
} eStateType;

class CStateID
{
	public:
		CStateID(eStateType Type, DWORD dwValue0 = 0, DWORD dwValue1 = 0)
			: m_Type(Type),
		m_dwValue0(dwValue0),
		m_dwValue1(dwValue1)
		{
		}

		CStateID(eStateType Type, DWORD dwStage, D3DTEXTURESTAGESTATETYPE StageType)
			: m_Type(Type),
		m_dwStage(dwStage),
		m_TextureStageStateType(StageType)
		{
		}

		CStateID(eStateType Type, D3DRENDERSTATETYPE RenderType)
			: m_Type(Type),
		m_RenderStateType(RenderType)
		{
		}

		eStateType m_Type;

		union
		{
			DWORD					m_dwValue0;
			DWORD					m_dwStage;
			D3DRENDERSTATETYPE		m_RenderStateType;
			D3DTRANSFORMSTATETYPE	m_TransformStateType;
		};

		union
		{
			DWORD						m_dwValue1;
			D3DTEXTURESTAGESTATETYPE	m_TextureStageStateType;
		};
};

typedef std::vector<CStateID> TStateID;

class CStateManagerState
{
	public:
		CStateManagerState()
		{
		}

		void ResetState()
		{
			DWORD i, y;

			for (i = 0; i < STATEMANAGER_MAX_RENDERSTATES; i++)
				m_RenderStates[i] = 0x7FFFFFFF;

			for (i = 0; i < STATEMANAGER_MAX_STAGES; i++)
				for (y = 0; y < STATEMANAGER_MAX_TEXTURESTATES; y++)
					m_TextureStates[i][y] = 0x7FFFFFFF;

			for (i = 0; i < STATEMANAGER_MAX_STAGES; i++)
				for (y = 0; y < STATEMANAGER_MAX_TEXTURESTATES; y++)
					m_SamplerStates[i][y] = 0x7FFFFFFF;

			for (i = 0; i < STATEMANAGER_MAX_STREAMS; i++)
				m_StreamData[i] = CStreamData();

			m_IndexData = CIndexData();

			for (i = 0; i < STATEMANAGER_MAX_STAGES; i++)
				m_Textures[i] = NULL;

			for (i = 0; i < STATEMANAGER_MAX_TRANSFORMSTATES; i++)
				D3DXMatrixIdentity(&m_Matrices[i]);

			for (i = 0; i < STATEMANAGER_MAX_VCONSTANTS; i++)
				m_VertexShaderConstants[i] = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);

			for (i = 0; i < STATEMANAGER_MAX_PCONSTANTS; i++)
				m_PixelShaderConstants[i] = D3DXVECTOR4(0.0f, 0.0f, 0.0f, 0.0f);

			m_dwPixelShader = NULL;
			m_dwVertexShader = NULL;
			m_dwVertexDeclaration = NULL;
			m_dwFVF = D3DFVF_XYZ;
			m_bVertexProcessing = FALSE;

			ZeroMemory(&m_Matrices, sizeof(D3DXMATRIX) * STATEMANAGER_MAX_TRANSFORMSTATES);
		}

		DWORD					m_RenderStates[STATEMANAGER_MAX_RENDERSTATES];

		DWORD					m_TextureStates[STATEMANAGER_MAX_STAGES][STATEMANAGER_MAX_TEXTURESTATES];
		DWORD					m_SamplerStates[STATEMANAGER_MAX_STAGES][STATEMANAGER_MAX_TEXTURESTATES];

		D3DXVECTOR4				m_VertexShaderConstants[STATEMANAGER_MAX_VCONSTANTS];

		D3DXVECTOR4				m_PixelShaderConstants[STATEMANAGER_MAX_PCONSTANTS];

		const void*				m_Textures[STATEMANAGER_MAX_STAGES];

		const void*				m_dwPixelShader;
		const void*				m_dwVertexShader;
		const void*				m_dwVertexDeclaration;
		DWORD					m_dwFVF;

		D3DXMATRIX				m_Matrices[STATEMANAGER_MAX_TRANSFORMSTATES];

		D3DMATERIAL9			m_D3DMaterial;

		CStreamData				m_StreamData[STATEMANAGER_MAX_STREAMS];
		CIndexData				m_IndexData;

		BOOL					m_bVertexProcessing;
};

class CStateManager : public CSingleton<CStateManager>
{
	public:
		CStateManager();
		virtual ~CStateManager();

		void	SetDefaultState();
		void	Restore();

		bool	BeginScene();
		void	EndScene();

		void	SaveMaterial();
		void	SaveMaterial(const D3DMATERIAL9 * pMaterial);
		void	RestoreMaterial();
		void	SetMaterial(const D3DMATERIAL9 * pMaterial);
		void	GetMaterial(D3DMATERIAL9 * pMaterial);
		BOOL	GetLightEnable(DWORD index);

		void	SetLight(DWORD index, CONST D3DLIGHT9* pLight);
		void	GetLight(DWORD index, D3DLIGHT9* pLight);
		void	LightEnable(DWORD index, BOOL bEnable);

		HRESULT	Clear(DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil);
		HRESULT	SetViewport(const D3DVIEWPORT9* pViewport);
		HRESULT	GetViewport(D3DVIEWPORT9* pViewport);
		void	SaveViewport();
		void	RestoreViewport();

		const void*	GetRenderTarget(DWORD RenderTargetIndex);
		HRESULT		SetRenderTarget(DWORD RenderTargetIndex, const void* pRenderTarget);
		const void*	GetDepthStencilSurface();
		HRESULT		SetDepthStencilSurface(const void* pNewZStencil);

		const void*	CreateVertexShader(CONST DWORD* pFunction);
		const void*	CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements);
		const void*	CreatePixelShader(CONST DWORD* pFunction);

		void	RegisterShaderProgramDX12(const void* pkShader, const char* c_szProgramName);
		void	RegisterTextureSRVDX12(const void* pkTexture, D3D12_CPU_DESCRIPTOR_HANDLE kSRVHandle);
		void	UnregisterTextureSRVDX12(const void* pkTexture);
		void	RegisterBufferDX12(const void* pkBuffer, struct ID3D12Resource* pkResource, DXGI_FORMAT eIndexFormat);
		void	UnregisterBufferDX12(const void* pkBuffer);
		void	SetTransientStream(const void* pvVertices, UINT uVertexCount, UINT uStride);
		void	RegisterIndexData(const void* pkIndexBuffer, const WORD* awIndices, UINT uIndexCount);
		void	UnregisterIndexData(const void* pkIndexBuffer);
		void	RegisterVertexData(const void* pkVertexBuffer, const void* pvVertices, UINT uByteCount, UINT uStride);
		void	UnregisterVertexData(const void* pkVertexBuffer);

	private:
		struct TInputLayoutDX12
		{
			D3D12_INPUT_ELEMENT_DESC	akElements[16];
			UINT						uElementCount;
			UINT						uLayoutID;
		};

		bool	__MirrorDrawDX12(D3DPRIMITIVETYPE ePrimitiveType,
								 const void* pvVertices, UINT uVertexCount, UINT uStrideBytes,
								 const WORD* awIndices, UINT uIndexCount);
		bool	__MirrorPrepareDX12(D3DPRIMITIVETYPE ePrimitiveType, D3D_PRIMITIVE_TOPOLOGY* peTopologyOut);
		bool	__MirrorDrawBuffersDX12(D3DPRIMITIVETYPE ePrimitiveType,
										UINT uStartVertex, UINT uVertexCount,
										UINT uStartIndex, UINT uIndexCount, INT nBaseVertex);
		bool	__MirrorDrawRegisteredDX12(D3DPRIMITIVETYPE ePrimitiveType,
										   UINT uStartVertex, UINT uVertexCount,
										   UINT uStartIndex, UINT uIndexCount, INT nBaseVertex);
		bool	__GetStream0Data(const BYTE** ppbyVertices, UINT* puVertexCount, UINT* puStride) const;

		std::unordered_map<const void*, UINT>				m_kShaderProgramMapDX12;
		std::unordered_map<const void*, TInputLayoutDX12>	m_kDeclLayoutMapDX12;
		std::unordered_map<const void*, SIZE_T>				m_kTextureSRVMapDX12;
		struct TBufferDX12
		{
			struct ID3D12Resource*	pkResource;
			DXGI_FORMAT				eIndexFormat;
		};
		std::unordered_map<const void*, TBufferDX12>		m_kBufferMapDX12;
		const void*	m_pvStream0DX12 = NULL;
		UINT		m_uStream0StrideDX12 = 0;
		const void*	m_pvIndicesDX12 = NULL;
		std::vector<BYTE>	m_kTransientVertexData;
		UINT		m_uTransientVertexCount = 0;
		UINT		m_uTransientVertexStride = 0;
		bool		m_bTransientStream0 = false;
		struct TIndexData
		{
			std::vector<WORD>		kIndices;
			UINT64					uFrameStamp;
			D3D12_INDEX_BUFFER_VIEW	kRingView;
		};
		std::unordered_map<const void*, TIndexData>	m_kIndexDataMap;
		struct TVertexData
		{
			std::vector<BYTE>			kBytes;
			UINT						uStride;
			UINT64						uFrameStamp;
			D3D12_VERTEX_BUFFER_VIEW	kRingView;
		};
		std::unordered_map<const void*, TVertexData>	m_kVertexDataMap;
		UINT	m_uNextLayoutIDDX12 = 1;
		UINT	m_uVertexProgramDX12 = 0xFFFFFFFF;
		UINT	m_uPixelProgramDX12 = 0xFFFFFFFF;

	public:

		void	SaveRenderState(D3DRENDERSTATETYPE Type, DWORD dwValue);
		void	RestoreRenderState(D3DRENDERSTATETYPE Type);
		void	SetRenderState(D3DRENDERSTATETYPE Type, DWORD Value);
		void	GetRenderState(D3DRENDERSTATETYPE Type, DWORD * pdwValue);

		void	SaveTexture(DWORD dwStage, const void* pTexture);
		void	RestoreTexture(DWORD dwStage);
		void	SetTexture(DWORD dwStage, const void* pTexture);
		void	GetTexture(DWORD dwStage, const void** ppTexture);

		void	SaveTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type, DWORD dwValue);
		void	RestoreTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type);
		void	SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type, DWORD dwValue);
		void	GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type, DWORD * pdwValue);
		void	SetBestFiltering(DWORD dwStage);

		void	SaveSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD dwValue);
		void	RestoreSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type);
		void	SetSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD dwValue);
		void	GetSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD * pdwValue);

		void	SaveVertexShader(const void* pShader);
		void	RestoreVertexShader();
		void	SetVertexShader(const void* pShader);
		void	GetVertexShader(const void** ppShader);

		void	SaveVertexDeclaration(const void* pDeclaration);
		void	RestoreVertexDeclaration();
		void	SetVertexDeclaration(const void* pDeclaration);
		void	GetVertexDeclaration(const void** ppDeclaration);

		void	SaveFVF(DWORD dwFVF);
		void	RestoreFVF();
		void	SetFVF(DWORD dwFVF);
		void	GetFVF(DWORD *pdwFVF);

		void	SavePixelShader(const void* pShader);
		void	RestorePixelShader();
		void	SetPixelShader(const void* pShader);
		void	GetPixelShader(const void** ppShader);

		void SaveTransform(D3DTRANSFORMSTATETYPE Transform, const D3DMATRIX* pMatrix);
		void RestoreTransform(D3DTRANSFORMSTATETYPE Transform);

		void SaveVertexProcessing(BOOL IsON);
		void RestoreVertexProcessing();

		void SetTransform(D3DTRANSFORMSTATETYPE Type, const D3DMATRIX* pMatrix);
		void GetTransform(D3DTRANSFORMSTATETYPE Type, D3DMATRIX * pMatrix);

		void SaveVertexShaderConstant(DWORD dwRegister, CONST void* pConstantData, DWORD dwConstantCount);
		void RestoreVertexShaderConstant(DWORD dwRegister, DWORD dwConstantCount);
		void SetVertexShaderConstant(DWORD dwRegister, CONST void* pConstantData, DWORD dwConstantCount);

		void SavePixelShaderConstant(DWORD dwRegister, CONST void* pConstantData, DWORD dwConstantCount);
		void RestorePixelShaderConstant(DWORD dwRegister, DWORD dwConstantCount);
		void SetPixelShaderConstant(DWORD dwRegister, CONST void* pConstantData, DWORD dwConstantCount);

		void SaveStreamSource(UINT StreamNumber, const void* pStreamData, UINT Stride);
		void RestoreStreamSource(UINT StreamNumber);
		void SetStreamSource(UINT StreamNumber, const void* pStreamData, UINT Stride);
		UINT GetStreamStride(UINT StreamNumber) const { return m_CurrentState.m_StreamData[StreamNumber].m_Stride; }

		void SaveIndices(const void* pIndexData, UINT BaseVertexIndex);
		void RestoreIndices();
		void SetIndices(const void* pIndexData, UINT BaseVertexIndex);

		HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
		HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
		HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount);
		HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, CONST void * pIndexData, D3DFORMAT IndexDataFormat, CONST void * pVertexStreamZeroData, UINT VertexStreamZeroStride);

		DWORD GetRenderState(D3DRENDERSTATETYPE Type);

	private:
		void Initialize();

	private:
		CStateManagerState	m_ChipState;
		CStateManagerState	m_CurrentState;
		CStateManagerState	m_CopyState;
		TStateID			m_DirtyStates;
		bool				m_bForce;
		bool				m_bScene;
		D3DVIEWPORT9		m_kViewport;
		D3DVIEWPORT9		m_SavedViewport;
		DWORD				m_dwBestMinFilter;
		DWORD				m_dwBestMagFilter;

#ifdef _DEBUG
		BOOL				m_bRenderStateSavingFlag[STATEMANAGER_MAX_RENDERSTATES];
		BOOL				m_bTextureStageStateSavingFlag[STATEMANAGER_MAX_STAGES][STATEMANAGER_MAX_TEXTURESTATES];
		BOOL				m_bSamplerStateSavingFlag[STATEMANAGER_MAX_STAGES][STATEMANAGER_MAX_TEXTURESTATES];
		BOOL				m_bTransformSavingFlag[STATEMANAGER_MAX_TRANSFORMSTATES];
#endif
};

#define STATEMANAGER (CStateManager::Instance())

#endif
