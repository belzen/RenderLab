#include "Precompiled.h"
#include "RdrContext.h"
#include "RdrDrawOp.h"
#include "RdrDrawState.h"
#include "RdrProfiler.h"

#include "pix3.h"
#include <d3dcompiler.h>
#include <dxgidebug.h>

#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "WinPixEventRuntime.lib")
#pragma comment (lib, "dxguid.lib")

//dxgi.lib;d3d12.lib;d3d11.lib;d2d1.lib;dwrite.lib;dxguid.lib;
namespace
{
	static const int kMaxNumRenderTargetViews = 64;
	static const int kMaxNumShaderResourceViews = 90240;
	static const int kMaxNumDepthStencilViews = 64;


	static constexpr uint kMaxRootVsConstantBufferViews = 4;
	static constexpr uint kMaxRootVsShaderResourceViews = 1;
	static constexpr uint kMaxRootPsShaderResourceViews = 19;
	static constexpr uint kMaxRootPsSamplers = 16;
	static constexpr uint kMaxRootPsConstantBufferViews = 4;

	static constexpr uint kRootVsConstantBufferTable = 0;
	static constexpr uint kRootVsShaderResourceViewTable = 1;
	static constexpr uint kRootPsShaderResourceViewTable = 2;
	static constexpr uint kRootPsSamplerTable = 3;
	static constexpr uint kRootPsConstantBufferTable = 4;

	static constexpr uint kRootCsConstantBufferTable = 0;
	static constexpr uint kRootCsShaderResourceViewTable = 1;
	static constexpr uint kRootCsSamplerTable = 2;
	static constexpr uint kRootCsUnorderedAccessViewTable = 3;

	bool resourceFormatIsDepth(const RdrResourceFormat eFormat)
	{
		return eFormat == RdrResourceFormat::D16 || eFormat == RdrResourceFormat::D24_UNORM_S8_UINT;
	}

	bool resourceFormatIsCompressed(const RdrResourceFormat eFormat)
	{
		return eFormat == RdrResourceFormat::DXT5 || eFormat == RdrResourceFormat::DXT1;
	}

	D3D12_PRIMITIVE_TOPOLOGY getD3DTopology(const RdrTopology eTopology)
	{
		static const D3D12_PRIMITIVE_TOPOLOGY s_d3dTopology[] = {
			D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,					// RdrTopology::Undefined
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,				// RdrTopology::TriangleList
			D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,				// RdrTopology::TriangleStrip
			D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,	// RdrTopology::Quad
		};
		static_assert(ARRAY_SIZE(s_d3dTopology) == (int)RdrTopology::Count, "Missing D3D topologies!");
		return s_d3dTopology[(int)eTopology];
	}

	DXGI_FORMAT getD3DFormat(const RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dFormats[] = {
			DXGI_FORMAT_UNKNOWN,				// ResourceFormat::UNKNOWN
			DXGI_FORMAT_R16_UNORM,				// ResourceFormat::D16
			DXGI_FORMAT_R24_UNORM_X8_TYPELESS,	// ResourceFormat::D24_UNORM_S8_UINT
			DXGI_FORMAT_R16_UNORM,				// ResourceFormat::R16_UNORM
			DXGI_FORMAT_R16_UINT,				// ResourceFormat::R16_UINT
			DXGI_FORMAT_R16_FLOAT,				// ResourceFormat::R16_FLOAT
			DXGI_FORMAT_R32_UINT,				// ResourceFormat::R32_UINT
			DXGI_FORMAT_R16G16_FLOAT,			// ResourceFormat::R16G16_FLOAT
			DXGI_FORMAT_R8_UNORM,				// ResourceFormat::R8_UNORM
			DXGI_FORMAT_BC1_UNORM,				// ResourceFormat::DXT1
			DXGI_FORMAT_BC1_UNORM_SRGB,			// ResourceFormat::DXT1_sRGB
			DXGI_FORMAT_BC3_UNORM,				// ResourceFormat::DXT5
			DXGI_FORMAT_BC3_UNORM_SRGB,			// ResourceFormat::DXT5_sRGB
			DXGI_FORMAT_B8G8R8A8_UNORM,			// ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,	// ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_R8G8B8A8_UNORM,			// ResourceFormat::R8G8B8A8_UNORM
			DXGI_FORMAT_R8G8_UNORM,				// ResourceFormat::R8G8_UNORM
			DXGI_FORMAT_R16G16B16A16_FLOAT,		// ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAY_SIZE(s_d3dFormats) == (int)RdrResourceFormat::Count, "Missing D3D formats!");
		return s_d3dFormats[(int)format];
	}

	DXGI_FORMAT getD3DDepthFormat(const RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dDepthFormats[] = {
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::UNKNOWN
			DXGI_FORMAT_D16_UNORM,			// ResourceFormat::D16
			DXGI_FORMAT_D24_UNORM_S8_UINT,	// ResourceFormat::D24_UNORM_S8_UINT
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16_UINT
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16_FLOAT
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R32_UINT
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R16G16_FLOAT
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT1
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT1_sRGB
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT5
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::DXT5_sRGB
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8G8B8A8_UNORM
			DXGI_FORMAT_UNKNOWN,			// ResourceFormat::R8G8B8_UNORM
			DXGI_FORMAT_UNKNOWN,	        // ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAY_SIZE(s_d3dDepthFormats) == (int)RdrResourceFormat::Count, "Missing D3D depth formats!");
		assert(s_d3dDepthFormats[(int)format] != DXGI_FORMAT_UNKNOWN);
		return s_d3dDepthFormats[(int)format];
	}

	DXGI_FORMAT getD3DTypelessFormat(const RdrResourceFormat format)
	{
		static const DXGI_FORMAT s_d3dTypelessFormats[] = {
			DXGI_FORMAT_UNKNOWN,			   // ResourceFormat::UNKNOWN
			DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::D16
			DXGI_FORMAT_R24G8_TYPELESS,		   // ResourceFormat::D24_UNORM_S8_UINT
			DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::R16_UNORM
			DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::R16_UINT
			DXGI_FORMAT_R16_TYPELESS,		   // ResourceFormat::R16_FLOAT
			DXGI_FORMAT_R32_TYPELESS,          // ResourceFormat::R32_UINT
			DXGI_FORMAT_R16G16_TYPELESS,	   // ResourceFormat::R16G16_FLOAT
			DXGI_FORMAT_R8_TYPELESS,		   // ResourceFormat::R8_UNORM
			DXGI_FORMAT_BC1_TYPELESS,		   // ResourceFormat::DXT1
			DXGI_FORMAT_BC1_TYPELESS,		   // ResourceFormat::DXT1_sRGB
			DXGI_FORMAT_BC3_TYPELESS,		   // ResourceFormat::DXT5
			DXGI_FORMAT_BC3_TYPELESS,		   // ResourceFormat::DXT5_sRGB
			DXGI_FORMAT_B8G8R8A8_TYPELESS,	   // ResourceFormat::B8G8R8A8_UNORM
			DXGI_FORMAT_B8G8R8A8_TYPELESS,     // ResourceFormat::B8G8R8A8_UNORM_sRGB
			DXGI_FORMAT_R8G8B8A8_TYPELESS,	   // ResourceFormat::R8G8B8A8_UNORM
			DXGI_FORMAT_R8G8_TYPELESS,	       // ResourceFormat::R8G8_UNORM
			DXGI_FORMAT_R16G16B16A16_TYPELESS, // ResourceFormat::R16G16B16A16_FLOAT
		};
		static_assert(ARRAY_SIZE(s_d3dTypelessFormats) == (int)RdrResourceFormat::Count, "Missing typeless formats!");
		return s_d3dTypelessFormats[(int)format];
	}

	CD3DX12_HEAP_PROPERTIES selectHeapProperties(const RdrResourceAccessFlags accessFlags)
	{
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::CpuWrite))
			return CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		else if (accessFlags == RdrResourceAccessFlags::CpuRO_GpuRW)
			return CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

		return CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	}

	void createBufferSrv(ComPtr<ID3D12Device> pDevice, DescriptorHeap& srvHeap, ID3D12Resource* pResource, uint numElements, uint structureByteStride, RdrResourceFormat eFormat, int firstElement, D3D12DescriptorHandle* pOutView)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		desc.Buffer.FirstElement = firstElement;
		desc.Buffer.NumElements = numElements;
		desc.Buffer.StructureByteStride = structureByteStride;
		desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
		desc.Format = getD3DFormat(eFormat);
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(pResource, &desc, descHandle);

		*pOutView = descHandle;
	}
}

bool RdrContext::CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
{
	D3D12_RESOURCE_STATES eInitialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	uint nDataSize = numElements * rdrGetTexturePitch(1, eFormat);
	ID3D12Resource* pResource = CreateBuffer(nDataSize, accessFlags, eInitialState);

	RdrShaderResourceView srv;
	RdrShaderResourceView* pSRV = nullptr;
	if (IsFlagSet(rResource.GetAccessFlags(), RdrResourceAccessFlags::GpuRead))
	{
		pSRV = &srv;
		createBufferSrv(m_pDevice, m_srvHeap, pResource, numElements, 0, eFormat, 0, &srv.hView);
	}

	RdrUnorderedAccessView uav;
	RdrUnorderedAccessView* pUAV = nullptr;
	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
	{
		pUAV = &uav;

		D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
		desc.Buffer.FirstElement = 0;
		desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		desc.Buffer.NumElements = numElements;
		desc.Buffer.StructureByteStride = 0;
		desc.Buffer.CounterOffsetInBytes = 0;
		desc.Format = getD3DFormat(eFormat);
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		uav.hView = m_srvHeap.AllocateDescriptor();
		m_pDevice->CreateUnorderedAccessView(pResource, nullptr, &desc, uav.hView);
	}

	rResource.InitAsConstantBuffer(accessFlags, nDataSize);
	rResource.BindDeviceResources(pResource, eInitialState, pSRV, pUAV);

	if (pSrcData)
	{
		UpdateResource(rResource, pSrcData, nDataSize);
	}

	return true;
}

bool RdrContext::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
{
	D3D12_RESOURCE_STATES eInitialState = D3D12_RESOURCE_STATE_GENERIC_READ;

	uint nDataSize = numElements * elementSize;
	ID3D12Resource* pResource = CreateBuffer(nDataSize, accessFlags, eInitialState);


	RdrShaderResourceView srv;
	RdrShaderResourceView* pSRV = nullptr;
	if (IsFlagSet(rResource.GetAccessFlags(), RdrResourceAccessFlags::GpuRead))
	{
		pSRV = &srv;
		createBufferSrv(m_pDevice, m_srvHeap, pResource, numElements, elementSize, RdrResourceFormat::UNKNOWN, 0, &srv.hView);
	}

	RdrUnorderedAccessView uav;
	RdrUnorderedAccessView* pUAV = nullptr;
	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
	{
		pUAV = &uav;

		D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
		desc.Buffer.FirstElement = 0;
		desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		desc.Buffer.NumElements = numElements;
		desc.Buffer.StructureByteStride = elementSize;
		desc.Buffer.CounterOffsetInBytes = 0;
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		desc.Format = DXGI_FORMAT_UNKNOWN;

		uav.hView = m_srvHeap.AllocateDescriptor();
		m_pDevice->CreateUnorderedAccessView(pResource, nullptr, &desc, uav.hView);
	}

	rResource.InitAsConstantBuffer(accessFlags, nDataSize);
	rResource.BindDeviceResources(pResource, eInitialState, pSRV, pUAV);

	if (pSrcData)
	{
		UpdateResource(rResource, pSrcData, nDataSize);
	}

	return true;
}

