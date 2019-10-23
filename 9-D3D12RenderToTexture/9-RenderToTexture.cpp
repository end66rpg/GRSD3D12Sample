#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <fstream>			//for ifstream
#include <wrl.h>			//����WTL֧�� ����ʹ��COM
#include <atlcoll.h>		//for atl array
#include <strsafe.h>		//for StringCchxxxxx function
#include <dxgi1_6.h>
#include <d3d12.h>			//for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <DirectXMath.h>
#include "..\WindowsCommons\d3dx12.h"
#include "..\WindowsCommons\DDSTextureLoader12.h"
using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 Render To Texture Sample")

#define GRS_THROW_IF_FAILED(hr) if (FAILED(hr)){ throw CGRSCOMException(hr); }

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

//------------------------------------------------------------------------------------------------------------
// Ϊ�˵��Լ�����������������ͺ궨�壬Ϊÿ���ӿڶ����������ƣ�����鿴�������
#if defined(_DEBUG)
inline void GRS_SetD3D12DebugName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}

inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		pObject->SetName(_DebugName);
	}
}
#else

inline void GRS_SetD3D12DebugName(ID3D12Object*, LPCWSTR)
{
}
inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_D3D12_DEBUGNAME(x)						GRS_SetD3D12DebugName(x, L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED(x, n)			GRS_SetD3D12DebugNameIndexed(x[n], L#x, n)

#define GRS_SET_D3D12_DEBUGNAME_COMPTR(x)				GRS_SetD3D12DebugName(x.Get(), L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(x, n)	GRS_SetD3D12DebugNameIndexed(x[n].Get(), L#x, n)

#if defined(_DEBUG)
inline void GRS_SetDXGIDebugName(IDXGIObject* pObject, LPCWSTR name)
{
	size_t szLen = 0;
	StringCchLengthW(name, 50, &szLen);
	pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen - 1), name);
}

inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject* pObject, LPCWSTR name, UINT index)
{
	size_t szLen = 0;
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		StringCchLengthW(_DebugName, _countof(_DebugName), &szLen);
		pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen), _DebugName);
	}
}
#else