void RdrContext::CopyResourceRegion(const RdrResource& rSrcResource, const RdrBox& srcRegion, const RdrResource& rDstResource, const IVec3& dstOffset)
{
	D3D12_BOX box;
	box.left = srcRegion.left;
	box.right = srcRegion.left + srcRegion.width;
	box.top = srcRegion.top;
	box.bottom = srcRegion.top + srcRegion.height;
	box.front = srcRegion.front;
	box.back = srcRegion.front + srcRegion.depth;

	assert(false);
	//donotcheckin 	m_pDevContext->CopySubresourceRegion(rDstResource.pResource, 0, dstOffset.x, dstOffset.y, dstOffset.z, rSrcResource.pResource, 0, &box);
}

void RdrContext::ReadResource(const RdrResource& rSrcResource, void* pDstData, uint dstDataSize)
{
	ID3D12Resource* pDeviceResource = rSrcResource.GetResource();

	void* pMappedData;
	D3D12_RANGE readRange = { 0, dstDataSize };
	HRESULT hr = pDeviceResource->Map(0, &readRange, &pMappedData);
	if (FAILED(hr))
	{
		assert(false);
		return;
	}

	memcpy(pDstData, pMappedData, dstDataSize);

	D3D12_RANGE writeRange = { 0,0 };
	pDeviceResource->Unmap(0, nullptr);
}

ID3D12Resource* RdrContext::CreateBuffer(const int size, RdrResourceAccessFlags accessFlags, const D3D12_RESOURCE_STATES initialState)
{
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = size;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	CD3DX12_HEAP_PROPERTIES heapProperties = selectHeapProperties(accessFlags);
	ID3D12Resource* pResource = nullptr;

	HRESULT hr = m_pDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&pResource)); //donotcheckin - manage ref count?

	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	return pResource;
}

bool RdrContext::CreateVertexBuffer(const void* vertices, int size, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
{
	D3D12_RESOURCE_STATES eInitialState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	ID3D12Resource* pResource = CreateBuffer(size, accessFlags, eInitialState);
	if (!pResource)
		return false;

	rResource.InitAsVertexBuffer(accessFlags, size);
	rResource.BindDeviceResources(pResource, eInitialState, nullptr, nullptr);

	if (vertices)
	{
		UpdateResource(rResource, vertices, size);
	}

	return true;
}

bool RdrContext::CreateIndexBuffer(const void* indices, int size, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
{
	D3D12_RESOURCE_STATES eInitialState = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	ID3D12Resource* pResource = CreateBuffer(size, accessFlags, eInitialState);
	if (!pResource)
		return false;

	rResource.InitAsIndexBuffer(accessFlags, size);
	rResource.BindDeviceResources(pResource, eInitialState, nullptr, nullptr);

	if (indices)
	{
		UpdateResource(rResource, indices, size);
	}

	return true;
}

static D3D12_COMPARISON_FUNC getComparisonFuncD3d(const RdrComparisonFunc cmpFunc)
{
	D3D12_COMPARISON_FUNC cmpFuncD3d[] = {
		D3D12_COMPARISON_FUNC_NEVER,
		D3D12_COMPARISON_FUNC_LESS,
		D3D12_COMPARISON_FUNC_EQUAL,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_COMPARISON_FUNC_GREATER,
		D3D12_COMPARISON_FUNC_NOT_EQUAL,
		D3D12_COMPARISON_FUNC_GREATER_EQUAL,
		D3D12_COMPARISON_FUNC_ALWAYS
	};
	static_assert(ARRAY_SIZE(cmpFuncD3d) == (int)RdrComparisonFunc::Count, "Missing comparison func");
	return cmpFuncD3d[(int)cmpFunc];
}

static D3D12_TEXTURE_ADDRESS_MODE getTexCoordModeD3d(const RdrTexCoordMode uvMode)
{
	D3D12_TEXTURE_ADDRESS_MODE uvModeD3d[] = {
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_MIRROR
	};
	static_assert(ARRAY_SIZE(uvModeD3d) == (int)RdrTexCoordMode::Count, "Missing tex coord mode");
	return uvModeD3d[(int)uvMode];
}

static D3D12_FILTER getFilterD3d(const RdrComparisonFunc cmpFunc, const bool bPointSample)
{
	if (cmpFunc != RdrComparisonFunc::Never)
	{
		return bPointSample ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	}
	else
	{
		return bPointSample ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	}
}

RdrContext::RdrContext(RdrProfiler& rProfiler)
	: m_rProfiler(rProfiler)
{

}

ComPtr<IDXGIAdapter4> GetAdapter()
{
	ComPtr<IDXGIFactory4> dxgiFactory;
	UINT createFactoryFlags = 0;
#if defined(_DEBUG)
	createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	ComPtr<IDXGIAdapter1> dxgiAdapter1;
	ComPtr<IDXGIAdapter4> dxgiAdapter4;

	SIZE_T maxDedicatedVideoMemory = 0;
	for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
		dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

		// Check to see if the adapter can create a D3D12 device without actually 
		// creating it. The adapter with the largest dedicated video memory
		// is favored.
		if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
			SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
				D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)) &&
			dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
		{
			maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
			hr = dxgiAdapter1.As(&dxgiAdapter4);
			if (FAILED(hr))
			{
				assert(false);
				return nullptr;
			}
		}
	}

	if (!dxgiAdapter4)
	{
		assert(false);
		return nullptr;
	}
	return dxgiAdapter4;
}

ComPtr<ID3D12Device> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device> d3d12Device1;
	HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d12Device1));
	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	return d3d12Device1;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue));
	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	return d3d12CommandQueue;
}

bool CheckTearingSupport()
{
	BOOL allowTearing = FALSE;

	// Rather than create the DXGI 1.5 factory interface directly, we create the
	// DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
	// graphics debugging tools which will not support the 1.5 factory interface 
	// until a future update.
	ComPtr<IDXGIFactory4> factory4;
	if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
	{
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(factory4.As(&factory5)))
		{
			if (FAILED(factory5->CheckFeatureSupport(
				DXGI_FEATURE_PRESENT_ALLOW_TEARING,
				&allowTearing, sizeof(allowTearing))))
			{
				allowTearing = FALSE;
			}
		}
	}

	return allowTearing == TRUE;
}

ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd,
	ComPtr<ID3D12CommandQueue> pCommandQueue,
	uint32_t width, uint32_t height, uint32_t bufferCount)
{
	ComPtr<IDXGISwapChain4> dxgiSwapChain4;
	ComPtr<IDXGIFactory4> dxgiFactory4;
	UINT createFactoryFlags = g_userConfig.debugDevice ? DXGI_CREATE_FACTORY_DEBUG : 0;

	HRESULT hr = CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4));
	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc = { 1, 0 };
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// It is recommended to always allow tearing if tearing support is available.
	swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain1;
	hr = dxgiFactory4->CreateSwapChainForHwnd(
		pCommandQueue.Get(),
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1);

	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	hr = dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	hr = swapChain1.As(&dxgiSwapChain4);
	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	return dxgiSwapChain4;
}

void DescriptorHeap::Create(ComPtr<ID3D12Device> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint nMaxDescriptors, bool bShaderVisible)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = nMaxDescriptors;
	desc.Type = type;
	desc.NodeMask = 0;
	desc.Flags = (bShaderVisible && (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER))
		? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap));
	if (FAILED(hr))
	{
		assert(false);
	}

	m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(type);

	m_idSet.init(nMaxDescriptors);
}

uint DescriptorHeap::AllocateDescriptorId()
{
	return m_idSet.allocId();
}

D3D12DescriptorHandle DescriptorHeap::AllocateDescriptor()
{
	uint nId = AllocateDescriptorId();
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart()).Offset(nId, m_descriptorSize);
}

void DescriptorHeap::FreeDescriptorById(uint nId)
{
	return m_idSet.releaseId(nId);
}

void DescriptorHeap::FreeDescriptor(D3D12DescriptorHandle hDesc)
{
	uint64 nId = (hDesc.ptr - m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) / m_descriptorSize;
	m_idSet.releaseId((uint)nId);
}

void DescriptorHeap::GetDescriptorHandle(uint nId, CD3DX12_CPU_DESCRIPTOR_HANDLE* pOutDesc)
{
	*pOutDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart()).Offset(nId, m_descriptorSize);
}

void DescriptorRingBuffer::Create(ComPtr<ID3D12Device> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint nMaxDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = nMaxDescriptors;
	desc.Type = type;
	desc.NodeMask = 0;
	desc.Flags = (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap));
	if (FAILED(hr))
	{
		assert(false);
	}

	m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(type);
	m_nextDescriptor = 0;
	m_maxDescriptors = nMaxDescriptors;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorRingBuffer::AllocateDescriptors(uint numToAllocate, uint& nOutDescriptorStartIndex)
{
	if (m_nextDescriptor + numToAllocate >= m_maxDescriptors)
	{
		// donotcheckin - this doesn't stop re-use in the same set of frames...
		m_nextDescriptor = 0;
	}
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	hDescriptor.Offset(m_nextDescriptor, m_descriptorSize);
	nOutDescriptorStartIndex = m_nextDescriptor;
	m_nextDescriptor += numToAllocate;
	return hDescriptor;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorRingBuffer::GetGpuHandle(uint nDescriptor) const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), nDescriptor, m_descriptorSize);
}

void QueryHeap::Create(ComPtr<ID3D12Device> pDevice, D3D12_QUERY_HEAP_TYPE type, uint nMaxQueries)
{
	D3D12_QUERY_HEAP_DESC desc = {};
	desc.Count = nMaxQueries;
	desc.Type = type;

	HRESULT hr = pDevice->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_pQueryHeap));
	if (FAILED(hr))
	{
		assert(false);
	}

	m_idSet.init(nMaxQueries);
	m_type = type;
}

D3D12QueryHandle QueryHeap::AllocateQuery()
{
	D3D12QueryHandle handle;
	handle.nType = m_type;
	handle.nId = m_idSet.allocId();
	return handle;
}

void QueryHeap::FreeQuery(D3D12QueryHandle hQuery)
{
	m_idSet.releaseId(hQuery.nId);
}


ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device> device,
	D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	HRESULT hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator));
	if(FAILED(hr))
	{
		assert(false);
		return false;
	}

	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device> device,
	ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;
	HRESULT hr = device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	hr = commandList->Close();
	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	return commandList;
}

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device> device)
{
	ComPtr<ID3D12Fence> fence;

	HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	return fence;
}

HANDLE CreateEventHandle()
{
	HANDLE fenceEvent;

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent && "Failed to create fence event.");

	return fenceEvent;
}

void Signal(ComPtr<ID3D12CommandQueue> pCommandQueue, ComPtr<ID3D12Fence> pFence, uint64 fenceValue)
{
	HRESULT hr = pCommandQueue->Signal(pFence.Get(), fenceValue);
	if (FAILED(hr))
	{
		assert(false);
	}
}

void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent)
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		HRESULT hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
		if (FAILED(hr))
		{
			assert(false);
			return;
		}

		::WaitForSingleObject(fenceEvent, INFINITE);
	}
}

void Flush(ComPtr<ID3D12CommandQueue> pCommandQueue, ComPtr<ID3D12Fence> pFence,
	uint64 fenceValue, HANDLE hFenceEvent)
{
	Signal(pCommandQueue, pFence, fenceValue);
	WaitForFenceValue(pFence, fenceValue, hFenceEvent);
}

void RdrContext::UpdateRenderTargetViews()
{
	for (int i = 0; i < kNumBackBuffers; ++i)
	{
		ComPtr<ID3D12Resource> backBuffer;
		HRESULT hr = m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));
		if (FAILED(hr))
		{
			assert(false);
			return;
		}

		if (m_hBackBufferRtvs[i].ptr == 0)
		{
			m_hBackBufferRtvs[i] = m_rtvHeap.AllocateDescriptor();
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = m_hBackBufferRtvs[i];
		m_pDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, descHandle);
		m_pBackBuffers[i] = backBuffer;
	}
}

bool RdrContext::Init(HWND hWnd, uint width, uint height)
{
	if (g_userConfig.debugDevice)
	{
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&m_pDebug))))
		{
			m_pDebug->EnableDebugLayer();
		}
	}

	m_slopeScaledDepthBias = 3.f;

	ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter();
	
	m_pDevice = CreateDevice(dxgiAdapter4);
	
	m_pCommandQueue = CreateCommandQueue(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
	m_pCommandQueue->GetTimestampFrequency(&m_commandQueueTimestampFreqency);

	m_pSwapChain = CreateSwapChain(hWnd, m_pCommandQueue, width, height, kNumBackBuffers);
	m_currBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();

	m_rtvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kMaxNumRenderTargetViews, true);
	m_srvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxNumShaderResourceViews, false);
	m_samplerHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, kMaxNumSamplers, false);
	m_dsvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kMaxNumDepthStencilViews, false);

	m_dynamicDescriptorHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024 * 16);
	m_dynamicSamplerDescriptorHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1024);

	for (int i = 0; i < kNumBackBuffers; ++i)
	{
		m_pCommandAllocators[i] = CreateCommandAllocator(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

		static constexpr uint kUploadBufferSize = 32 * 1024 * 1024; // 32 MB per frame - donotcheckin - define elsewhere
		CD3DX12_HEAP_PROPERTIES heapProperties = selectHeapProperties(RdrResourceAccessFlags::CpuRW_GpuRO);
		HRESULT hr = m_pDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(kUploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_uploadBuffers[i].pBuffer));

		if (FAILED(hr))
		{
			assert(false);//donotcheckin
		}

		CD3DX12_RANGE readRange(0, 0);
		m_uploadBuffers[i].pBuffer->Map(0, &readRange, (void**)&m_uploadBuffers[i].pStart);
		m_uploadBuffers[i].pCurr = m_uploadBuffers[i].pStart;
		m_uploadBuffers[i].pEnd = m_uploadBuffers[i].pCurr + kUploadBufferSize;
	}

	m_pCommandList = CreateCommandList(m_pDevice, m_pCommandAllocators[m_currBackBuffer], D3D12_COMMAND_LIST_TYPE_DIRECT);

	m_pFence = CreateFence(m_pDevice);
	m_hFenceEvent = CreateEventHandle();
	m_nFrameNum = kNumBackBuffers;

	// Query heaps
	const uint kMaxTimestampQueries = 512;
	m_timestampQueryHeap.Create(m_pDevice, D3D12_QUERY_HEAP_TYPE_TIMESTAMP, kMaxTimestampQueries);

	HRESULT hr = m_pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(kMaxTimestampQueries * sizeof(UINT64)),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_timestampResultBuffer));

	if (FAILED(hr))
	{
		assert(false);//donotcheckin
	}

	// Debug settings
	if (g_userConfig.debugDevice)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
		else
		{
			OutputDebugStringA("WARNING: Direct3D Debug Device is not available\n");
		}

		ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
		{
			//HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_dxgiFactory)));
			//dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
			dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
			dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
		}

#if 0
		ComPtr<ID3D12InfoQueue> pInfoQueue;
		if (SUCCEEDED(m_pDevice.As(&pInfoQueue)))
		{
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);

			// Suppress whole categories of messages
			//D3D12_MESSAGE_CATEGORY Categories[] = {};

			// Suppress messages based on their severity level
			D3D12_MESSAGE_SEVERITY Severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO
			};

			// Suppress individual messages by their ID
			D3D12_MESSAGE_ID DenyIds[] = {
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
			};


			D3D12_INFO_QUEUE_FILTER filter = {};
			HRESULT hr = pInfoQueue->AddStorageFilterEntries(&filter);

			/*
			D3D12_INFO_QUEUE_FILTER NewFilter = {};
			//NewFilter.DenyList.NumCategories = _countof(Categories);
			//NewFilter.DenyList.pCategoryList = Categories;
			NewFilter.DenyList.NumSeverities = _countof(Severities);
			NewFilter.DenyList.pSeverityList = Severities;
			NewFilter.DenyList.NumIDs = _countof(DenyIds);
			NewFilter.DenyList.pIDList = DenyIds;

			HRESULT hr = pInfoQueue->PushStorageFilter(&NewFilter);
			if (FAILED(hr))
			{
				assert(false);
				return nullptr;
			}
			*/
		}
#endif
	}


	{
#if 0 // donotcheckin - the way it should be done (probably moved to per-material type signature)
		CD3DX12_DESCRIPTOR_RANGE ranges[10];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // VS per-action constants
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // VS per-object constants
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // VS instancing buffer

		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0); // PS per-material resources
		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 3, 0); // PS per-material samplers
		ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // PS per-action constants
		ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // PS per-material constants
		ranges[7].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 2); // PS lighting constants
		ranges[8].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 14); // PS global samplers
		ranges[9].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 11); // PS global resources

		CD3DX12_ROOT_PARAMETER rootParameters[10];
		for (int i = 0; i < 3; ++i)
			rootParameters[i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_VERTEX);
		for (int i = 0; i < 7; ++i)
			rootParameters[i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL);

		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
#else
		CD3DX12_DESCRIPTOR_RANGE ranges[5];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 4, 0); // VS constants
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // VS buffers
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 19, 0); // PS resources
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0); // PS samplers
		ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 4, 0); // PS constants

		CD3DX12_ROOT_PARAMETER rootParameters[5];
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[3].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[4].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_PIXEL);

		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
#endif
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);


		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		if (FAILED(hr))
		{
			const char* pErrorMsg = (const char*)error->GetBufferPointer();
			Error(pErrorMsg);
			return nullptr;
		}

		hr = m_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pGraphicsRootSignature));
		if (FAILED(hr))
		{
			assert(false);
			return nullptr;
		}
	}

	// Compute root signature
	{
		CD3DX12_DESCRIPTOR_RANGE ranges[4];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 4, 0); // CS constants
		ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 0); // CS resources
		ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 4, 0); // CS samplers
		ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0); // CS UAVs

		CD3DX12_ROOT_PARAMETER rootParameters[4];
		rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_ALL);
		rootParameters[3].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_ALL);

		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE; //DONOTCHECKIN - deny everything?
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		if (FAILED(hr))
		{
			const char* pErrorMsg = (const char*)error->GetBufferPointer();
			Error(pErrorMsg);
			return nullptr;
		}

		hr = m_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pComputeRootSignature));
		if (FAILED(hr))
		{
			assert(false);
			return nullptr;
		}
	}

	// Null resources
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		m_nullRenderTargetView.hView = m_rtvHeap.AllocateDescriptor();
		m_nullRenderTargetView.pResource = nullptr;
		m_pDevice->CreateRenderTargetView(nullptr, &rtvDesc, m_nullRenderTargetView.hView);


		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		m_nullDepthStencilView.hView = m_dsvHeap.AllocateDescriptor();
		m_nullDepthStencilView.pResource = nullptr;
		m_pDevice->CreateDepthStencilView(nullptr, &dsvDesc, m_nullDepthStencilView.hView);
	}

	Resize(width, height);
	m_drawState.Reset();
	return true;
}

void RdrContext::Release()
{
	//donotcheckin
	m_drawState.Reset();
	m_pSwapChain->Release();
	m_pDevice->Release();
}

bool RdrContext::IsIdle()
{
	return (m_presentFlags & DXGI_PRESENT_TEST);
}

D3D12DescriptorHandle RdrContext::GetSampler(const RdrSamplerState& state)
{
	uint samplerIndex =
		state.cmpFunc * ((uint)RdrTexCoordMode::Count * 2)
		+ state.texcoordMode * 2
		+ state.bPointSample;

	if (m_samplers[samplerIndex].ptr == 0)
	{
		D3D12_SAMPLER_DESC desc;
		desc.MaxAnisotropy = 1;
		desc.MaxLOD = FLT_MAX;
		desc.MinLOD = -FLT_MIN;
		desc.MipLODBias = 0;
		desc.BorderColor[0] = 1.f;
		desc.BorderColor[1] = 1.f;
		desc.BorderColor[2] = 1.f;
		desc.BorderColor[3] = 1.f;

		desc.AddressU = desc.AddressV = desc.AddressW = getTexCoordModeD3d((RdrTexCoordMode)state.texcoordMode);
		desc.Filter = getFilterD3d((RdrComparisonFunc)state.cmpFunc, state.bPointSample);
		desc.ComparisonFunc = getComparisonFuncD3d((RdrComparisonFunc)state.cmpFunc);

		m_samplers[samplerIndex] = m_samplerHeap.AllocateDescriptor();
		m_pDevice->CreateSampler(&desc, m_samplers[samplerIndex]);
	}

	return m_samplers[samplerIndex];
}

namespace
{
	bool createTextureCubeSrv(ComPtr<ID3D12Device> pDevice, DescriptorHeap& srvHeap, const RdrTextureInfo& rTexInfo, ID3D12Resource* pResource, D3D12DescriptorHandle* pOutView)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Format = getD3DFormat(rTexInfo.format);

		if (rTexInfo.depth > 1)
		{
			viewDesc.TextureCubeArray.First2DArrayFace = 0;
			viewDesc.TextureCubeArray.MipLevels = rTexInfo.mipLevels;
			viewDesc.TextureCubeArray.MostDetailedMip = 0;
			viewDesc.TextureCubeArray.NumCubes = rTexInfo.depth;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		}
		else
		{
			viewDesc.TextureCube.MipLevels = rTexInfo.mipLevels;
			viewDesc.TextureCube.MostDetailedMip = 0;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(pResource, &viewDesc, descHandle);

		*pOutView = descHandle;
		return true;
	}