inline void GRS_SetDXGIDebugName(IDXGIObject*, LPCWSTR)
{
}
inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_DXGI_DEBUGNAME(x)						GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)				GRS_SetDXGIDebugName(x.Get(), L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED_COMPTR(x, n)		GRS_SetDXGIDebugNameIndexed(x[n].Get(), L#x, n)
//------------------------------------------------------------------------------------------------------------

class CGRSCOMException
{
public:
	CGRSCOMException(HRESULT hr) : m_hrError(hr)
	{
	}
	HRESULT Error() const
	{
		return m_hrError;
	}
private:
	const HRESULT m_hrError;
};

// ����ṹ
struct ST_GRS_VERTEX
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

// ����������
struct ST_GRS_MVP
{
	XMFLOAT4X4 m_MVP;			//�����Model-view-projection(MVP)����.
};

// ���ο�Ķ���ṹ
struct ST_GRS_VERTEX_QUAD
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT4 m_vClr;		//Color
	XMFLOAT2 m_vTxc;		//Texcoord
};

// ����ͶӰ�ĳ�������
struct ST_GRS_CB_MVO
{
	XMFLOAT4X4 m_mMVO;	   //Model * View * Orthographic 
};

// �����������
struct ST_GRS_MODULE_PARAMS
{
	UINT								nIndex;
	XMFLOAT4							v4ModelPos;
	TCHAR								pszDDSFile[MAX_PATH];
	CHAR								pszMeshFile[MAX_PATH];
	UINT								nVertexCnt;
	UINT								nIndexCnt;
	ComPtr<ID3D12Resource>				pITexture;
	ComPtr<ID3D12Resource>				pITextureUpload;
	ComPtr<ID3D12DescriptorHeap>		pISampleHp;
	ComPtr<ID3D12Resource>				pIVertexBuffer;
	ComPtr<ID3D12Resource>				pIIndexsBuffer;
	ComPtr<ID3D12DescriptorHeap>		pISRVCBVHp;
	ComPtr<ID3D12Resource>			    pIConstBuffer;
	ComPtr<ID3D12CommandAllocator>		pIBundleAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pIBundle;
	ST_GRS_MVP*							pMVPBuf;
	D3D12_VERTEX_BUFFER_VIEW			stVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				stIndexsBufferView;
};

//��ʼ��Ĭ���������λ��
//��һ���������-Z����
XMFLOAT3 g_f3EyePos1 = XMFLOAT3(0.0f, 5.0f, -10.0f);  //�۾�λ��
XMFLOAT3 g_f3LockAt1 = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
XMFLOAT3 g_f3HeapUp1 = XMFLOAT3(0.0f, 1.0f, 0.0f);    //ͷ�����Ϸ�λ��

float g_fYaw1 = 0.0f;			// ����Z�����ת��.
float g_fPitch1 = 0.0f;			// ��XZƽ�����ת��

double g_fPalstance = 10.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��

//��ʼ��Ĭ���������λ��
//�ڶ��������������Ⱦ��������λ����y�����Ϸ�
XMFLOAT3 g_f3EyePos2 = XMFLOAT3(0.0f, 15.0f, 0.0f);   //�۾�λ��
XMFLOAT3 g_f3LockAt2 = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
XMFLOAT3 g_f3HeapUp2 = XMFLOAT3(0.0f, 0.0f, 1.0f);    //ͷ�����Ϸ�λ�ã�ע��ڶ������������ֱ���µģ����ԡ��ϡ����������Z������

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR*pszMeshFileName, UINT&nVertexCnt, ST_GRS_VERTEX*&ppVertex, UINT*&ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for COM

	int									iWndWidth = 1024;
	int									iWndHeight = 768;

	HWND								hWnd = nullptr;
	MSG									msg = {};
	TCHAR								pszAppPath[MAX_PATH] = {};

	CD3DX12_VIEWPORT					stViewPort(0.0f, 0.0f, static_cast<float>(iWndWidth), static_cast<float>(iWndHeight));
	CD3DX12_RECT						stScissorRect(0, 0, static_cast<LONG>(iWndWidth), static_cast<LONG>(iWndHeight));
	//����Ⱦ�����������ɫ������ʱ���ɫ����Ⱦǰ���ɫ����һ�£���������һ��δ�õ��Ż��ľ���
	const float							f4RTTexClearColor[] = { 0.8f, 0.8f, 0.8f, 0.0f };

	UINT								nDXGIFactoryFlags = 0U;

	const UINT							nFrameBackBufCount = 3u;
	UINT								nCurrentFrameIndex = 0;

	UINT								nRTVDescriptorSize = 0U;

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIAdapter1>				pIAdapter;
	ComPtr<ID3D12Device4>				pID3DDevice;
	ComPtr<ID3D12CommandQueue>			pIMainCmdQueue;
	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;
	ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBufCount];
	ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
	ComPtr<ID3D12DescriptorHeap>		pIDSVHeap;				//��Ȼ�����������
	ComPtr<ID3D12Resource>				pIDepthStencilBuffer;	//������建����

	DXGI_FORMAT							emRTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT							emDSFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ComPtr<ID3D12Fence>					pIFence;
	UINT64								n64FenceValue = 1ui64;
	HANDLE								hFenceEvent = nullptr;

	ComPtr<ID3D12CommandAllocator>		pICmdAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pICmdList;

	CAtlArray<HANDLE>					arHWaited;
	CAtlArray<HANDLE>					arHSubThread;

	ComPtr<ID3DBlob>					pIVSShader;
	ComPtr<ID3DBlob>					pIPSShader;
	ComPtr<ID3D12RootSignature>			pIRSPass1;
	ComPtr<ID3D12PipelineState>			pIPSOPass1;

	ComPtr<ID3D12Resource>				pIResRenderTarget;  //Render Target
	ComPtr<ID3D12Resource>				pIDSTex;
	ComPtr<ID3D12DescriptorHeap>		pIDHRTVTex;
	ComPtr<ID3D12DescriptorHeap>		pIDHDSVTex;


	ComPtr<ID3D12RootSignature>			pIRSQuad;
	ComPtr<ID3D12PipelineState>			pIPSOQuad;
	ComPtr<ID3D12DescriptorHeap>		pIDHQuad;
	ComPtr<ID3D12DescriptorHeap>		pIDHSampleQuad;
	ComPtr<ID3D12Resource>			    pICBMVO;	//��������
	ComPtr<ID3D12Resource>				pIVBQuad;

	D3D12_VERTEX_BUFFER_VIEW			stVBViewQuad = {};
	ST_GRS_CB_MVO*						pMOV = nullptr;
	SIZE_T								szCBMOVBuf = GRS_UPPER(sizeof(ST_GRS_CB_MVO), 256);

	// �����������
	const UINT							nMaxObject = 3;
	const UINT							nSphere = 0;
	const UINT							nCube = 1;
	const UINT							nPlane = 2;
	ST_GRS_MODULE_PARAMS				stModuleParams[nMaxObject] = {};

	// ������������С�϶��뵽256Bytes�߽�
	// D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
	SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);

	UINT								nQuadVertexCnt = 0;
	try
	{
		// �õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
		{
			UINT nBytes = GetCurrentDirectory(MAX_PATH, pszAppPath);
			if (MAX_PATH == nBytes)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// ��������
		{
			//---------------------------------------------------------------------------------------------
			WNDCLASSEX wcex = {};
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_GLOBALCLASS;
			wcex.lpfnWndProc = WndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//��ֹ���ĵı����ػ�
			wcex.lpszClassName = GRS_WND_CLASS_NAME;
			RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, iWndWidth, iWndHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			// ���㴰�ھ��е���Ļ����
			INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
			INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME, GRS_WND_TITLE, dwWndStyle
				, posX, posY, rtWnd.right - rtWnd.left, rtWnd.bottom - rtWnd.top
				, nullptr, nullptr, hInstance, nullptr);

			if (!hWnd)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			ShowWindow(hWnd, nCmdShow);
			UpdateWindow(hWnd);
		}

		// ����ʾ��ϵͳ�ĵ���֧��
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// �򿪸��ӵĵ���֧��
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
		}

		// ����DXGI Factory����
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			// �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize�����������ȹر�
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		// ö�������������豸
		{//ѡ��NUMA�ܹ��Ķ���������3D�豸����,��ʱ�Ȳ�֧�ּ����ˣ���Ȼ������޸���Щ��Ϊ
			D3D12_FEATURE_DATA_ARCHITECTURE stArchitecture = {};
			for (UINT nAdapterIndex = 0; DXGI_ERROR_NOT_FOUND != pIDXGIFactory5->EnumAdapters1(nAdapterIndex, &pIAdapter); ++nAdapterIndex)
			{
				DXGI_ADAPTER_DESC1 stAdapterDesc = {};
				pIAdapter->GetDesc1(&stAdapterDesc);

				if (stAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//�������������������豸
					continue;
				}

				GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3DDevice)));
				GRS_THROW_IF_FAILED(pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE
					, &stArchitecture, sizeof(D3D12_FEATURE_DATA_ARCHITECTURE)));

				if (!stArchitecture.UMA)
				{
					break;
				}

				pID3DDevice.Reset();
			}

			//---------------------------------------------------------------------------------------------
			if (nullptr == pID3DDevice.Get())
			{// �����Ļ����Ͼ�Ȼû�ж��� �������˳����� 
				throw CGRSCOMException(E_FAIL);
			}

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3DDevice);
		}

		// ����ֱ���������
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pIMainCmdQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCmdQueue);
		}

		// ����������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = nFrameBackBufCount;
			stSwapChainDesc.Width = iWndWidth;
			stSwapChainDesc.Height = iWndHeight;
			stSwapChainDesc.Format = emRTFormat;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				pIMainCmdQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);

			// ��ȡ��ǰ��һ�������Ƶĺ󻺳����
			nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//����RTV(��ȾĿ����ͼ)��������(����ѵĺ���Ӧ������Ϊ������߹̶���СԪ�صĹ̶���С�Դ��)
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVHeap);

			//�õ�ÿ��������Ԫ�صĴ�С
			nRTVDescriptorSize = pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			//---------------------------------------------------------------------------------------------
			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < nFrameBackBufCount; i++)
			{//���ѭ����©����������ʵ�����Ǹ�����ı���
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&pIARenderTargets[i])));
				GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIARenderTargets, i);
				pID3DDevice->CreateRenderTargetView(pIARenderTargets[i].Get(), nullptr, stRTVHandle);
				stRTVHandle.Offset(1, nRTVDescriptorSize);
			}
		}

		// ������Ȼ��弰��Ȼ�����������
		{
			D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
			stDepthOptimizedClearValue.Format = emDSFormat;
			stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

			//ʹ����ʽĬ�϶Ѵ���һ��������建������
			//��Ϊ��������Ȼ�������һֱ��ʹ�ã����õ����岻������ֱ��ʹ����ʽ�ѣ�ͼ����
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Tex2D(emDSFormat
					, iWndWidth
					, iWndHeight
					, 1
					, 0
					, 1
					, 0
					, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
				, D3D12_RESOURCE_STATE_DEPTH_WRITE
				, &stDepthOptimizedClearValue
				, IID_PPV_ARGS(&pIDepthStencilBuffer)
			));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDepthStencilBuffer);

			D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
			stDepthStencilDesc.Format = emDSFormat;
			stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&pIDSVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDSVHeap);

			pID3DDevice->CreateDepthStencilView(pIDepthStencilBuffer.Get()
				, &stDepthStencilDesc
				, pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
		}

		// ����ֱ�������б�
		{
			// Ԥ���������б�
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICmdAlloc)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdAlloc);
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICmdAlloc.Get(), nullptr, IID_PPV_ARGS(&pICmdList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdList);
		}

		// ������Ⱦ�����õĹ��ߵĸ�ǩ����״̬����
		{//��������У���������ʹ����ͬ�ĸ�ǩ������Ϊ��Ⱦ��������Ҫ�Ĳ�����һ����
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 stDSPRanges[3];
			stDSPRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			stDSPRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			stDSPRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRootParameters[3];
			stRootParameters[0].InitAsDescriptorTable(1, &stDSPRanges[0], D3D12_SHADER_VISIBILITY_ALL); //CBV������Shader�ɼ�
			stRootParameters[1].InitAsDescriptorTable(1, &stDSPRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);//SRV��PS�ɼ�
			stRootParameters[2].InitAsDescriptorTable(1, &stDSPRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE��PS�ɼ�

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc;
			stRootSignatureDesc.Init_1_1(_countof(stRootParameters)
				, stRootParameters
				, 0
				, nullptr
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;
			GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRootSignatureDesc
				, stFeatureData.HighestVersion
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(pID3DDevice->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSPass1)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSPass1);

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif
			//����Ϊ�о�����ʽ	   
			compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s\\Shader\\TextureCube.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", compileFlags, 0, &pIVSShader, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", compileFlags, 0, &pIPSShader, nullptr));

			// ���Ƕ�������һ�����ߵĶ��壬��ĿǰShader�����ǲ�û��ʹ��
			D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// ���� graphics pipeline state object (PSO)����
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stIALayoutSphere, _countof(stIALayoutSphere) };
			stPSODesc.pRootSignature = pIRSPass1.Get();
			stPSODesc.VS = CD3DX12_SHADER_BYTECODE(pIVSShader.Get());
			stPSODesc.PS = CD3DX12_SHADER_BYTECODE(pIPSShader.Get());
			stPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			stPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.SampleDesc.Count = 1;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFormat;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.DepthStencilState.DepthEnable = TRUE;			//����Ȼ���		
			stPSODesc.DSVFormat = emDSFormat;
			stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//������Ȼ���д�빦��
			stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //��Ȳ��Ժ�������ֵΪ��ͨ����Ȳ��ԣ�����Сֵд�룩

			GRS_THROW_IF_FAILED(pID3DDevice->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPSOPass1)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOPass1);
		}

		// ������Ⱦ�����õĹ��ߵĸ�ǩ����״̬����
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// ����Ƿ�֧��V1.1�汾�ĸ�ǩ��
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			//������Ⱦ���εĸ�ǩ������
			CD3DX12_DESCRIPTOR_RANGE1 stDSPRanges[3];
			stDSPRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			stDSPRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			stDSPRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRootParameters[3];
			stRootParameters[0].InitAsDescriptorTable(1, &stDSPRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);
			stRootParameters[1].InitAsDescriptorTable(1, &stDSPRanges[1], D3D12_SHADER_VISIBILITY_VERTEX);
			stRootParameters[2].InitAsDescriptorTable(1, &stDSPRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc;
			stRootSignatureDesc.Init_1_1(_countof(stRootParameters), stRootParameters
				, 0, nullptr
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;

			GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRootSignatureDesc
				, stFeatureData.HighestVersion
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(pID3DDevice->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSQuad);

			ComPtr<ID3DBlob> pIBlobVertexShader;
			ComPtr<ID3DBlob> pIBlobPixelShader;
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif
			compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s\\Shader\\Quad.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", compileFlags, 0, &pIBlobVertexShader, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", compileFlags, 0, &pIBlobPixelShader, nullptr));

			D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// ���� graphics pipeline state object (PSO)����
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stInputElementDescs, _countof(stInputElementDescs) };
			stPSODesc.pRootSignature = pIRSQuad.Get();
			stPSODesc.VS = CD3DX12_SHADER_BYTECODE(pIBlobVertexShader.Get());
			stPSODesc.PS = CD3DX12_SHADER_BYTECODE(pIBlobPixelShader.Get());
			stPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			stPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			stPSODesc.DepthStencilState.DepthEnable = FALSE;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFormat;
			stPSODesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pID3DDevice->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOQuad);
		}

		// ������ȾĿ������
		{
			D3D12_RESOURCE_DESC stTexDepthsDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				emRTFormat
				, iWndWidth
				, iWndHeight
				, 1
				, 1
				, 1
				, 0
				, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET		//ע�������־������������Ϊ��ȾĿ��ʹ�õĹؼ�
			);
			D3D12_CLEAR_VALUE stClear = {};
			stClear.Format = emRTFormat;
			memcpy(&stClear.Color, &f4RTTexClearColor, 4 * sizeof(float));

			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &stTexDepthsDesc
				, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
				, &stClear
				, IID_PPV_ARGS(&pIResRenderTarget)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIResRenderTarget);

			//����RTV(��ȾĿ����ͼ)��������
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = 1;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIDHRTVTex)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHRTVTex);

			pID3DDevice->CreateRenderTargetView(pIResRenderTarget.Get(), nullptr, pIDHRTVTex->GetCPUDescriptorHandleForHeapStart());

			D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
			stDepthOptimizedClearValue.Format = emDSFormat;
			stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

			//ʹ����ʽĬ�϶Ѵ���һ��������建������
			//��Ϊ��������Ȼ�������һֱ��ʹ�ã����õ����岻������ֱ��ʹ����ʽ�ѣ�ͼ����
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Tex2D(
					emDSFormat
					, iWndWidth
					, iWndHeight
					, 1
					, 0
					, 1
					, 0
					, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
				, D3D12_RESOURCE_STATE_DEPTH_WRITE
				, &stDepthOptimizedClearValue
				, IID_PPV_ARGS(&pIDSTex)
			));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDSTex);

			D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
			stDepthStencilDesc.Format = emDSFormat;
			stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&pIDHDSVTex)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHDSVTex);

			pID3DDevice->CreateDepthStencilView(pIDSTex.Get()
				, &stDepthStencilDesc
				, pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());
		}

		// �������ο�Ķ��㻺��
		{
			//���ο�Ķ���ṹ����
			//ע�����ﶨ����һ����λ��С�������Σ�����ͨ�����ź�λ�ƾ���任�����ʵĴ�С���������������Spirit��UI��2D��Ⱦ
			ST_GRS_VERTEX_QUAD stTriangleVertices[] =
			{
				{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f, 0.0f }  },
				{ { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f },	{ 1.0f, 0.0f }  },
				{ { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },	{ 0.0f, 1.0f }  },
				{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },	{ 1.0f, 1.0f }  }
			};

			UINT nQuadVBSize = sizeof(stTriangleVertices);

			nQuadVertexCnt = _countof(stTriangleVertices);

			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(nQuadVBSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIVBQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVBQuad);

			UINT8* pVertexDataBegin = nullptr;
			CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.
			GRS_THROW_IF_FAILED(pIVBQuad->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, stTriangleVertices, sizeof(stTriangleVertices));
			pIVBQuad->Unmap(0, nullptr);

			stVBViewQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
			stVBViewQuad.StrideInBytes = sizeof(ST_GRS_VERTEX_QUAD);
			stVBViewQuad.SizeInBytes = nQuadVBSize;
		}

		// �������ο���Ⱦ��Ҫ��SRV CBV Sample��
		{
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(szCBMOVBuf),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pICBMVO)));

			GRS_THROW_IF_FAILED(pICBMVO->Map(0, nullptr, reinterpret_cast<void**>(&pMOV)));

			D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
			stHeapDesc.NumDescriptors = 2;
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			//SRV��
			GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHQuad);

			// SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFormat;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			// ʹ����ȾĿ����ΪShader��������Դ
			pID3DDevice->CreateShaderResourceView(pIResRenderTarget.Get(), &stSRVDesc, pIDHQuad->GetCPUDescriptorHandleForHeapStart());

			// CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = pICBMVO->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<UINT>(szCBMOVBuf);

			CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(pIDHQuad->GetCPUDescriptorHandleForHeapStart()
				, 1
				, pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

			pID3DDevice->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);

			//Sample
			stHeapDesc.NumDescriptors = 1;  //ֻ��һ��Sample
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSampleQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSampleQuad);

			// Sample View
			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			pID3DDevice->CreateSampler(&stSamplerDesc, pIDHSampleQuad->GetCPUDescriptorHandleForHeapStart());
		}

		// ׼������������
		{
			USES_CONVERSION;
			// ������Բ���
			
			StringCchPrintf(stModuleParams[nSphere].pszDDSFile, MAX_PATH, _T("%s\\Mesh\\sphere.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nSphere].pszMeshFile, MAX_PATH, "%s\\Mesh\\sphere.txt", T2A(pszAppPath));
			stModuleParams[nSphere].v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

			// ��������Բ���
			StringCchPrintf(stModuleParams[nCube].pszDDSFile, MAX_PATH, _T("%s\\Mesh\\Cube.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nCube].pszMeshFile, MAX_PATH, "%s\\Mesh\\Cube.txt", T2A(pszAppPath));
			stModuleParams[nCube].v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

			// ƽ����Բ���
			StringCchPrintf(stModuleParams[nPlane].pszDDSFile, MAX_PATH, _T("%s\\Mesh\\Plane.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nPlane].pszMeshFile, MAX_PATH, "%s\\Mesh\\Plane.txt", T2A(pszAppPath));
			stModuleParams[nPlane].v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

			// Mesh Value
			ST_GRS_VERTEX*						pstVertices = nullptr;
			UINT*								pnIndices = nullptr;
			// DDS Value
			std::unique_ptr<uint8_t[]>			pbDDSData;
			std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
			DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
			bool								bIsCube = false;

			// ��������Ĺ��Բ�����Ҳ���Ǹ��̵߳Ĺ��Բ���
			for (int i = 0; i < nMaxObject; i++)
			{
				stModuleParams[i].nIndex = i;

				//����ÿ��Module�������
				GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
					, IID_PPV_ARGS(&stModuleParams[i].pIBundleAlloc)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundleAlloc.Get(), _T("pIBundleAlloc"), i);

				GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
					, stModuleParams[i].pIBundleAlloc.Get(), nullptr, IID_PPV_ARGS(&stModuleParams[i].pIBundle)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundle.Get(), _T("pIBundle"), i);

				//����DDS
				pbDDSData.release();
				stArSubResources.clear();
				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(pID3DDevice.Get()
					, stModuleParams[i].pszDDSFile
					, stModuleParams[i].pITexture.GetAddressOf()
					, pbDDSData
					, stArSubResources
					, SIZE_MAX
					, &emAlphaMode
					, &bIsCube));

				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITexture.Get(), _T("pITexture"), i);

				UINT64 n64szUpSphere = GetRequiredIntermediateSize(
					stModuleParams[i].pITexture.Get()
					, 0
					, static_cast<UINT>(stArSubResources.size()));
				D3D12_RESOURCE_DESC stTXDesc = stModuleParams[i].pITexture->GetDesc();

				//�����ϴ���
				GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pITextureUpload)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITextureUpload.Get(), _T("pITextureUpload"), i);

				//�ϴ�DDS
				UpdateSubresources(pICmdList.Get()
					, stModuleParams[i].pITexture.Get()
					, stModuleParams[i].pITextureUpload.Get()
					, 0
					, 0
					, static_cast<UINT>(stArSubResources.size())
					, stArSubResources.data());

				//ͬ��
				pICmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
					stModuleParams[i].pITexture.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));


				//����SRV CBV��
				D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
				stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
				stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stSRVHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISRVCBVHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISRVCBVHp.Get(), _T("pISRVCBVHp"), i);
				
				//����SRV
				D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
				stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				stSRVDesc.Format = stTXDesc.Format;
				stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				stSRVDesc.Texture2D.MipLevels = 1;

				CD3DX12_CPU_DESCRIPTOR_HANDLE stCBVSRVHandle(
					stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart()
					, 1
					, pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

				pID3DDevice->CreateShaderResourceView(stModuleParams[i].pITexture.Get()
					, &stSRVDesc
					, stCBVSRVHandle);

				// ����Sample
				D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
				stSamplerHeapDesc.NumDescriptors = 1;
				stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
				stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stSamplerHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISampleHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISampleHp.Get(), _T("pISampleHp"), i);
			
				D3D12_SAMPLER_DESC stSamplerDesc = {};
				stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				stSamplerDesc.MinLOD = 0;
				stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
				stSamplerDesc.MipLODBias = 0.0f;
				stSamplerDesc.MaxAnisotropy = 1;
				stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
				stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

				pID3DDevice->CreateSampler(&stSamplerDesc
					, stModuleParams[i].pISampleHp->GetCPUDescriptorHandleForHeapStart());

				//������������
				LoadMeshVertex(stModuleParams[i].pszMeshFile, stModuleParams[i].nVertexCnt, pstVertices, pnIndices);
				stModuleParams[i].nIndexCnt = stModuleParams[i].nVertexCnt;

				//���� Vertex Buffer ��ʹ��Upload��ʽ��
				GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX))
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIVertexBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIVertexBuffer.Get(), _T("pIVertexBuffer"), i);
				
				//ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
				UINT8* pVertexDataBegin = nullptr;
				CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.

				GRS_THROW_IF_FAILED(stModuleParams[i].pIVertexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
				memcpy(pVertexDataBegin, pstVertices, stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX));
				stModuleParams[i].pIVertexBuffer->Unmap(0, nullptr);

				//���� Index Buffer ��ʹ��Upload��ʽ��
				GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(stModuleParams[i].nIndexCnt * sizeof(UINT))
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIIndexsBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIIndexsBuffer.Get(), _T("pIIndexsBuffer"), i);

				UINT8* pIndexDataBegin = nullptr;
				GRS_THROW_IF_FAILED(stModuleParams[i].pIIndexsBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
				memcpy(pIndexDataBegin, pnIndices, stModuleParams[i].nIndexCnt * sizeof(UINT));
				stModuleParams[i].pIIndexsBuffer->Unmap(0, nullptr);

				//����Vertex Buffer View
				stModuleParams[i].stVertexBufferView.BufferLocation = stModuleParams[i].pIVertexBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX);
				stModuleParams[i].stVertexBufferView.SizeInBytes = stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX);

				//����Index Buffer View
				stModuleParams[i].stIndexsBufferView.BufferLocation = stModuleParams[i].pIIndexsBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stIndexsBufferView.Format = DXGI_FORMAT_R32_UINT;
				stModuleParams[i].stIndexsBufferView.SizeInBytes = stModuleParams[i].nIndexCnt * sizeof(UINT);

				::HeapFree(::GetProcessHeap(), 0, pstVertices);
				::HeapFree(::GetProcessHeap(), 0, pnIndices);

				// ������������ ע�⻺��ߴ�����Ϊ256�߽�����С
				GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(szMVPBuf)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIConstBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIConstBuffer.Get(), _T("pIConstBuffer"), i);
				// Map ֮��Ͳ���Unmap�� ֱ�Ӹ������ݽ�ȥ ����ÿ֡������map-copy-unmap�˷�ʱ����
				GRS_THROW_IF_FAILED(stModuleParams[i].pIConstBuffer->Map(0, nullptr, reinterpret_cast<void**>(&stModuleParams[i].pMVPBuf)));
				//---------------------------------------------------------------------------------------------

				// ����CBV
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation = stModuleParams[i].pIConstBuffer->GetGPUVirtualAddress();
				cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);
				pID3DDevice->CreateConstantBufferView(&cbvDesc, stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart());
			}
		}

		// ��¼����������Ⱦ�������
		// ������������Ⱦ���õ�����������������ʾ����������ڴ�������÷��������
		{
			//	������Ⱦ���������һ����
			for (int i = 0; i < nMaxObject; i++)
			{
				stModuleParams[i].pIBundle->SetGraphicsRootSignature(pIRSPass1.Get());
				stModuleParams[i].pIBundle->SetPipelineState(pIPSOPass1.Get());

				ID3D12DescriptorHeap* ppHeapsSphere[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
				stModuleParams[i].pIBundle->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);

				//����SRV
				CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere(stModuleParams[i].pISRVCBVHp->GetGPUDescriptorHandleForHeapStart());
				stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(0, stGPUCBVHandleSphere);

				stGPUCBVHandleSphere.Offset(1, pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
				//����CBV
				stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleSphere);

				//����Sample
				stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(2, stModuleParams[i].pISampleHp->GetGPUDescriptorHandleForHeapStart());

				//ע������ʹ�õ���Ⱦ�ַ����������б���Ҳ����ͨ����Mesh����
				stModuleParams[i].pIBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				stModuleParams[i].pIBundle->IASetVertexBuffers(0, 1, &stModuleParams[i].stVertexBufferView);
				stModuleParams[i].pIBundle->IASetIndexBuffer(&stModuleParams[i].stIndexsBufferView);

				//Draw Call������
				stModuleParams[i].pIBundle->DrawIndexedInstanced(stModuleParams[i].nIndexCnt, 1, 0, 0, 0);
				stModuleParams[i].pIBundle->Close();
			}
			//End for
		}

		// ����Χ������
		{
			GRS_THROW_IF_FAILED(pID3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIFence);

			//����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
			hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hFenceEvent == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		UINT64 n64fence = 0;
		CAtlArray<ID3D12CommandList*> arCmdList;

		// ִ�еڶ���Copy����ȴ����е��������ϴ�����Ĭ�϶���
		{
			GRS_THROW_IF_FAILED(pICmdList->Close());
			arCmdList.RemoveAll();
			arCmdList.Add(pICmdList.Get());
			pIMainCmdQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

			// �ȴ�������Դ��ʽ���������
			const UINT64 fence = n64FenceValue;
			GRS_THROW_IF_FAILED(pIMainCmdQueue->Signal(pIFence.Get(), fence));
			n64FenceValue++;
			GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(fence, hFenceEvent));
		}

	
		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;

		// ������ת�Ƕ���Ҫ�ı���
		double dModelRotationYAngle = 0.0f;

		XMMATRIX mxRotY;
		// ����ͶӰ����
		XMMATRIX mxProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)iWndWidth / (FLOAT)iWndHeight, 0.1f, 1000.0f);

		// ��������ͶӰ���� Orthographic
		// �������Ͻ�������ԭ�� X���������� Y���������� �봰������ϵ��ͬ
		// ������㷨�����ڹ��ϵ�HGE�������
		// Orthographic
		XMMATRIX mxOrthographic = XMMatrixScaling(1.0f, -1.0f, 1.0f); //���·�ת�����֮ǰ��Quad��Vertex����һ��ʹ��
		mxOrthographic = XMMatrixMultiply(mxOrthographic, XMMatrixTranslation(-0.5f, +0.5f, 0.0f)); //΢��ƫ�ƣ���������λ��
		mxOrthographic = XMMatrixMultiply(
			mxOrthographic
			, XMMatrixOrthographicOffCenterLH(stViewPort.TopLeftX
				, stViewPort.TopLeftX + stViewPort.Width
				, -(stViewPort.TopLeftY + stViewPort.Height)
				, -stViewPort.TopLeftY
				, stViewPort.MinDepth
				, stViewPort.MaxDepth)
		);
		// View * Orthographic ����ͶӰʱ���Ӿ���ͨ����һ����λ��������֮���Գ�һ���Ա�ʾ����ӰͶӰһ��
		mxOrthographic = XMMatrixMultiply(XMMatrixIdentity(), mxOrthographic);

		DWORD dwRet = 0;
		BOOL bExit = FALSE;
		// ��ʼ��Ϣѭ�����������в�����Ⱦ
		while (!bExit)
		{//ע���������ǵ�������Ϣѭ�������ȴ�ʱ������Ϊ0��ͬʱ����ʱ�Ե���Ⱦ���ĳ���ÿ��ѭ������Ⱦ
		 //�ر�ע����εȴ���֮ǰ��ͬ
			//���߳̽���ȴ�
			dwRet = ::MsgWaitForMultipleObjects(1, &hFenceEvent, FALSE, INFINITE, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{//��һ�������Ѿ�ִ�н�������ʼ��һ֡��Ⱦ
				//OnUpdate()
				{//������ת����
					n64tmCurrent = ::GetTickCount();
					//������ת�ĽǶȣ���ת�Ƕ�(����) = ʱ��(��) * ���ٶ�(����/��)
					//�����������൱�ھ�����Ϸ��Ϣѭ���е�OnUpdate��������Ҫ��������
					dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;

					n64tmFrameStart = n64tmCurrent;

					//��ת�Ƕ���2PI���ڵı�����ȥ����������ֻ�������0���ȿ�ʼ��С��2PI�Ļ��ȼ���
					if (dModelRotationYAngle > XM_2PI)
					{
						dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
					}
					mxRotY = XMMatrixRotationY(static_cast<float>(dModelRotationYAngle));

					//���� �Ӿ��� view * �ü����� projection
					XMMATRIX xmMVP = XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos2)
						, XMLoadFloat3(&g_f3LockAt2)
						, XMLoadFloat3(&g_f3HeapUp2))
						, mxProjection);

					//���������MVP
					xmMVP = XMMatrixMultiply(mxRotY, xmMVP);

					for (int i = 0; i < nMaxObject; i++)
					{
						XMMATRIX xmModulePos = XMMatrixTranslation(stModuleParams[i].v4ModelPos.x
							, stModuleParams[i].v4ModelPos.y
							, stModuleParams[i].v4ModelPos.z);

						xmModulePos = XMMatrixMultiply(xmModulePos, xmMVP);

						XMStoreFloat4x4(&stModuleParams[i].pMVPBuf->m_MVP, xmModulePos);
					}

				}

				//��һ����Ⱦ����Ⱦ������
				{
					nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

					GRS_THROW_IF_FAILED(pICmdAlloc->Reset());
					GRS_THROW_IF_FAILED(pICmdList->Reset(pICmdAlloc.Get(), pIPSOPass1.Get()));

					//�������Ͻ���ȾĿ����������Դת��Ϊ��ȾĿ��״̬
					pICmdList->ResourceBarrier(1
						, &CD3DX12_RESOURCE_BARRIER::Transition(
							pIResRenderTarget.Get()
							, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
							, D3D12_RESOURCE_STATE_RENDER_TARGET)
					);
					CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIDHRTVTex->GetCPUDescriptorHandleForHeapStart());
					CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());

					//������ȾĿ��
					pICmdList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &dsvHandle);

					pICmdList->RSSetViewports(1, &stViewPort);
					pICmdList->RSSetScissorRects(1, &stScissorRect);

					pICmdList->ClearRenderTargetView(stRTVHandle, f4RTTexClearColor, 0, nullptr);
					pICmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

					//��һ��ִ��ÿ����������ĵ����������Ⱦ������
					for (int i = 0; i < nMaxObject; i++)
					{
						ID3D12DescriptorHeap* ppHeapsSkybox[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
						pICmdList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
						pICmdList->ExecuteBundle(stModuleParams[i].pIBundle.Get());
					}

					//��Ⱦ��Ͻ���ȾĿ����������ȾĿ��״̬ת��������״̬����׼����ʾ
					pICmdList->ResourceBarrier(1
						, &CD3DX12_RESOURCE_BARRIER::Transition(
							pIResRenderTarget.Get()
							, D3D12_RESOURCE_STATE_RENDER_TARGET
							, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

					//�ر������б���ȥִ��
					GRS_THROW_IF_FAILED(pICmdList->Close());
					arCmdList.RemoveAll();
					arCmdList.Add(pICmdList.Get());
					pIMainCmdQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//��ʼͬ��GPU��CPU��ִ��
					n64fence = n64FenceValue;
					GRS_THROW_IF_FAILED(pIMainCmdQueue->Signal(pIFence.Get(), n64fence));
					n64FenceValue++;
					if (pIFence->GetCompletedValue() < n64fence)
					{
						GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(n64fence, hFenceEvent));
						WaitForSingleObject(hFenceEvent, INFINITE);
					}
					GRS_THROW_IF_FAILED(pICmdAlloc->Reset());
					GRS_THROW_IF_FAILED(pICmdList->Reset(pICmdAlloc.Get(), pIPSOPass1.Get()));
				}

				
				{
					//���� �Ӿ��� view * �ü����� projection
					XMMATRIX xmMVP = XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos1)
						, XMLoadFloat3(&g_f3LockAt1)
						, XMLoadFloat3(&g_f3HeapUp1))
						, mxProjection);

					//���������MVP
					xmMVP = XMMatrixMultiply(mxRotY, xmMVP);

					for (int i = 0; i < nMaxObject; i++)
					{
						XMMATRIX xmModulePos = XMMatrixTranslation(stModuleParams[i].v4ModelPos.x
							, stModuleParams[i].v4ModelPos.y
							, stModuleParams[i].v4ModelPos.z);

						xmModulePos = XMMatrixMultiply(xmModulePos, xmMVP);

						XMStoreFloat4x4(&stModuleParams[i].pMVPBuf->m_MVP, xmModulePos);
					}
				}

				//�ڶ�����Ⱦ����Ⱦ����Ļ������ʾ������Ⱦ�Ľ��
				{
					pICmdList->ResourceBarrier(1
						, &CD3DX12_RESOURCE_BARRIER::Transition(
							pIARenderTargets[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_PRESENT
							, D3D12_RESOURCE_STATE_RENDER_TARGET)
					);

					//ƫ��������ָ�뵽ָ��֡������ͼλ��
					CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
						, nCurrentFrameIndex, nRTVDescriptorSize);
					CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
					//������ȾĿ��
					pICmdList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &dsvHandle);

					pICmdList->RSSetViewports(1, &stViewPort);
					pICmdList->RSSetScissorRects(1, &stScissorRect);
					const float clearColor[] = { 0.0f, 0.0f, 0.5f, 1.0f };
					pICmdList->ClearRenderTargetView(stRTVHandle, clearColor, 0, nullptr);
					pICmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

					//��һ��ִ��ÿ����������ĵ����������Ⱦ����Ļ
					for (int i = 0; i < nMaxObject; i++)
					{
						ID3D12DescriptorHeap* ppHeapsSkybox[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
						pICmdList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
						pICmdList->ExecuteBundle(stModuleParams[i].pIBundle.Get());
					}

					// ��Ⱦ���ο�
					// ���þ��ο��λ�ã����������Ͻǵ����꣬ע����Ϊ����ͶӰ��Ե�ʣ����ﵥλ������
					XMMATRIX xmMVO = XMMatrixMultiply(
						XMMatrixTranslation(10.0f, 10.0f, 0.0f)
						, mxOrthographic);

					// ���þ��ο�Ĵ�С����λͬ��������
					xmMVO = XMMatrixMultiply(
						XMMatrixScaling(320.0f, 240.0f, 1.0f)
						, xmMVO);

					//����MVO
					XMStoreFloat4x4(&pMOV->m_mMVO, xmMVO);

					// ���ƾ���
					pICmdList->SetGraphicsRootSignature(pIRSQuad.Get());
					pICmdList->SetPipelineState(pIPSOQuad.Get());
					ID3D12DescriptorHeap* ppHeapsQuad[] = { pIDHQuad.Get(),pIDHSampleQuad.Get() };
					pICmdList->SetDescriptorHeaps(_countof(ppHeapsQuad), ppHeapsQuad);

					CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandle(pIDHQuad->GetGPUDescriptorHandleForHeapStart());
					pICmdList->SetGraphicsRootDescriptorTable(0, stGPUCBVHandle);
					stGPUCBVHandle.Offset(1, pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
					pICmdList->SetGraphicsRootDescriptorTable(1, stGPUCBVHandle);
					pICmdList->SetGraphicsRootDescriptorTable(2, pIDHSampleQuad->GetGPUDescriptorHandleForHeapStart());

					pICmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					pICmdList->IASetVertexBuffers(0, 1, &stVBViewQuad);
					//Draw Call������
					pICmdList->DrawInstanced(nQuadVertexCnt, 1, 0, 0);
					
					pICmdList->ResourceBarrier(1
						, &CD3DX12_RESOURCE_BARRIER::Transition(
							pIARenderTargets[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_RENDER_TARGET
							, D3D12_RESOURCE_STATE_PRESENT));

					//�ر������б���ȥִ��
					GRS_THROW_IF_FAILED(pICmdList->Close());
					arCmdList.RemoveAll();
					arCmdList.Add(pICmdList.Get());
					pIMainCmdQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//�ύ����
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					//��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
					n64fence = n64FenceValue;
					GRS_THROW_IF_FAILED(pIMainCmdQueue->Signal(pIFence.Get(), n64fence));
					n64FenceValue++;
					GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(n64fence, hFenceEvent));
				}
			}
			break;
			case 1:
			{//������Ϣ
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{
					if (WM_QUIT != msg.message)
					{
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
					else
					{
						bExit = TRUE;
					}
				}
			}
			break;
			case WAIT_TIMEOUT:
			{
			}
			break;
			default:
				break;
			}
		}
	}
	catch (CGRSCOMException& e)
	{//������COM�쳣
		e;
	}

	::CoUninitialize();
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	{
		USHORT n16KeyCode = (wParam & 0xFF);
		if (VK_SPACE == n16KeyCode)
		{//���ո���л���ͬ�Ĳ�������Ч����������ÿ�ֲ���������ĺ���
			//UINT g_nCurrentSamplerNO = 0; //��ǰʹ�õĲ���������
			//UINT g_nSampleMaxCnt = 5;		//����������͵Ĳ�����
			//++g_nCurrentSamplerNO;
			//g_nCurrentSamplerNO %= g_nSampleMaxCnt;

			//=================================================================================================
			//������������������
			//pICmdListSphere->Reset(pICmdAllocSphere.Get(), pIPSOPass1.Get());
			//pICmdListSphere->SetGraphicsRootSignature(pIRSPass1.Get());
			//pICmdListSphere->SetPipelineState(pIPSOPass1.Get());
			//ID3D12DescriptorHeap* ppHeapsSphere[] = { pISRVCBVHp.Get(),pISampleHp.Get() };
			//pICmdListSphere->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);
			////����SRV
			//pICmdListSphere->SetGraphicsRootDescriptorTable(0, pISRVCBVHp->GetGPUDescriptorHandleForHeapStart());

			//CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere(pISRVCBVHp->GetGPUDescriptorHandleForHeapStart()
			//	, 1
			//	, pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
			////����CBV
			//pICmdListSphere->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleSphere);
			//CD3DX12_GPU_DESCRIPTOR_HANDLE hGPUSamplerSphere(pISampleHp->GetGPUDescriptorHandleForHeapStart()
			//	, g_nCurrentSamplerNO
			//	, nSamplerDescriptorSize);
			////����Sample
			//pICmdListSphere->SetGraphicsRootDescriptorTable(2, hGPUSamplerSphere);
			////ע������ʹ�õ���Ⱦ�ַ����������б���Ҳ����ͨ����Mesh����
			//pICmdListSphere->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//pICmdListSphere->IASetVertexBuffers(0, 1, &stVBV);
			//pICmdListSphere->IASetIndexBuffer(&stIBV);

			////Draw Call������
			//pICmdListSphere->DrawIndexedInstanced(nIndexCnt, 1, 0, 0, 0);
			//pICmdListSphere->Close();
			//=================================================================================================
		}
		if (VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode)
		{
			//double g_fPalstance = 10.0f * XM_PI / 180.0f;	//������ת�Ľ��ٶȣ���λ������/��
			g_fPalstance += 10 * XM_PI / 180.0f;
			if (g_fPalstance > XM_PI)
			{
				g_fPalstance = XM_PI;
			}
			//XMMatrixOrthographicOffCenterLH()
		}

		if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
		{
			g_fPalstance -= 10 * XM_PI / 180.0f;
			if (g_fPalstance < 0.0f)
			{
				g_fPalstance = XM_PI / 180.0f;
			}
		}

		//�����û�����任
		//XMVECTOR g_f3EyePos1 = XMVectorSet(0.0f, 5.0f, -10.0f, 0.0f); //�۾�λ��
		//XMVECTOR g_f3LockAt1 = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  //�۾�������λ��
		//XMVECTOR g_f3HeapUp1 = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  //ͷ�����Ϸ�λ��
		XMFLOAT3 move(0, 0, 0);
		float fMoveSpeed = 2.0f;
		float fTurnSpeed = XM_PIDIV2 * 0.005f;

		if ('w' == n16KeyCode || 'W' == n16KeyCode)
		{
			move.z -= 1.0f;
		}

		if ('s' == n16KeyCode || 'S' == n16KeyCode)
		{
			move.z += 1.0f;
		}

		if ('d' == n16KeyCode || 'D' == n16KeyCode)
		{
			move.x += 1.0f;
		}

		if ('a' == n16KeyCode || 'A' == n16KeyCode)
		{
			move.x -= 1.0f;
		}

		if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
		{
			XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
			move.x = XMVectorGetX(vector);
			move.z = XMVectorGetZ(vector);
		}

		if (VK_UP == n16KeyCode)
		{
			g_fPitch1 += fTurnSpeed;
		}

		if (VK_DOWN == n16KeyCode)
		{
			g_fPitch1 -= fTurnSpeed;
		}

		if (VK_RIGHT == n16KeyCode)
		{
			g_fYaw1 -= fTurnSpeed;
		}

		if (VK_LEFT == n16KeyCode)
		{
			g_fYaw1 += fTurnSpeed;
		}

		// Prevent looking too far up or down.
		g_fPitch1 = min(g_fPitch1, XM_PIDIV4);
		g_fPitch1 = max(-XM_PIDIV4, g_fPitch1);

		// Move the camera in model space.
		float x = move.x * -cosf(g_fYaw1) - move.z * sinf(g_fYaw1);
		float z = move.x * sinf(g_fYaw1) - move.z * cosf(g_fYaw1);
		g_f3EyePos1.x += x * fMoveSpeed;
		g_f3EyePos1.z += z * fMoveSpeed;

		// Determine the look direction.
		float r = cosf(g_fPitch1);
		g_f3LockAt1.x = r * sinf(g_fYaw1);
		g_f3LockAt1.y = sinf(g_fPitch1);
		g_f3LockAt1.z = r * cosf(g_fYaw1);

		if (VK_TAB == n16KeyCode)
		{//��Tab����ԭ�����λ��
			g_f3EyePos1 = XMFLOAT3(0.0f, 0.0f, -10.0f); //�۾�λ��
			g_f3LockAt1 = XMFLOAT3(0.0f, 0.0f, 0.0f);    //�۾�������λ��
			g_f3HeapUp1 = XMFLOAT3(0.0f, 1.0f, 0.0f);    //ͷ�����Ϸ�λ��
		}

	}

	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL LoadMeshVertex(const CHAR*pszMeshFileName, UINT&nVertexCnt, ST_GRS_VERTEX*&ppVertex, UINT*&ppIndices)
{
	ifstream fin;
	char input;
	BOOL bRet = TRUE;
	try
	{
		fin.open(pszMeshFileName);
		if (fin.fail())
		{
			throw CGRSCOMException(E_FAIL);
		}
		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin >> nVertexCnt;

		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin.get(input);
		fin.get(input);

		ppVertex = (ST_GRS_VERTEX*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(ST_GRS_VERTEX));
		ppIndices = (UINT*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(UINT));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_vPos.x >> ppVertex[i].m_vPos.y >> ppVertex[i].m_vPos.z;
			ppVertex[i].m_vPos.w = 1.0f;
			fin >> ppVertex[i].m_vTex.x >> ppVertex[i].m_vTex.y;
			fin >> ppVertex[i].m_vNor.x >> ppVertex[i].m_vNor.y >> ppVertex[i].m_vNor.z;

			ppIndices[i] = i;
		}
	}
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}