	bool createTextureCube(ComPtr<ID3D12Device> pDevice, DescriptorHeap& srvHeap, D3D12_RESOURCE_STATES eInitialState, const RdrTextureInfo& rTexInfo, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
	{
		const uint cubemapArraySize = rTexInfo.depth * 6;
		D3D12_RESOURCE_DESC desc;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = 0;
		desc.Width = rTexInfo.width;
		desc.Height = rTexInfo.height;
		desc.DepthOrArraySize = cubemapArraySize;
		desc.MipLevels = rTexInfo.mipLevels;
		desc.Format = getD3DTypelessFormat(rTexInfo.format);
		desc.SampleDesc.Count = rTexInfo.sampleCount;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuRenderTarget))
		{
			desc.Flags |= resourceFormatIsDepth(rTexInfo.format) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}

		D3D12_HEAP_PROPERTIES heapProps = selectHeapProperties(accessFlags);

		ID3D12Resource* pResource;
		HRESULT hr = pDevice->CreateCommittedResource(
			&heapProps, 
			D3D12_HEAP_FLAG_NONE,
			&desc, 
			eInitialState,
			nullptr,
			IID_PPV_ARGS(&pResource)); //donotcheckin - manage ref count?

		if (FAILED(hr))
		{
			assert(false);
			return false;
		}

		RdrShaderResourceView srv;
		createTextureCubeSrv(pDevice, srvHeap, rTexInfo, pResource, &srv.hView);

		rResource.BindDeviceResources(pResource, eInitialState, &srv, nullptr);
		return true;
	}

	bool createTexture2DSrv(ComPtr<ID3D12Device> pDevice, DescriptorHeap& srvHeap, const RdrTextureInfo& rTexInfo, ID3D12Resource* pResource, D3D12DescriptorHandle* pOutView)
	{
		bool bIsMultisampled = (rTexInfo.sampleCount > 1);
		bool bIsArray = (rTexInfo.depth > 1);

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Format = getD3DFormat(rTexInfo.format);
		if (bIsArray)
		{
			if (bIsMultisampled)
			{
				viewDesc.Texture2DMSArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DMSArray.FirstArraySlice = 0;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
			}
			else
			{
				viewDesc.Texture2DArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DArray.FirstArraySlice = 0;
				viewDesc.Texture2DArray.MipLevels = rTexInfo.mipLevels;
				viewDesc.Texture2DArray.MostDetailedMip = 0;
				viewDesc.Texture2DArray.PlaneSlice = 0;
				viewDesc.Texture2DArray.ResourceMinLODClamp = 0.f;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			}
		}
		else
		{
			if (bIsMultisampled)
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
				viewDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
			}
			else
			{
				viewDesc.Texture2D.MipLevels = rTexInfo.mipLevels;
				viewDesc.Texture2D.MostDetailedMip = 0;
				viewDesc.Texture2D.PlaneSlice = 0;
				viewDesc.Texture2D.ResourceMinLODClamp = 0.f;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			}
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(pResource, &viewDesc, descHandle);

		*pOutView = descHandle;
		return true;
	}

	bool createTexture2D(ComPtr<ID3D12Device> pDevice, DescriptorHeap& srvHeap, D3D12_RESOURCE_STATES eInitialState, const RdrTextureInfo& rTexInfo, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
	{
		bool bIsMultisampled = (rTexInfo.sampleCount > 1);
		bool bIsArray = (rTexInfo.depth > 1);

		D3D12_RESOURCE_DESC desc;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = 0;
		desc.Width = rTexInfo.width;
		desc.Height = rTexInfo.height;
		desc.DepthOrArraySize = rTexInfo.depth;
		desc.MipLevels = rTexInfo.mipLevels;
		desc.Format = getD3DTypelessFormat(rTexInfo.format);
		desc.SampleDesc.Count = rTexInfo.sampleCount;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_CLEAR_VALUE clearValue;
		D3D12_CLEAR_VALUE* pClearValue = nullptr;
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuRenderTarget))
		{
			if (resourceFormatIsDepth(rTexInfo.format))
			{
				pClearValue = &clearValue;
				clearValue.Format = getD3DDepthFormat(rTexInfo.format);
				clearValue.DepthStencil.Depth = 1.f;
				clearValue.DepthStencil.Stencil = 0;

				desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			}
			else
			{
				pClearValue = &clearValue;
				clearValue.Format = getD3DFormat(rTexInfo.format);
				clearValue.Color[0] = 0.f;
				clearValue.Color[1] = 0.f;
				clearValue.Color[2] = 0.f;
				clearValue.Color[3] = 0.f;

				desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			}
		}
		else if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		D3D12_HEAP_PROPERTIES heapProps = selectHeapProperties(accessFlags);

		ID3D12Resource* pResource;
		HRESULT hr = pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			eInitialState,
			pClearValue,
			IID_PPV_ARGS(&pResource)); //donotcheckin - manage ref count?

		if (FAILED(hr))
		{
			assert(false);
			return false;
		}

		RdrShaderResourceView srv;
		RdrShaderResourceView* pSRV = nullptr;
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuRead))
		{
			pSRV = &srv;
			createTexture2DSrv(pDevice, srvHeap, rTexInfo, pResource, &srv.hView);
		}

		RdrUnorderedAccessView uav;
		RdrUnorderedAccessView* pUAV = nullptr;
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
		{
			pUAV = &uav;

			D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);
			if (bIsArray)
			{
				viewDesc.Texture2DArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DArray.FirstArraySlice = 0;
				viewDesc.Texture2DArray.MipSlice = 0;
				viewDesc.Texture2DArray.PlaneSlice = 0;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			}
			else
			{
				viewDesc.Texture2D.MipSlice = 0;
				viewDesc.Texture2D.PlaneSlice = 0;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			}

			CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = srvHeap.AllocateDescriptor();
			pDevice->CreateUnorderedAccessView(pResource, nullptr, &viewDesc, descHandle);
			uav.hView = descHandle;

			assert(hr == S_OK);
		}

		rResource.BindDeviceResources(pResource, eInitialState, pSRV, pUAV);

		return true;
	}

	bool createTexture3DSrv(ComPtr<ID3D12Device> pDevice, DescriptorHeap& srvHeap, const RdrTextureInfo& rTexInfo, ID3D12Resource* pResource, D3D12DescriptorHandle* pOutView)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		viewDesc.Format = getD3DFormat(rTexInfo.format);
		viewDesc.Texture3D.MipLevels = rTexInfo.mipLevels;
		viewDesc.Texture3D.MostDetailedMip = 0;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(pResource, &viewDesc, descHandle);
		*pOutView = descHandle;

		return true;
	}

	bool createTexture3D(ComPtr<ID3D12Device> pDevice, DescriptorHeap& srvHeap, D3D12_RESOURCE_STATES eInitialState, const RdrTextureInfo& rTexInfo, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
	{
		D3D12_RESOURCE_DESC desc;
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		desc.Alignment = 0;
		desc.Width = rTexInfo.width;
		desc.Height = rTexInfo.height;
		desc.DepthOrArraySize = rTexInfo.depth;
		desc.MipLevels = rTexInfo.mipLevels;
		desc.Format = getD3DTypelessFormat(rTexInfo.format);
		desc.SampleDesc.Count = rTexInfo.sampleCount;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		desc.Flags = D3D12_RESOURCE_FLAG_NONE;
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuRenderTarget))
		{
			desc.Flags |= resourceFormatIsDepth(rTexInfo.format)
				? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
				: D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		}
		else if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
		{
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}

		D3D12_HEAP_PROPERTIES heapProps = selectHeapProperties(accessFlags);

		ID3D12Resource* pResource = nullptr;
		HRESULT hr = pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			eInitialState,
			nullptr,
			IID_PPV_ARGS(&pResource)); //donotcheckin - manage ref count?

		if (FAILED(hr))
		{
			assert(false);
			return false;
		}

		RdrShaderResourceView srv;
		RdrShaderResourceView* pSRV = nullptr;
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuRead))
		{
			pSRV = &srv;
			createTexture3DSrv(pDevice, srvHeap, rTexInfo, pResource, &srv.hView);
			srv.pResource = &rResource;
		}

		RdrUnorderedAccessView uav;
		RdrUnorderedAccessView* pUAV = nullptr;
		if (IsFlagSet(accessFlags, RdrResourceAccessFlags::GpuWrite))
		{
			pUAV = &uav;

			D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);
			viewDesc.Texture3D.MipSlice = 0;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = rTexInfo.depth;
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;

			uav.hView = srvHeap.AllocateDescriptor();
			pDevice->CreateUnorderedAccessView(pResource, nullptr, &viewDesc, uav.hView);
		}

		rResource.BindDeviceResources(pResource, eInitialState, pSRV, pUAV);

		return true;
	}
}

bool RdrContext::CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
{
	D3D12_RESOURCE_STATES eInitialState = D3D12_RESOURCE_STATE_COMMON;

	rResource.InitAsTexture(rTexInfo, accessFlags);

	bool res = false;
	switch (rTexInfo.texType)
	{
	case RdrTextureType::k2D:
		res = createTexture2D(m_pDevice, m_srvHeap, eInitialState, rTexInfo, accessFlags, rResource);
		break;
	case RdrTextureType::kCube:
		res = createTextureCube(m_pDevice, m_srvHeap, eInitialState, rTexInfo, accessFlags, rResource);
		break;
	case RdrTextureType::k3D:
		res = createTexture3D(m_pDevice, m_srvHeap, eInitialState, rTexInfo, accessFlags, rResource);
		break;
	}

	if (pSrcData)
	{
		uint sliceCount = (rTexInfo.texType == RdrTextureType::kCube) ? rTexInfo.depth * 6 : rTexInfo.depth;
		uint numSubresources = sliceCount * rTexInfo.mipLevels;
		D3D12_SUBRESOURCE_DATA* pSubresourceData = nullptr;

		uint resIndex = 0;
		pSubresourceData = (D3D12_SUBRESOURCE_DATA*)_malloca(numSubresources * sizeof(D3D12_SUBRESOURCE_DATA));

		char* pPos = (char*)pSrcData;
		for (uint slice = 0; slice < sliceCount; ++slice)
		{
			uint mipWidth = rTexInfo.width;
			uint mipHeight = rTexInfo.height;
			for (uint mip = 0; mip < rTexInfo.mipLevels; ++mip)
			{
				int rows = rdrGetTextureRows(mipHeight, rTexInfo.format);

				pSubresourceData[resIndex].pData = pPos;
				pSubresourceData[resIndex].RowPitch = rdrGetTexturePitch(mipWidth, rTexInfo.format);
				pSubresourceData[resIndex].SlicePitch = pSubresourceData[resIndex].RowPitch * rows;

				pPos += pSubresourceData[resIndex].SlicePitch;

				if (mipWidth > 1)
					mipWidth >>= 1;
				if (mipHeight > 1)
					mipHeight >>= 1;
				++resIndex;
			}
		}

		// Donotcheckin -just make temp upload resources
		RdrUploadBuffer& uploadBuffer = m_uploadBuffers[m_currBackBuffer];
		static const uint kAlignment = 512;
		uint nSrcOffset = (uint)(uploadBuffer.pCurr - uploadBuffer.pStart);
		uint nPadding = kAlignment - (nSrcOffset % kAlignment);
		nSrcOffset += (nPadding == 512) ? 0 : nPadding;

		static constexpr uint kMaxSubresources = 16;
		assert(numSubresources < kMaxSubresources);

		UINT64 RequiredSize = 0;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[kMaxSubresources];
		UINT NumRows[kMaxSubresources];
		UINT64 RowSizesInBytes[kMaxSubresources];


		ID3D12Resource* pResource = rResource.GetResource();
		D3D12_RESOURCE_DESC Desc = pResource->GetDesc();
		m_pDevice->GetCopyableFootprints(&Desc, 0, numSubresources, nSrcOffset, Layouts, NumRows, RowSizesInBytes, &RequiredSize);

		if (uploadBuffer.pCurr + nPadding + RequiredSize <= uploadBuffer.pEnd)
		{
			uploadBuffer.pCurr += nPadding;

			TransitionResource(&rResource, D3D12_RESOURCE_STATE_COPY_DEST);

			for (uint i = 0; i < numSubresources; ++i)
			{
				if (RowSizesInBytes[i] > (SIZE_T)-1)
					assert(false);//donotcheckin

				D3D12_MEMCPY_DEST DestData = { uploadBuffer.pStart + Layouts[i].Offset, Layouts[i].Footprint.RowPitch, SIZE_T(Layouts[i].Footprint.RowPitch) * SIZE_T(NumRows[i]) };
				MemcpySubresource(&DestData, &pSubresourceData[i], (SIZE_T)RowSizesInBytes[i], NumRows[i], Layouts[i].Footprint.Depth);
			}

			for (uint i = 0; i < numSubresources; ++i)
			{
				CD3DX12_TEXTURE_COPY_LOCATION Dst(pResource, i);
				CD3DX12_TEXTURE_COPY_LOCATION Src(uploadBuffer.pBuffer.Get(), Layouts[i]);
				m_pCommandList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
			}
			
			TransitionResource(&rResource, eInitialState);

			uploadBuffer.pCurr += RequiredSize;
		}
		else
		{
			assert(false);
		}
		 
		_freea(pSubresourceData);
	}

	return res;
}

void RdrContext::ReleaseResource(RdrResource& rResource)
{
	if (rResource.GetResource())
	{
		rResource.GetResource()->Release();
	}
	if (rResource.GetSRV().hView.ptr)
	{
		m_srvHeap.FreeDescriptor(rResource.GetSRV().hView);
	}
	if (rResource.GetUAV().hView.ptr)
	{
		m_srvHeap.FreeDescriptor(rResource.GetUAV().hView);
	}

	rResource.Reset();
}

void RdrContext::ResolveResource(const RdrResource& rSrc, const RdrResource& rDst)
{
	m_pCommandList->ResolveSubresource(rDst.GetResource(), 0, rSrc.GetResource(), 0, getD3DFormat(rSrc.GetTextureInfo().format));
}

RdrShaderResourceView RdrContext::CreateShaderResourceViewTexture(RdrResource& rResource)
{
	RdrShaderResourceView view;
	switch (rResource.GetTextureInfo().texType)
	{
	case RdrTextureType::k2D:
		createTexture2DSrv(m_pDevice, m_srvHeap, rResource.GetTextureInfo(), rResource.GetResource(), &view.hView);
		break;
	case RdrTextureType::k3D:
		createTexture3DSrv(m_pDevice, m_srvHeap, rResource.GetTextureInfo(), rResource.GetResource(), &view.hView);
		break;
	case RdrTextureType::kCube:
		createTextureCubeSrv(m_pDevice, m_srvHeap, rResource.GetTextureInfo(), rResource.GetResource(), &view.hView);
		break;
	}

	view.pResource = &rResource;
	return view;
}

RdrShaderResourceView RdrContext::CreateShaderResourceViewBuffer(RdrResource& rResource, uint firstElement)
{
	RdrShaderResourceView view;

	const RdrBufferInfo& rBufferInfo = rResource.GetBufferInfo();
	createBufferSrv(m_pDevice, m_srvHeap, rResource.GetResource(), rBufferInfo.numElements - firstElement, 0, rBufferInfo.eFormat, firstElement, &view.hView);

	view.pResource = &rResource;

	return view;
}

void RdrContext::ReleaseShaderResourceView(RdrShaderResourceView& resourceView)
{
	m_srvHeap.FreeDescriptor(resourceView.hView);
	resourceView.hView.ptr = 0;
}

RdrDepthStencilView RdrContext::CreateDepthStencilView(RdrResource& rDepthTex)
{
	RdrDepthStencilView view;

	bool bIsMultisampled = (rDepthTex.GetTextureInfo().sampleCount > 1);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = getD3DDepthFormat(rDepthTex.GetTextureInfo().format);
	if (rDepthTex.GetTextureInfo().depth > 1)
	{
		if (bIsMultisampled)
		{
			dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
			dsvDesc.Texture2DMSArray.ArraySize = rDepthTex.GetTextureInfo().depth;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
		}
		else
		{
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.ArraySize = rDepthTex.GetTextureInfo().depth;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		}
	}
	else
	{
		if (bIsMultisampled)
		{
			dsvDesc.Texture2DMS.UnusedField_NothingToDefine = 0;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			dsvDesc.Texture2D.MipSlice = 0;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		}
	}

	view.hView = m_dsvHeap.AllocateDescriptor();
	view.pResource = &rDepthTex;
	m_pDevice->CreateDepthStencilView(rDepthTex.GetResource(), &dsvDesc, view.hView);

	return view;
}

RdrDepthStencilView RdrContext::CreateDepthStencilView(RdrResource& rDepthTex, uint arrayStartIndex, uint arraySize)
{
	RdrDepthStencilView view;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = getD3DDepthFormat(rDepthTex.GetTextureInfo().format);

	if (rDepthTex.GetTextureInfo().sampleCount > 1)
	{
		dsvDesc.Texture2DMSArray.FirstArraySlice = arrayStartIndex;
		dsvDesc.Texture2DMSArray.ArraySize = arraySize;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
	}
	else
	{
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = arrayStartIndex;
		dsvDesc.Texture2DArray.ArraySize = arraySize;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
	}

	view.hView = m_dsvHeap.AllocateDescriptor();
	view.pResource = &rDepthTex;
	m_pDevice->CreateDepthStencilView(rDepthTex.GetResource(), &dsvDesc, view.hView);

	return view;
}

void RdrContext::ReleaseDepthStencilView(const RdrDepthStencilView& depthStencilView)
{
	m_dsvHeap.FreeDescriptor(depthStencilView.hView);
}

RdrRenderTargetView RdrContext::CreateRenderTargetView(RdrResource& rTexRes)
{
	RdrRenderTargetView view;

	D3D12_RENDER_TARGET_VIEW_DESC desc;
	desc.Format = getD3DFormat(rTexRes.GetTextureInfo().format);
	if (rTexRes.GetTextureInfo().sampleCount > 1)
	{
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		desc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
		desc.Texture2D.PlaneSlice = 0;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = m_rtvHeap.AllocateDescriptor();
	m_pDevice->CreateRenderTargetView(rTexRes.GetResource(), &desc, descHandle);
	view.hView = descHandle;
	view.pResource = &rTexRes;

	return view;
}

RdrRenderTargetView RdrContext::CreateRenderTargetView(RdrResource& rTexArrayRes, uint arrayStartIndex, uint arraySize)
{
	RdrRenderTargetView view;

	D3D12_RENDER_TARGET_VIEW_DESC desc;
	desc.Format = getD3DFormat(rTexArrayRes.GetTextureInfo().format);
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
	desc.Texture2DArray.MipSlice = 0;
	desc.Texture2DArray.FirstArraySlice = arrayStartIndex;
	desc.Texture2DArray.ArraySize = arraySize;
	desc.Texture2DArray.PlaneSlice = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle = m_rtvHeap.AllocateDescriptor();
	m_pDevice->CreateRenderTargetView(rTexArrayRes.GetResource(), &desc, descHandle);
	view.hView = descHandle;
	view.pResource = &rTexArrayRes;

	return view;
}

void RdrContext::ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView)
{
	m_rtvHeap.FreeDescriptor(renderTargetView.hView);
}

void RdrContext::TransitionResource(RdrResource* pResource, D3D12_RESOURCE_STATES eState)
{
	if (pResource == nullptr || pResource->GetResourceState() == eState)
		return;

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		pResource->GetResource(),
		pResource->GetResourceState(), 
		eState);
	m_pCommandList->ResourceBarrier(1, &barrier);

	pResource->SetResourceState(eState);
}

void RdrContext::UAVBarrier(const RdrResource* pResource)
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(pResource->GetResource());
	m_pCommandList->ResourceBarrier(1, &barrier);
}

void RdrContext::SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE hRenderTargetViews[8] = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDepthStencilView = {};

	assert(numTargets < ARRAY_SIZE(hRenderTargetViews));

	for (uint i = 0; i < numTargets; ++i)
	{
		TransitionResource(aRenderTargets[i].pResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
		hRenderTargetViews[i] = aRenderTargets[i].hView;
	}

	hDepthStencilView = depthStencilTarget.hView;
	TransitionResource(depthStencilTarget.pResource, D3D12_RESOURCE_STATE_DEPTH_WRITE);

	m_pCommandList->OMSetRenderTargets(numTargets, hRenderTargetViews, false, &hDepthStencilView);
}

void RdrContext::BeginEvent(LPCWSTR eventName)
{
	PIXBeginEvent(m_pCommandList.Get(), 0, eventName);
}

void RdrContext::EndEvent()
{
	PIXEndEvent(m_pCommandList.Get());
}

void RdrContext::BeginFrame()
{
	// Reset command list for the next frame
	HRESULT hr = m_pCommandList->Reset(m_pCommandAllocators[m_currBackBuffer].Get(), nullptr);
	assert(SUCCEEDED(hr));
	
	SetDescriptorHeaps();

	// Transition new backbuffer back into RTV state
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_pBackBuffers[m_currBackBuffer].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommandList->ResourceBarrier(1, &barrier);
	}
}

void RdrContext::Present()
{
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_pBackBuffers[m_currBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	m_pCommandList->ResourceBarrier(1, &barrier);

	HRESULT hr = m_pCommandList->Close();
	if (FAILED(hr))
	{
		assert(false);
		return;
	}

	ID3D12CommandList* aCommandLists[1] = { m_pCommandList.Get() };
	m_pCommandQueue->ExecuteCommandLists(ARRAYSIZE(aCommandLists), aCommandLists);

	Signal(m_pCommandQueue, m_pFence, m_nFrameNum);
	++m_nFrameNum;


	uint syncInterval = g_userConfig.vsync;
	uint presentFlags = m_presentFlags; // donotcheckin   g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
	hr = m_pSwapChain->Present(syncInterval, presentFlags);
	if (hr == DXGI_STATUS_OCCLUDED)
	{
		// Window is no longer visible, most likely minimized.  
		// Set test flag so that we don't waste time presenting until the window is visible again.
		m_presentFlags |= DXGI_PRESENT_TEST;
	}
	else if (hr == S_OK)
	{
		m_presentFlags &= ~DXGI_PRESENT_TEST;
	}
	else
	{
		HRESULT hrRemovedReason = m_pDevice->GetDeviceRemovedReason();
		switch (hrRemovedReason)
		{
		case DXGI_ERROR_DEVICE_HUNG:
			Error("D3D12 device hung!");
			break;
		case DXGI_ERROR_DEVICE_REMOVED:
			Error("D3D12 device removed!");
			break;
		case DXGI_ERROR_DEVICE_RESET:
			Error("D3D12 device reset!");
			break;
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
			Error("D3D12 device internal driver error!");
			break;
		case DXGI_ERROR_INVALID_CALL:
			Error("D3D12 device invalid call!");
			break;
		default:
			Error("Unknown D3D12 device error %d!", hrRemovedReason);
			break;
		}
	}

	m_drawState.Reset();

	// Move to next back buffer
	m_currBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();
	m_uploadBuffers[m_currBackBuffer].pCurr = m_uploadBuffers[m_currBackBuffer].pStart;

	WaitForFenceValue(m_pFence, m_nFrameNum - 1, m_hFenceEvent);
}

void RdrContext::Resize(uint width, uint height)
{
	// Flush the GPU queue to make sure the swap chain's back buffers
	// are not being referenced by an in-flight command list.
	Flush(m_pCommandQueue, m_pFence, ++m_nFenceValue, m_hFenceEvent);

	for (int i = 0; i < kNumBackBuffers; ++i)
	{
		// Any references to the back buffers must be released
		// before the swap chain can be resized.
		m_pBackBuffers[i].Reset();
		//donotcheckin m_nFrameFenceValues[i] = m_nFrameFenceValues[m_currBackBuffer];
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	HRESULT hr = m_pSwapChain->GetDesc(&swapChainDesc);
	if (FAILED(hr))
	{
		assert(false);
		return;
	}

	hr = m_pSwapChain->ResizeBuffers(kNumBackBuffers, width, height,
		swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);
	if (FAILED(hr))
	{
		assert(false);
		return;
	}

	m_currBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();

	UpdateRenderTargetViews();
}

void RdrContext::ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor)
{
	TransitionResource(renderTarget.pResource, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_pCommandList->ClearRenderTargetView(renderTarget.hView, clearColor.asFloat4(), 0, nullptr);
}

void RdrContext::ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal)
{
	D3D12_CLEAR_FLAGS clearFlags = (D3D12_CLEAR_FLAGS)0;
	if (bClearDepth)
		clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
	if (bClearStencil)
		clearFlags |= D3D12_CLEAR_FLAG_STENCIL;

	TransitionResource(depthStencil.pResource, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_pCommandList->ClearDepthStencilView(depthStencil.hView, clearFlags, depthVal, stencilVal, 0, nullptr);
}

RdrRenderTargetView RdrContext::GetPrimaryRenderTarget()
{
	RdrRenderTargetView view;
	view.hView = m_hBackBufferRtvs[m_currBackBuffer];
	view.pResource = nullptr;//donotcheckin m_pBackBuffers[m_currBackBuffer];
	return view;
}

void RdrContext::SetViewport(const Rect& viewport)
{
	D3D12_VIEWPORT d3dViewport;
	d3dViewport.TopLeftX = viewport.left;
	d3dViewport.TopLeftY = viewport.top;
	d3dViewport.Width = viewport.width;
	d3dViewport.Height = viewport.height;
	d3dViewport.MinDepth = 0.f;
	d3dViewport.MaxDepth = 1.f;
	m_pCommandList->RSSetViewports(1, &d3dViewport);

	D3D12_RECT scissorRect;
	scissorRect.left	= (LONG)(viewport.left);
	scissorRect.bottom	= (LONG)(viewport.top + viewport.height);
	scissorRect.top		= (LONG)(viewport.top);
	scissorRect.right	= (LONG)(viewport.left + viewport.width);
	m_pCommandList->RSSetScissorRects(1, &scissorRect);

}

bool RdrContext::CreateConstantBuffer(const void* pData, uint size, RdrResourceAccessFlags accessFlags, RdrResource& rResource)
{
	size = (size + 255) & ~255;	// CB size is required to be 256-byte aligned.

	D3D12_RESOURCE_STATES eResourceState = IsFlagSet(accessFlags, RdrResourceAccessFlags::CpuWrite) ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	ID3D12Resource* pResource = CreateBuffer(size, accessFlags, eResourceState);

	// Describe and create a constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = pResource->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = size;

	RdrShaderResourceView srv;
	srv.hView = m_srvHeap.AllocateDescriptor();
	m_pDevice->CreateConstantBufferView(&cbvDesc, srv.hView);

	rResource.InitAsConstantBuffer(accessFlags, size);
	rResource.BindDeviceResources(pResource, eResourceState, &srv, nullptr);

	if (pData)
	{
		UpdateResource(rResource, pData, size);
	}

	return true;
}

void RdrContext::UpdateResource(RdrResource& rResource, const void* pSrcData, const uint dataSize)
{
	ID3D12Resource* pDeviceResource = rResource.GetResource();

	if (IsFlagSet(rResource.GetAccessFlags(), RdrResourceAccessFlags::CpuWrite))
	{
		void* pDstData;
		CD3DX12_RANGE readRange(0, 0);
		HRESULT hr = pDeviceResource->Map(0, &readRange, &pDstData);
		if (FAILED(hr))
		{
			assert(false);
			return;
		}

		memcpy(pDstData, pSrcData, dataSize);

		CD3DX12_RANGE writeRange(0, dataSize);
		pDeviceResource->Unmap(0, &writeRange);
	}
	else
	{
		RdrUploadBuffer& uploadBuffer = m_uploadBuffers[m_currBackBuffer];
		if (uploadBuffer.pCurr + dataSize <= uploadBuffer.pEnd)
		{
			memcpy(uploadBuffer.pCurr, pSrcData, dataSize);

			D3D12_RESOURCE_STATES eOrigState = rResource.GetResourceState();
			TransitionResource(&rResource, D3D12_RESOURCE_STATE_COPY_DEST);
			m_pCommandList->CopyBufferRegion(pDeviceResource, 0, uploadBuffer.pBuffer.Get(), (uploadBuffer.pCurr - uploadBuffer.pStart), dataSize);
			TransitionResource(&rResource, eOrigState);

			uploadBuffer.pCurr += dataSize;
		}
	}
}

void RdrContext::Draw(const RdrDrawState& rDrawState, uint instanceCount)
{
	m_pCommandList->SetGraphicsRootSignature(m_pGraphicsRootSignature.Get());
	m_pCommandList->SetPipelineState(rDrawState.pipelineState.pPipelineState.Get());

	// Vertex buffers
	D3D12_VERTEX_BUFFER_VIEW views[RdrDrawState::kMaxVertexBuffers];
	for (uint i = 0; i < rDrawState.vertexBufferCount; ++i)
	{
		const RdrResource* pBuffer = rDrawState.pVertexBuffers[i];
		views[i].BufferLocation = pBuffer->GetResource() ? pBuffer->GetResource()->GetGPUVirtualAddress() : 0;
		views[i].BufferLocation += rDrawState.vertexOffsets[i];
		views[i].SizeInBytes = pBuffer->GetSize();
		views[i].StrideInBytes = rDrawState.vertexStrides[i];
	}

	m_pCommandList->IASetVertexBuffers(0, rDrawState.vertexBufferCount, views);
	m_rProfiler.IncrementCounter(RdrProfileCounter::VertexBuffer);

	// VS constants
	uint nDescriptorStartIndex;
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescCurr = m_dynamicDescriptorHeap.AllocateDescriptors(rDrawState.vsConstantBufferCount, nDescriptorStartIndex);
	m_pCommandList->SetGraphicsRootDescriptorTable(kRootVsConstantBufferTable, m_dynamicDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < rDrawState.vsConstantBufferCount; ++i)
	{
		m_pDevice->CopyDescriptorsSimple(1, hDescCurr, rDrawState.vsConstantBuffers[i].hView, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsConstantBuffer);
	}

	// VS resources
	hDescCurr = m_dynamicDescriptorHeap.AllocateDescriptors(rDrawState.vsResourceCount, nDescriptorStartIndex);
	m_pCommandList->SetGraphicsRootDescriptorTable(kRootVsShaderResourceViewTable, m_dynamicDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < rDrawState.vsResourceCount; ++i)
	{
		m_pDevice->CopyDescriptorsSimple(1, hDescCurr, rDrawState.vsResources[i].hView, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsResource);
	}

#if 0 // donotcheckin - ignoring for now
	// GS constants
	for (uint i = 0; i < rDrawState.gsConstantBufferCount; ++i)
	{
		if (rDrawState.gsConstantBuffers[i] != m_drawState.gsConstantBuffers[i])
		{
			m_pCommandList->SetGraphicsRootShaderResourceView(kRootConstantBufferStart + i, rDrawState.gsConstantBuffers[i].hView);
			m_rProfiler.IncrementCounter(RdrProfileCounter::GsConstantBuffer);
		}
	}

	if (rDrawState.pDomainShader)
	{
		for (uint i = 0; i < rDrawState.dsConstantBufferCount; ++i)
		{
			if (rDrawState.dsConstantBuffers[i] != m_drawState.dsConstantBuffers[i])
			{
				m_pCommandList->SetGraphicsRootConstantBufferView(i, rDrawState.dsConstantBuffers[i].pBufferD3D12->GetGPUVirtualAddress());
				m_rProfiler.IncrementCounter(RdrProfileCounter::DsConstantBuffer);
			}
		}

		for (uint i = 0; i < rDrawState.dsResourceCount; ++i)
		{
			if (rDrawState.dsResources[i] != m_drawState.dsResources[i])
			{
				m_pCommandList->SetGraphicsRootDescriptorTable(i, rDrawState.dsResources[i].hView);
				m_rProfiler.IncrementCounter(RdrProfileCounter::DsResource);
			}
		}

		for (uint i = 0; i < rDrawState.dsSamplerCount; ++i)
		{
			if (rDrawState.dsSamplers[i] != m_drawState.dsSamplers[i])
			{
				m_pCommandList->SetGraphicsRootDescriptorTable(i, rDrawState.dsSamplers[i].);
				m_rProfiler.IncrementCounter(RdrProfileCounter::DsResource);
			}
		}

		if (updateDataList<RdrSamplerState>(rDrawState.dsSamplers, m_drawState.dsSamplers,
			rDrawState.dsSamplerCount, &firstChanged, &lastChanged))
		{
			ID3D12SamplerState* apSamplers[ARRAY_SIZE(rDrawState.dsSamplers)] = { 0 };
			for (int i = firstChanged; i <= lastChanged; ++i)
			{
				apSamplers[i] = GetSampler(rDrawState.dsSamplers[i]);
			}
			m_pCommandList->DSSetSamplers(firstChanged, lastChanged - firstChanged + 1, apSamplers + firstChanged);
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsSamplers);
		}
	}
#endif

	// PS constants
	hDescCurr = m_dynamicDescriptorHeap.AllocateDescriptors(rDrawState.psConstantBufferCount, nDescriptorStartIndex);
	m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsConstantBufferTable, m_dynamicDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < rDrawState.psConstantBufferCount; ++i)
	{
		if (rDrawState.psConstantBuffers[i].hView.ptr != 0) // donotcheckin - reorganize resources
		{
			m_pDevice->CopyDescriptorsSimple(1, hDescCurr, rDrawState.psConstantBuffers[i].hView, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			m_rProfiler.IncrementCounter(RdrProfileCounter::PsConstantBuffer);
		}
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
	}

	// PS resources
	hDescCurr = m_dynamicDescriptorHeap.AllocateDescriptors(rDrawState.psResourceCount, nDescriptorStartIndex);
	m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsShaderResourceViewTable, m_dynamicDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < rDrawState.psResourceCount; ++i)
	{
		if (rDrawState.psResources[i].hView.ptr != 0) // donotcheckin - reorganize resources
		{
			m_pDevice->CopyDescriptorsSimple(1, hDescCurr, rDrawState.psResources[i].hView, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			m_rProfiler.IncrementCounter(RdrProfileCounter::PsResource);
		}
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
	}

	// PS samplers
	hDescCurr = m_dynamicSamplerDescriptorHeap.AllocateDescriptors(rDrawState.psSamplerCount, nDescriptorStartIndex);
	m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsSamplerTable, m_dynamicSamplerDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < rDrawState.psSamplerCount; ++i)
	{
		m_pDevice->CopyDescriptorsSimple(1, hDescCurr, GetSampler(rDrawState.psSamplers[i]), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsSamplers);
	}

	if (rDrawState.eTopology != m_drawState.eTopology)
	{
		m_pCommandList->IASetPrimitiveTopology(getD3DTopology(rDrawState.eTopology));
		m_drawState.eTopology = rDrawState.eTopology;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PrimitiveTopology);
	}

	if (rDrawState.pIndexBuffer && rDrawState.pIndexBuffer->GetResource())
	{
		if (rDrawState.pIndexBuffer != m_drawState.pIndexBuffer)
		{
			D3D12_INDEX_BUFFER_VIEW view;
			view.BufferLocation = rDrawState.pIndexBuffer->GetResource()->GetGPUVirtualAddress();
			view.Format = DXGI_FORMAT_R16_UINT;
			view.SizeInBytes = rDrawState.indexCount * sizeof(uint16);

			m_pCommandList->IASetIndexBuffer(&view);
			m_drawState.pIndexBuffer = rDrawState.pIndexBuffer;
			m_rProfiler.IncrementCounter(RdrProfileCounter::IndexBuffer);
		}

		m_pCommandList->DrawIndexedInstanced(rDrawState.indexCount, instanceCount, 0, 0, 0);
	}
	else
	{
		m_pCommandList->DrawInstanced(rDrawState.vertexCount, instanceCount, 0, 0);
	}

#if 0
	//////////////////////////////////////////////////////////////////////////
	// Input assembler

	// Primitive topology
	if (rDrawState.eTopology != m_drawState.eTopology)
	{
		m_pCommandList->IASetPrimitiveTopology(getD3DTopology(rDrawState.eTopology));
		m_drawState.eTopology = rDrawState.eTopology;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PrimitiveTopology);
	}

	// Input layout
	if (rDrawState.inputLayout.pInputLayout != m_drawState.inputLayout.pInputLayout)
	{
		m_pCommandList->IASetInputLayout(rDrawState.inputLayout.pInputLayout);
		m_drawState.inputLayout.pInputLayout = rDrawState.inputLayout.pInputLayout;
		m_rProfiler.IncrementCounter(RdrProfileCounter::InputLayout);
	}

	//////////////////////////////////////////////////////////////////////////
	// Vertex shader
	if (rDrawState.pVertexShader != m_drawState.pVertexShader)
	{
		m_pCommandList->VSSetShader(rDrawState.pVertexShader->pVertex, nullptr, 0);
		m_drawState.pVertexShader = rDrawState.pVertexShader;
		m_rProfiler.IncrementCounter(RdrProfileCounter::VertexShader);
	}

	//////////////////////////////////////////////////////////////////////////
	// Geometry shader
	if (rDrawState.pGeometryShader != m_drawState.pGeometryShader)
	{
		if (rDrawState.pGeometryShader)
		{
			m_pCommandList->GSSetShader(rDrawState.pGeometryShader->pGeometry, nullptr, 0);
		}
		else
		{
			m_pCommandList->GSSetShader(nullptr, nullptr, 0);
		}
		m_rProfiler.IncrementCounter(RdrProfileCounter::GeometryShader);
		m_drawState.pGeometryShader = rDrawState.pGeometryShader;
	}

	//////////////////////////////////////////////////////////////////////////
	// Hull shader
	if (rDrawState.pHullShader != m_drawState.pHullShader)
	{
		if (rDrawState.pHullShader)
		{
			m_pCommandList->HSSetShader(rDrawState.pHullShader->pHull, nullptr, 0);
		}
		else
		{
			m_pCommandList->HSSetShader(nullptr, nullptr, 0);
		}
		m_drawState.pHullShader = rDrawState.pHullShader;
		m_rProfiler.IncrementCounter(RdrProfileCounter::HullShader);
	}

	//////////////////////////////////////////////////////////////////////////
	// Domain shader
	if (rDrawState.pDomainShader != m_drawState.pDomainShader)
	{
		if (rDrawState.pDomainShader)
		{
			m_pCommandList->DSSetShader(rDrawState.pDomainShader->pDomain, nullptr, 0);
		}
		else
		{
			m_pCommandList->DSSetShader(nullptr, nullptr, 0);
		}
		m_drawState.pDomainShader = rDrawState.pDomainShader;
		m_rProfiler.IncrementCounter(RdrProfileCounter::DomainShader);
	}

	//////////////////////////////////////////////////////////////////////////
	// Pixel shader
	if (rDrawState.pPixelShader != m_drawState.pPixelShader)
	{
		if (rDrawState.pPixelShader)
		{
			m_pCommandList->PSSetShader(rDrawState.pPixelShader->pPixel, nullptr, 0);
		}
		else
		{
			m_pCommandList->PSSetShader(nullptr, nullptr, 0);
		}
		m_drawState.pPixelShader = rDrawState.pPixelShader;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PixelShader);
	}
#endif
}

void RdrContext::DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ)
{
	m_drawState.Reset();

	m_pCommandList->SetComputeRootSignature(m_pComputeRootSignature.Get());
	m_pCommandList->SetPipelineState(rDrawState.pipelineState.pPipelineState.Get());

	uint nDescriptorStartIndex;
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescCurr = m_dynamicDescriptorHeap.AllocateDescriptors(rDrawState.csConstantBufferCount, nDescriptorStartIndex);
	m_pCommandList->SetComputeRootDescriptorTable(kRootCsConstantBufferTable, m_dynamicDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < rDrawState.csConstantBufferCount; ++i)
	{
		m_pDevice->CopyDescriptorsSimple(1, hDescCurr, rDrawState.csConstantBuffers[i].hView, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
		m_rProfiler.IncrementCounter(RdrProfileCounter::CsConstantBuffer);
	}

	// CS resources
	uint numResources = ARRAY_SIZE(rDrawState.csResources);
	hDescCurr = m_dynamicDescriptorHeap.AllocateDescriptors(numResources, nDescriptorStartIndex);
	m_pCommandList->SetComputeRootDescriptorTable(kRootCsShaderResourceViewTable, m_dynamicDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < numResources; ++i)
	{
		if (rDrawState.csResources[i].hView.ptr != 0) // donotcheckin - define numResources explicitly
		{
			m_pDevice->CopyDescriptorsSimple(1, hDescCurr, rDrawState.csResources[i].hView, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			m_rProfiler.IncrementCounter(RdrProfileCounter::CsResource);
		}
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
	}

	uint numSamplers = ARRAY_SIZE(rDrawState.csSamplers);
	hDescCurr = m_dynamicSamplerDescriptorHeap.AllocateDescriptors(numSamplers, nDescriptorStartIndex);
	m_pCommandList->SetComputeRootDescriptorTable(kRootCsSamplerTable, m_dynamicSamplerDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < numSamplers; ++i)
	{
		m_pDevice->CopyDescriptorsSimple(1, hDescCurr, GetSampler(rDrawState.csSamplers[i]), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
		m_rProfiler.IncrementCounter(RdrProfileCounter::CsSampler);
	}

	uint numUavs = ARRAY_SIZE(rDrawState.csUavs);
	hDescCurr = m_dynamicDescriptorHeap.AllocateDescriptors(numUavs, nDescriptorStartIndex);
	m_pCommandList->SetComputeRootDescriptorTable(kRootCsUnorderedAccessViewTable, m_dynamicDescriptorHeap.GetGpuHandle(nDescriptorStartIndex));
	for (uint i = 0; i < numUavs; ++i)
	{
		if (rDrawState.csUavs[i].hView.ptr != 0) // donotcheckin - define numUavs explicitly
		{
			m_pDevice->CopyDescriptorsSimple(1, hDescCurr, rDrawState.csUavs[i].hView, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			m_rProfiler.IncrementCounter(RdrProfileCounter::CsUnorderedAccess);
		}
		hDescCurr.Offset(1, m_dynamicDescriptorHeap.GetDescriptorSize());
	}

	m_pCommandList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);

	// donotcheckin? Clear resources to avoid binding errors (input bound as output).  todo: don't do this

	m_drawState.Reset();
}

void RdrContext::PSClearResources()
{
	memset(m_drawState.psResources, 0, sizeof(m_drawState.psResources));

	// donotcheckin - do nothing?
	//ID3D12ShaderResourceView* resourceViews[ARRAY_SIZE(RdrDrawState::psResources)] = { 0 };
	//m_pCommandList->PSSetShaderResources(0, 20, resourceViews);
}

RdrQuery RdrContext::CreateQuery(RdrQueryType eType)
{
	RdrQuery query;

	switch (eType)
	{
	case RdrQueryType::Timestamp:
		query.hQuery = m_timestampQueryHeap.AllocateQuery();
		break;
	}

	query.bIsValid = true;
	return query;
}

void RdrContext::ReleaseQuery(RdrQuery& rQuery)
{
	rQuery.bIsValid = false;

	switch (rQuery.hQuery.nType)
	{
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		m_timestampQueryHeap.FreeQuery(rQuery.hQuery);
		break;
	}
}

void RdrContext::BeginQuery(RdrQuery& rQuery)
{
	switch (rQuery.hQuery.nType)
	{
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		// Timestamp queries don't have a begin.
		break;
	}
}

void RdrContext::EndQuery(RdrQuery& rQuery)
{
	switch (rQuery.hQuery.nType)
	{
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		m_pCommandList->EndQuery(m_timestampQueryHeap.GetHeap(), D3D12_QUERY_TYPE_TIMESTAMP, rQuery.hQuery.nId);
		break;
	}
}

uint64 RdrContext::GetTimestampQueryData(RdrQuery& rQuery)
{
	uint64 timestamp = -1;
	uint64 nBufferOffset = rQuery.hQuery.nId * sizeof(timestamp);
	m_pCommandList->ResolveQueryData(m_timestampQueryHeap.GetHeap(), // donotcheckin - do at end of frame and hit all the queries at once?
		D3D12_QUERY_TYPE_TIMESTAMP, 
		rQuery.hQuery.nId, 
		1, 
		m_timestampResultBuffer.Get(), 
		nBufferOffset);

	D3D12_RANGE readRange = { nBufferOffset, nBufferOffset + sizeof(timestamp) };
	uint64* pTimestampData = nullptr;
	HRESULT hr = m_timestampResultBuffer->Map(0, &readRange, (void**)&pTimestampData);
	if (hr == S_OK)
	{
		timestamp = pTimestampData[0];
	}

	D3D12_RANGE writeRange = { 0, 0 };
	m_timestampResultBuffer->Unmap(0, &writeRange);

	return timestamp;
}

uint64 RdrContext::GetTimestampFrequency() const
{
	return m_commandQueueTimestampFreqency;
}

void RdrContext::SetDescriptorHeaps()
{
	ID3D12DescriptorHeap* ppHeaps[] = {
		m_dynamicDescriptorHeap.GetHeap(),
		m_dynamicSamplerDescriptorHeap.GetHeap()
	};

	m_pCommandList->SetDescriptorHeaps(ARRAYSIZE(ppHeaps), ppHeaps);
}

namespace
{
	ID3D10Blob* compileShaderD3D(const char* pShaderText, uint textLen, const char* entrypoint, const char* shadermodel)
	{
		HRESULT hr;
		ID3D10Blob* pCompiledData;
		ID3D10Blob* pErrors = nullptr;

		uint flags = (g_userConfig.debugShaders ? D3DCOMPILE_DEBUG : D3DCOMPILE_OPTIMIZATION_LEVEL3);

		hr = D3DCompile(pShaderText, textLen, nullptr, nullptr, nullptr, entrypoint, shadermodel, flags, 0, &pCompiledData, &pErrors);
		if (hr != S_OK)
		{
			if (pCompiledData)
				pCompiledData->Release();
		}

		if (pErrors)
		{
			char errorMsg[2048];
			strcpy_s(errorMsg, sizeof(errorMsg), (char*)pErrors->GetBufferPointer());
			OutputDebugStringA(errorMsg);
			pErrors->Release();
		}

		return pCompiledData;
	}

	const char* getD3DSemanticName(RdrShaderSemantic eSemantic)
	{
		static const char* s_d3dSemantics[] = {
			"POSITION", // RdrShaderSemantic::Position
			"TEXCOORD", // RdrShaderSemantic::Texcoord
			"COLOR",    // RdrShaderSemantic::Color
			"NORMAL",   // RdrShaderSemantic::Normal
			"BINORMAL", // RdrShaderSemantic::Binormal
			"TANGENT"   // RdrShaderSemantic::Tangent
		};
		static_assert(ARRAY_SIZE(s_d3dSemantics) == (int)RdrShaderSemantic::Count, "Missing D3D12 shader semantic!");
		return s_d3dSemantics[(int)eSemantic];
	}

	D3D12_INPUT_CLASSIFICATION getD3DVertexInputClass(RdrVertexInputClass eClass)
	{
		static const D3D12_INPUT_CLASSIFICATION s_d3dClasses[] = {
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,		// RdrVertexInputClass::PerVertex
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA	// RdrVertexInputClass::PerInstance
		};
		static_assert(ARRAY_SIZE(s_d3dClasses) == (int)RdrVertexInputClass::Count, "Missing D3D12 vertex input class!");
		return s_d3dClasses[(int)eClass];
	}

	DXGI_FORMAT getD3DVertexInputFormat(RdrVertexInputFormat eFormat)
	{
		static const DXGI_FORMAT s_d3dFormats[] = {
			DXGI_FORMAT_R32_FLOAT,			// RdrVertexInputFormat::R_F32
			DXGI_FORMAT_R32G32_FLOAT,		// RdrVertexInputFormat::RG_F32
			DXGI_FORMAT_R32G32B32_FLOAT,	// RdrVertexInputFormat::RGB_F32
			DXGI_FORMAT_R32G32B32A32_FLOAT	// RdrVertexInputFormat::RGBA_F32
		};
		static_assert(ARRAY_SIZE(s_d3dFormats) == (int)RdrVertexInputFormat::Count, "Missing D3D12 vertex input format!");
		return s_d3dFormats[(int)eFormat];
	}
}


bool RdrContext::CompileShader(RdrShaderStage eType, const char* pShaderText, uint textLen, void** ppOutCompiledData, uint* pOutDataSize) const
{
	static const char* s_d3dShaderModels[] = {
		"vs_5_0", // RdrShaderStage::Vertex
		"ps_5_0", // RdrShaderStage::Pixel
		"gs_5_0", // RdrShaderStage::Geometry
		"hs_5_0", // RdrShaderStage::Hull
		"ds_5_0", // RdrShaderStage::Domain
		"cs_5_0"  // RdrShaderStage::Compute
	};
	static_assert(ARRAY_SIZE(s_d3dShaderModels) == (int)RdrShaderStage::Count, "Missing D3D shader models!");

	ID3D10Blob* pCompiledData = compileShaderD3D(pShaderText, textLen, "main", s_d3dShaderModels[(int)eType]);
	if (!pCompiledData)
		return false;

	*ppOutCompiledData = new char[pCompiledData->GetBufferSize()];
	memcpy(*ppOutCompiledData, pCompiledData->GetBufferPointer(), pCompiledData->GetBufferSize());
	*pOutDataSize = (uint)pCompiledData->GetBufferSize();

	pCompiledData->Release();
	return true;
}

void RdrContext::ReleaseShader(RdrShader* pShader) const
{
	delete[] pShader->pCompiledData;
}

RdrPipelineState RdrContext::CreateGraphicsPipelineState(
	const RdrShader* pVertexShader, const RdrShader* pPixelShader,
	const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
	const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
	const RdrBlendMode eBlendMode,
	const RdrRasterState& rasterState,
	const RdrDepthStencilState& depthStencilState)
{
	D3D12_INPUT_ELEMENT_DESC d3dVertexDesc[32];
	for (uint i = 0; i < nNumInputElements; ++i)
	{
		d3dVertexDesc[i].SemanticName = getD3DSemanticName(pInputLayoutElements[i].semantic);
		d3dVertexDesc[i].SemanticIndex = pInputLayoutElements[i].semanticIndex;
		d3dVertexDesc[i].AlignedByteOffset = pInputLayoutElements[i].byteOffset;
		d3dVertexDesc[i].Format = getD3DVertexInputFormat(pInputLayoutElements[i].format);
		d3dVertexDesc[i].InputSlotClass = getD3DVertexInputClass(pInputLayoutElements[i].inputClass);
		d3dVertexDesc[i].InputSlot = pInputLayoutElements[i].streamIndex;
		d3dVertexDesc[i].InstanceDataStepRate = pInputLayoutElements[i].instanceDataStepRate;
	}

	// Describe and create the graphics pipeline state objects (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { d3dVertexDesc, nNumInputElements };
	psoDesc.pRootSignature = m_pGraphicsRootSignature.Get();
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(pVertexShader->pCompiledData, pVertexShader->compiledSize);
	psoDesc.PS = pPixelShader ? CD3DX12_SHADER_BYTECODE(pPixelShader->pCompiledData, pPixelShader->compiledSize) : CD3DX12_SHADER_BYTECODE(0, 0);
	psoDesc.DepthStencilState.DepthEnable = depthStencilState.bTestDepth;
	psoDesc.DepthStencilState.DepthWriteMask = depthStencilState.bWriteDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = getComparisonFuncD3d(depthStencilState.eDepthFunc);
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DSVFormat = (psoDesc.DepthStencilState.DepthEnable ? getD3DDepthFormat(kDefaultDepthFormat) : DXGI_FORMAT_UNKNOWN);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = nNumRtvFormats;
	for (uint i = 0; i < nNumRtvFormats; ++i)
	{
		psoDesc.RTVFormats[i] = getD3DFormat(pRtvFormats[i]);
	}
	psoDesc.SampleDesc.Count = 1;

	// Rasterizer state
	{
		psoDesc.RasterizerState.AntialiasedLineEnable = !rasterState.bEnableMSAA;
		psoDesc.RasterizerState.MultisampleEnable = rasterState.bEnableMSAA;
		psoDesc.RasterizerState.CullMode = rasterState.bDoubleSided ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
		psoDesc.RasterizerState.SlopeScaledDepthBias = rasterState.bUseSlopeScaledDepthBias ? m_slopeScaledDepthBias : 0.f;
		psoDesc.RasterizerState.DepthBiasClamp = 0.f;
		psoDesc.RasterizerState.DepthClipEnable = true;
		psoDesc.RasterizerState.FrontCounterClockwise = false;
		psoDesc.RasterizerState.ForcedSampleCount = 0;
		psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		if (rasterState.bWireframe)
		{
			psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
			psoDesc.RasterizerState.DepthBias = -2; // Wireframe gets a slight depth bias so z-fighting doesn't occur.
		}
		else
		{
			psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			psoDesc.RasterizerState.DepthBias = 0;
		}
	}


	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.BlendState.IndependentBlendEnable = false;
	psoDesc.BlendState.AlphaToCoverageEnable = false;
	for (uint i = 0; i < ARRAY_SIZE(psoDesc.BlendState.RenderTarget); ++i)
	{
		switch (eBlendMode)
		{
		case RdrBlendMode::kOpaque:
			psoDesc.BlendState.RenderTarget[i].BlendEnable = true;
			psoDesc.BlendState.RenderTarget[i].LogicOpEnable = false;
			psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
			psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
			psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kAlpha:
			psoDesc.BlendState.RenderTarget[i].BlendEnable = true;
			psoDesc.BlendState.RenderTarget[i].LogicOpEnable = false;
			psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
			psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kAdditive:
			psoDesc.BlendState.RenderTarget[i].BlendEnable = true;
			psoDesc.BlendState.RenderTarget[i].LogicOpEnable = false;
			psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
			psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kSubtractive:
			psoDesc.BlendState.RenderTarget[i].BlendEnable = true;
			psoDesc.BlendState.RenderTarget[i].LogicOpEnable = false;
			psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
			psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_REV_SUBTRACT;
			psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
			psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			break;
		}
	}


	RdrPipelineState pipelineState;
	m_pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState.pPipelineState));
	return pipelineState;
}

RdrPipelineState RdrContext::CreateComputePipelineState(const RdrShader& computeShader)
{
	// Describe and create the compute pipeline state objects (PSO).
	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.pCompiledData, computeShader.compiledSize);
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	psoDesc.NodeMask = 0;
	psoDesc.pRootSignature = m_pComputeRootSignature.Get();

	RdrPipelineState pipelineState;
	m_pDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState.pPipelineState));
	return pipelineState;
}