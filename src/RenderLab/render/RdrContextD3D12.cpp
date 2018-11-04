#include "Precompiled.h"
#include "RdrContextD3D12.h"
#include "RdrDrawOp.h"
#include "RdrDrawState.h"
#include "RdrProfiler.h"

#include "pix3.h"
#include <d3d12.h>
#pragma comment (lib, "d3d12.lib")
#include <dxgi1_6.h>
#include "d3dx12.h"

namespace
{
	static const int kMaxNumRenderTargetViews = 64;
	static const int kMaxNumShaderResourceViews = 1024;
	static const int kMaxNumSamplers = 64;
	static const int kMaxNumDepthStencilViews = 64;

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

	CD3DX12_HEAP_PROPERTIES selectHeapProperties(const RdrCpuAccessFlags cpuAccessFlags, const RdrResourceUsage eUsage)
	{
		//donotcheckin
		if ((cpuAccessFlags & RdrCpuAccessFlags::Write) != RdrCpuAccessFlags::None)
			return CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		return CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	}

	void createBufferSrv(ComPtr<ID3D12Device2> pDevice, DescriptorHeap& srvHeap, const RdrResource& rResource, uint numElements, uint structureByteStride, RdrResourceFormat eFormat, int firstElement, D3D12DescriptorHandle* pOutView)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC desc;
		desc.Buffer.FirstElement = firstElement;
		desc.Buffer.NumElements = numElements;
		desc.Buffer.StructureByteStride = structureByteStride;
		desc.Format = getD3DFormat(eFormat);
		desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
		descHandle.ptr = srvHeap.AllocateDescriptor();

		pDevice->CreateShaderResourceView(rResource.pResource12, &desc, descHandle);

		*pOutView = descHandle.ptr;
	}
}

bool RdrContextD3D12::CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResource& rResource)
{
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = numElements * rdrGetTexturePitch(1, eFormat);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	RdrCpuAccessFlags cpuAccessFlags = RdrCpuAccessFlags::None;//donotcheckin
	CD3DX12_HEAP_PROPERTIES heapProperties = selectHeapProperties(cpuAccessFlags, eUsage);
	HRESULT hr = m_pDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&rResource.pResource12)); //donotcheckin - manage ref count?

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	/*donotcheckin
	if (eUsage == RdrResourceUsage::Staging)
	{
		desc.CPUAccessFlags = D3D12_CPU_ACCESS_READ;
		desc.BindFlags = 0;
	}
	else if (eUsage == RdrResourceUsage::Dynamic)
	{
		desc.CPUAccessFlags = D3D12_CPU_ACCESS_WRITE;
		desc.BindFlags = D3D12_BIND_SHADER_RESOURCE;
	}
	else
	{
		desc.CPUAccessFlags = 0;
		desc.BindFlags = D3D12_BIND_UNORDERED_ACCESS | D3D12_BIND_SHADER_RESOURCE;
	}
	*/
	//donotcheckin - staging should need srv/uav..>?
	if (eUsage != RdrResourceUsage::Staging)
	{
		createBufferSrv(m_pDevice, m_srvHeap, rResource, numElements, 0, eFormat, 0, &rResource.resourceView.hViewD3D12);
	}

	if (eUsage != RdrResourceUsage::Dynamic && eUsage != RdrResourceUsage::Staging)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
		desc.Buffer.FirstElement = 0;
		desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		desc.Buffer.NumElements = numElements;
		desc.Format = getD3DFormat(eFormat);
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
		descHandle.ptr = m_srvHeap.AllocateDescriptor();

		m_pDevice->CreateUnorderedAccessView(rResource.pResource12, nullptr, &desc, descHandle);
		rResource.uav.hViewD3D12 = descHandle.ptr;
	}

	if (pSrcData)
	{
		//donotcheckin
	}

	return true;
}

bool RdrContextD3D12::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage, RdrResource& rResource)
{
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = numElements * elementSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	RdrCpuAccessFlags cpuAccessFlags = RdrCpuAccessFlags::None;//donotcheckin
	CD3DX12_HEAP_PROPERTIES heapProperties = selectHeapProperties(cpuAccessFlags, eUsage);
	HRESULT hr = m_pDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&rResource.pResource12)); //donotcheckin - manage ref count?

	if (FAILED(hr))
	{
		assert(false);
		return false;
	}

	/* donotcheckin
	if (eUsage == RdrResourceUsage::Staging)
	{
		desc.CPUAccessFlags = D3D12_CPU_ACCESS_READ;
		desc.BindFlags = 0;
	}
	else if (eUsage == RdrResourceUsage::Dynamic)
	{
		desc.CPUAccessFlags = D3D12_CPU_ACCESS_WRITE;
		desc.BindFlags = D3D12_BIND_SHADER_RESOURCE;
	}
	else
	{
		desc.CPUAccessFlags = 0;
		desc.BindFlags = D3D12_BIND_UNORDERED_ACCESS | D3D12_BIND_SHADER_RESOURCE;
	}*/

	if (eUsage != RdrResourceUsage::Staging)
	{
		createBufferSrv(m_pDevice, m_srvHeap, rResource, numElements, elementSize, RdrResourceFormat::UNKNOWN, 0, &rResource.resourceView.hViewD3D12);
	}

	if (eUsage != RdrResourceUsage::Dynamic && eUsage != RdrResourceUsage::Staging)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC desc;
		desc.Buffer.FirstElement = 0;
		desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		desc.Buffer.NumElements = numElements;
		desc.Buffer.StructureByteStride = elementSize;
		desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
		descHandle.ptr = m_srvHeap.AllocateDescriptor();

		m_pDevice->CreateUnorderedAccessView(rResource.pResource12, nullptr, &desc, descHandle);
		rResource.uav.hViewD3D12 = descHandle.ptr;
	}

	if (pSrcData)
	{
		//donotcheckin
	}
	return true;
}

bool RdrContextD3D12::UpdateBuffer(const void* pSrcData, RdrResource& rResource, int numElements)
{
	if (numElements == -1)
	{
		numElements = rResource.bufferInfo.numElements;
	}

	uint dataSize = rResource.bufferInfo.elementSize
		? (rResource.bufferInfo.elementSize * numElements)
		: rdrGetTexturePitch(1, rResource.bufferInfo.eFormat) * numElements;

	if (rResource.eUsage == RdrResourceUsage::Dynamic)
	{
		void* pDstData;
		CD3DX12_RANGE readRange(0, 0);
		HRESULT hr = rResource.pResource12->Map(0, &readRange, &pDstData);
		if (FAILED(hr))
		{
			assert(false);
			return false;
		}

		memcpy(pDstData, pSrcData, dataSize);

		CD3DX12_RANGE writeRange(0, dataSize);
		rResource.pResource12->Unmap(0, &writeRange);
	}
	else
	{
		D3D12_BOX box = { 0 };
		box.top = 0;
		box.bottom = 1;
		box.front = 0;
		box.back = 1;
		box.left = 0;
		box.right = dataSize;

		rResource.pResource12->WriteToSubresource(0, &box, pSrcData, 0, 0);
	}

	return true;
}

void RdrContextD3D12::CopyResourceRegion(const RdrResource& rSrcResource, const RdrBox& srcRegion, const RdrResource& rDstResource, const IVec3& dstOffset)
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

void RdrContextD3D12::ReadResource(const RdrResource& rSrcResource, void* pDstData, uint dstDataSize)
{
	void* pMappedData;
	D3D12_RANGE readRange = { 0, dstDataSize };
	HRESULT hr = rSrcResource.pResource12->Map(0, &readRange, &pMappedData);
	if (FAILED(hr))
	{
		assert(false);
		return;
	}

	memcpy(pDstData, pMappedData, dstDataSize);

	D3D12_RANGE writeRange = { 0,0 };
	rSrcResource.pResource12->Unmap(0, nullptr);
}

void RdrContextD3D12::CreateBuffer(const void* pSrcData, const int size, const D3D12_RESOURCE_STATES initialState, RdrBuffer& rBuffer)
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

	RdrCpuAccessFlags cpuAccessFlags = RdrCpuAccessFlags::None;//donotcheckin
	RdrResourceUsage eUsage = RdrResourceUsage::Default;
	CD3DX12_HEAP_PROPERTIES heapProperties = selectHeapProperties(cpuAccessFlags, eUsage);
	HRESULT hr = m_pDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState, //donotcheckin
		nullptr,
		IID_PPV_ARGS(&rBuffer.pBuffer12)); //donotcheckin - manage ref count?

	if (FAILED(hr))
	{
		assert(false);
		return;
	}

	//donotcheckin desc.Usage = (bindFlags & D3D12_BIND_UNORDERED_ACCESS) ? D3D12_USAGE_DEFAULT : D3D12_USAGE_IMMUTABLE;

	if (pSrcData)
	{
		//donotcheckin
	}
}

RdrBuffer RdrContextD3D12::CreateVertexBuffer(const void* vertices, int size, RdrResourceUsage eUsage)
{
	RdrBuffer buffer;
	CreateBuffer(vertices, size, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, buffer);
	return buffer;
}

RdrBuffer RdrContextD3D12::CreateIndexBuffer(const void* indices, int size, RdrResourceUsage eUsage)
{
	RdrBuffer buffer;
	CreateBuffer(indices, size, D3D12_RESOURCE_STATE_INDEX_BUFFER, buffer);
	return buffer;
}

void RdrContextD3D12::ReleaseBuffer(const RdrBuffer& rBuffer)
{
	rBuffer.pBuffer12->Release();
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

RdrContextD3D12::RdrContextD3D12(RdrProfiler& rProfiler)
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
				D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
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

	return dxgiAdapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device2> d3d12Device2;
	HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2));
	if (FAILED(hr))
	{
		assert(false);
		return nullptr;
	}

	return d3d12Device2;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
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

void DescriptorHeap::Create(ComPtr<ID3D12Device2> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint nMaxDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = nMaxDescriptors;
	desc.Type = type;

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
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart()).Offset(nId).ptr;
}

void DescriptorHeap::FreeDescriptorById(uint nId)
{
	return m_idSet.releaseId(nId);
}

void DescriptorHeap::FreeDescriptor(D3D12DescriptorHandle hDesc)
{
	uint64 nId = (hDesc - m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) / m_descriptorSize;
	m_idSet.releaseId((uint)nId);
}

void DescriptorHeap::GetDescriptorHandle(uint nId, CD3DX12_CPU_DESCRIPTOR_HANDLE* pOutDesc)
{
	*pOutDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart()).Offset(nId, m_descriptorSize);
}


void QueryHeap::Create(ComPtr<ID3D12Device2> pDevice, D3D12_QUERY_HEAP_TYPE type, uint nMaxQueries)
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


ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device,
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

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device,
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

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
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

uint64_t Signal(ComPtr<ID3D12CommandQueue> pCommandQueue, ComPtr<ID3D12Fence> pFence,
	uint64_t& fenceValue)
{
	uint64_t fenceValueForSignal = ++fenceValue;
	HRESULT hr = pCommandQueue->Signal(pFence.Get(), fenceValueForSignal);
	if (FAILED(hr))
	{
		assert(false);
		return 0;
	}

	return fenceValueForSignal;
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
	uint64_t& fenceValue, HANDLE hFenceEvent)
{
	uint64_t fenceValueForSignal = Signal(pCommandQueue, pFence, fenceValue);
	WaitForFenceValue(pFence, fenceValueForSignal, hFenceEvent);
}

void RdrContextD3D12::UpdateRenderTargetViews()
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

		if (m_hBackBufferRtvs[i] == 0)
		{
			m_hBackBufferRtvs[i] = m_rtvHeap.AllocateDescriptor();
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE desc;
		desc.ptr = m_hBackBufferRtvs[i];
		
		m_pDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, desc);
		m_pBackBuffers[i] = backBuffer;
	}
}

bool RdrContextD3D12::Init(HWND hWnd, uint width, uint height)
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
	
	m_pSwapChain = CreateSwapChain(hWnd, m_pCommandQueue, width, height, kNumBackBuffers);
	m_currBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();

	m_rtvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kMaxNumRenderTargetViews);
	m_srvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMaxNumShaderResourceViews);
	m_samplerHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, kMaxNumSamplers);
	m_dsvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kMaxNumDepthStencilViews);

	for (int i = 0; i < kNumBackBuffers; ++i)
	{
		m_pCommandAllocators[i] = CreateCommandAllocator(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}

	m_pCommandList = CreateCommandList(m_pDevice, m_pCommandAllocators[m_currBackBuffer], D3D12_COMMAND_LIST_TYPE_DIRECT);

	m_pFence = CreateFence(m_pDevice);
	m_hFenceEvent = CreateEventHandle();

	// Query heaps
	const uint kMaxTimestampQueries = 128;
	m_timestampQueryHeap.Create(m_pDevice, D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 128);


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
		ComPtr<ID3D12InfoQueue> pInfoQueue;
		if (SUCCEEDED(m_pDevice.As(&pInfoQueue)))
		{
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
		}
	}

	Resize(width, height);
	ResetDrawState();
	return true;
}

void RdrContextD3D12::Release()
{
	//donotcheckin
	m_pSwapChain->Release();
	m_pDevice->Release();
}

bool RdrContextD3D12::IsIdle()
{
	return (m_presentFlags & DXGI_PRESENT_TEST);
}

/*donotcheckin
ID3D12SamplerState* RdrContextD3D12::GetSampler(const RdrSamplerState& state)
{
	uint samplerIndex =
		state.cmpFunc * ((uint)RdrTexCoordMode::Count * 2)
		+ state.texcoordMode * 2
		+ state.bPointSample;

	if (!m_pSamplers[samplerIndex])
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

		HRESULT hr = m_pDevice->CreateSamplerState(&desc, &m_pSamplers[samplerIndex]);
		assert(hr == S_OK);
	}

	return m_pSamplers[samplerIndex];
}
*/
namespace
{
	bool createTextureCubeSrv(ComPtr<ID3D12Device2> pDevice, DescriptorHeap& srvHeap, const RdrTextureInfo& rTexInfo, const RdrResource& rResource, D3D12DescriptorHandle* pOutView)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
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

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
		descHandle.ptr = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(rResource.pResource12, &viewDesc, descHandle);

		*pOutView = descHandle.ptr;
		return true;
	}

	bool createTextureCube(ComPtr<ID3D12Device2> pDevice, DescriptorHeap& srvHeap, D3D12_SUBRESOURCE_DATA* pData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource)
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
		if (resourceFormatIsDepth(rTexInfo.format))
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		else if (eUsage != RdrResourceUsage::Immutable)
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		//desc.Usage = getD3DUsage(eUsage); donotcheckin
		//desc.CPUAccessFlags = (desc.Usage == D3D12_USAGE_DYNAMIC) ? D3D12_CPU_ACCESS_WRITE : 0;

		D3D12_HEAP_PROPERTIES heapProps = selectHeapProperties(RdrCpuAccessFlags::None, eUsage);

		HRESULT hr = pDevice->CreateCommittedResource(
			&heapProps, 
			D3D12_HEAP_FLAG_NONE,
			&desc, 
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&rResource.pResource12)); //donotcheckin - manage ref count?

		if (FAILED(hr))
		{
			assert(false);
			return false;
		}

		createTextureCubeSrv(pDevice, srvHeap, rTexInfo, rResource, &rResource.resourceView.hViewD3D12);
		return true;
	}

	bool createTexture2DSrv(ComPtr<ID3D12Device2> pDevice, DescriptorHeap& srvHeap, const RdrTextureInfo& rTexInfo, const RdrResource& rResource, D3D12DescriptorHandle* pOutView)
	{
		bool bIsMultisampled = (rTexInfo.sampleCount > 1);
		bool bIsArray = (rTexInfo.depth > 1);

		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
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
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			}
		}
		else
		{
			if (bIsMultisampled)
			{
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				viewDesc.Texture2D.MipLevels = rTexInfo.mipLevels;
				viewDesc.Texture2D.MostDetailedMip = 0;
				viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			}
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
		descHandle.ptr = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(rResource.pResource12, &viewDesc, descHandle);

		*pOutView = descHandle.ptr;
		return true;
	}

	bool createTexture2D(ComPtr<ID3D12Device2> pDevice, DescriptorHeap& srvHeap, D3D12_SUBRESOURCE_DATA* pData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResourceBindings eBindings, RdrResource& rResource)
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
		if (resourceFormatIsDepth(rTexInfo.format))
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		else if (eUsage != RdrResourceUsage::Immutable)
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		//desc.Usage = getD3DUsage(eUsage); donotcheckin
		//desc.CPUAccessFlags = (desc.Usage == D3D12_USAGE_DYNAMIC) ? D3D12_CPU_ACCESS_WRITE : 0;

		D3D12_HEAP_PROPERTIES heapProps = selectHeapProperties(RdrCpuAccessFlags::None, eUsage);

		HRESULT hr = pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&rResource.pResource12)); //donotcheckin - manage ref count?

		if (FAILED(hr))
		{
			assert(false);
			return false;
		}

		/*donotcheckin
		if (desc.Usage == D3D12_USAGE_STAGING)
		{
			desc.CPUAccessFlags = D3D12_CPU_ACCESS_READ;
		}
		else
		{
			if (desc.Usage == D3D12_USAGE_DYNAMIC)
			{
				desc.CPUAccessFlags = D3D12_CPU_ACCESS_WRITE;
			}

			desc.BindFlags = D3D12_BIND_SHADER_RESOURCE;
			if (resourceFormatIsDepth(rTexInfo.format))
			{
				desc.BindFlags |= D3D12_BIND_DEPTH_STENCIL;
			}
			else
			{
				if (eBindings == RdrResourceBindings::kUnorderedAccessView)
				{
					desc.BindFlags |= D3D12_BIND_UNORDERED_ACCESS;
				}
				else if (eBindings == RdrResourceBindings::kRenderTarget)
				{
					desc.BindFlags |= D3D12_BIND_RENDER_TARGET;
				}
			}
		}
		*/

		//if (desc.BindFlags & D3D12_BIND_SHADER_RESOURCE) donotcheckin
		{
			createTexture2DSrv(pDevice, srvHeap, rTexInfo, rResource, &rResource.resourceView.hViewD3D12);
		}

		//if (desc.BindFlags & D3D12_BIND_UNORDERED_ACCESS) donotcheckin
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);
			if (bIsArray)
			{
				viewDesc.Texture2DArray.ArraySize = rTexInfo.depth;
				viewDesc.Texture2DArray.FirstArraySlice = 0;
				viewDesc.Texture2DArray.MipSlice = 0;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			}
			else
			{
				viewDesc.Texture2D.MipSlice = 0;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			}

			CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
			descHandle.ptr = srvHeap.AllocateDescriptor();

			pDevice->CreateUnorderedAccessView(rResource.pResource12, nullptr, &viewDesc, descHandle);
			rResource.uav.hViewD3D12 = descHandle.ptr;

			assert(hr == S_OK);
		}

		return true;
	}

	bool createTexture3DSrv(ComPtr<ID3D12Device2> pDevice, DescriptorHeap& srvHeap, const RdrTextureInfo& rTexInfo, const RdrResource& rResource, D3D12DescriptorHandle* pOutView)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc;
		viewDesc.Format = getD3DFormat(rTexInfo.format);
		viewDesc.Texture3D.MipLevels = rTexInfo.mipLevels;
		viewDesc.Texture3D.MostDetailedMip = 0;
		viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;

		CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
		descHandle.ptr = srvHeap.AllocateDescriptor();
		pDevice->CreateShaderResourceView(rResource.pResource12, &viewDesc, descHandle);
		*pOutView = descHandle.ptr;

		return true;
	}

	bool createTexture3D(ComPtr<ID3D12Device2> pDevice, DescriptorHeap& srvHeap, D3D12_SUBRESOURCE_DATA* pData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource)
	{
		bool bCanBindAccessView = !resourceFormatIsCompressed(rTexInfo.format) && eUsage != RdrResourceUsage::Immutable;
		
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
		if (resourceFormatIsDepth(rTexInfo.format))
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		else if (eUsage != RdrResourceUsage::Immutable)
			desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		//desc.Usage = getD3DUsage(eUsage); donotcheckin
		//desc.CPUAccessFlags = (desc.Usage == D3D12_USAGE_DYNAMIC) ? D3D12_CPU_ACCESS_WRITE : 0;

		D3D12_HEAP_PROPERTIES heapProps = selectHeapProperties(RdrCpuAccessFlags::None, eUsage);

		HRESULT hr = pDevice->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&desc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&rResource.pResource12)); //donotcheckin - manage ref count?

		if (FAILED(hr))
		{
			assert(false);
			return false;
		}

		/* donotcheckin
		if (desc.Usage == D3D12_USAGE_STAGING)
		{
			desc.CPUAccessFlags = D3D12_CPU_ACCESS_READ;
		}
		else
		{
			if (desc.Usage == D3D12_USAGE_DYNAMIC)
			{
				desc.CPUAccessFlags = D3D12_CPU_ACCESS_WRITE;
			}
			desc.BindFlags = D3D12_BIND_SHADER_RESOURCE;
			if (resourceFormatIsDepth(rTexInfo.format))
			{
				desc.BindFlags |= D3D12_BIND_DEPTH_STENCIL;
			}
			else
			{
				// todo: find out if having all these bindings cause any inefficiencies in D3D
				if (bCanBindAccessView)
					desc.BindFlags |= D3D12_BIND_UNORDERED_ACCESS;
				if (eUsage != RdrResourceUsage::Immutable)
					desc.BindFlags |= D3D12_BIND_RENDER_TARGET;
			}
		}
		*/

		//donotcheckin if (desc.BindFlags & D3D12_BIND_SHADER_RESOURCE)
		{
			createTexture3DSrv(pDevice, srvHeap, rTexInfo, rResource, &rResource.resourceView.hViewD3D12);
		}

		//donotcheckin if (desc.BindFlags & D3D12_BIND_UNORDERED_ACCESS)
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc;
			viewDesc.Format = getD3DFormat(rTexInfo.format);
			viewDesc.Texture3D.MipSlice = 0;
			viewDesc.Texture3D.FirstWSlice = 0;
			viewDesc.Texture3D.WSize = rTexInfo.depth;
			viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;

			CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
			descHandle.ptr = srvHeap.AllocateDescriptor();

			pDevice->CreateUnorderedAccessView(rResource.pResource12, nullptr, &viewDesc, descHandle);
			rResource.uav.hViewD3D12 = descHandle.ptr;
		}

		return true;
	}
}

bool RdrContextD3D12::CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResourceBindings eBindings, RdrResource& rResource)
{
	uint sliceCount = (rTexInfo.texType == RdrTextureType::kCube) ? rTexInfo.depth * 6 : rTexInfo.depth;
	D3D12_SUBRESOURCE_DATA* pData = nullptr;

	if (pSrcData)
	{
		uint resIndex = 0;
		pData = (D3D12_SUBRESOURCE_DATA*)_malloca(sliceCount * rTexInfo.mipLevels * sizeof(D3D12_SUBRESOURCE_DATA));

		char* pPos = (char*)pSrcData;
		for (uint slice = 0; slice < sliceCount; ++slice)
		{
			uint mipWidth = rTexInfo.width;
			uint mipHeight = rTexInfo.height;
			for (uint mip = 0; mip < rTexInfo.mipLevels; ++mip)
			{
				int rows = rdrGetTextureRows(mipHeight, rTexInfo.format);

				pData[resIndex].pData = pPos;
				pData[resIndex].RowPitch = rdrGetTexturePitch(mipWidth, rTexInfo.format);
				pData[resIndex].SlicePitch = pData[resIndex].RowPitch * rows;

				pPos += pData[resIndex].SlicePitch;

				if (mipWidth > 1)
					mipWidth >>= 1;
				if (mipHeight > 1)
					mipHeight >>= 1;
				++resIndex;
			}
		}
	}

	bool res = false;
	switch (rTexInfo.texType)
	{
	case RdrTextureType::k2D:
		res = createTexture2D(m_pDevice, m_srvHeap, pData, rTexInfo, eUsage, eBindings, rResource);
		break;
	case RdrTextureType::kCube:
		res = createTextureCube(m_pDevice, m_srvHeap, pData, rTexInfo, eUsage, rResource);
		break;
	case RdrTextureType::k3D:
		res = createTexture3D(m_pDevice, m_srvHeap, pData, rTexInfo, eUsage, rResource);
		break;
	}

	if (pData)
		_freea(pData);
	return res;
}

void RdrContextD3D12::ReleaseResource(RdrResource& rResource)
{
	if (rResource.pResource12)
	{
		rResource.pResource12->Release();
		rResource.pResource12 = nullptr;
	}
	if (rResource.resourceView.hViewD3D12)
	{
		m_srvHeap.FreeDescriptor(rResource.resourceView.hViewD3D12);
		rResource.resourceView.hViewD3D12 = 0;
	}
	if (rResource.uav.hViewD3D12)
	{
		m_srvHeap.FreeDescriptor(rResource.uav.hViewD3D12);
		rResource.uav.hViewD3D12 = 0;
	}
}

void RdrContextD3D12::ResolveResource(const RdrResource& rSrc, const RdrResource& rDst)
{
	m_pCommandList->ResolveSubresource(rDst.pResource12, 0, rSrc.pResource12, 0, getD3DFormat(rSrc.texInfo.format));
}

RdrShaderResourceView RdrContextD3D12::CreateShaderResourceViewTexture(const RdrResource& rResource)
{
	RdrShaderResourceView view = { 0 };
	switch (rResource.texInfo.texType)
	{
	case RdrTextureType::k2D:
		createTexture2DSrv(m_pDevice, m_srvHeap, rResource.texInfo, rResource, &view.hViewD3D12);
		break;
	case RdrTextureType::k3D:
		createTexture3DSrv(m_pDevice, m_srvHeap, rResource.texInfo, rResource, &view.hViewD3D12);
		break;
	case RdrTextureType::kCube:
		createTextureCubeSrv(m_pDevice, m_srvHeap, rResource.texInfo, rResource, &view.hViewD3D12);
		break;
	}
	return view;
}

RdrShaderResourceView RdrContextD3D12::CreateShaderResourceViewBuffer(const RdrResource& rResource, uint firstElement)
{
	RdrShaderResourceView view = { 0 };

	const RdrBufferInfo& rBufferInfo = rResource.bufferInfo;
	createBufferSrv(m_pDevice, m_srvHeap, rResource, rBufferInfo.numElements - firstElement, 0, rBufferInfo.eFormat, firstElement, &view.hViewD3D12);

	return view;
}

void RdrContextD3D12::ReleaseShaderResourceView(RdrShaderResourceView& resourceView)
{
	m_srvHeap.FreeDescriptor(resourceView.hViewD3D12);
	resourceView.pTypeless = nullptr;
}

RdrDepthStencilView RdrContextD3D12::CreateDepthStencilView(const RdrResource& rDepthTex)
{
	RdrDepthStencilView view;

	bool bIsMultisampled = (rDepthTex.texInfo.sampleCount > 1);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = getD3DDepthFormat(rDepthTex.texInfo.format);
	if (rDepthTex.texInfo.depth > 1)
	{
		if (bIsMultisampled)
		{
			dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
			dsvDesc.Texture2DMSArray.ArraySize = rDepthTex.texInfo.depth;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
		}
		else
		{
			dsvDesc.Texture2DArray.MipSlice = 0;
			dsvDesc.Texture2DArray.FirstArraySlice = 0;
			dsvDesc.Texture2DArray.ArraySize = rDepthTex.texInfo.depth;
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

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
	descHandle.ptr = m_dsvHeap.AllocateDescriptor();

	m_pDevice->CreateDepthStencilView(rDepthTex.pResource12, &dsvDesc, descHandle);
	view.hViewD3D12 = descHandle.ptr;

	return view;
}

RdrDepthStencilView RdrContextD3D12::CreateDepthStencilView(const RdrResource& rDepthTex, uint arrayStartIndex, uint arraySize)
{
	RdrDepthStencilView view;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = getD3DDepthFormat(rDepthTex.texInfo.format);

	if (rDepthTex.texInfo.sampleCount > 1)
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

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
	descHandle.ptr = m_dsvHeap.AllocateDescriptor();

	m_pDevice->CreateDepthStencilView(rDepthTex.pResource12, &dsvDesc, descHandle);
	view.hViewD3D12 = descHandle.ptr;

	return view;
}

void RdrContextD3D12::ReleaseDepthStencilView(const RdrDepthStencilView& depthStencilView)
{
	m_dsvHeap.FreeDescriptor(depthStencilView.hViewD3D12);
}

RdrRenderTargetView RdrContextD3D12::CreateRenderTargetView(RdrResource& rTexRes)
{
	RdrRenderTargetView view;

	D3D12_RENDER_TARGET_VIEW_DESC desc;
	desc.Format = getD3DFormat(rTexRes.texInfo.format);
	if (rTexRes.texInfo.sampleCount > 1)
	{
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
		desc.Texture2DMS.UnusedField_NothingToDefine = 0;
	}
	else
	{
		desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
	descHandle.ptr = m_rtvHeap.AllocateDescriptor();

	m_pDevice->CreateRenderTargetView(rTexRes.pResource12, &desc, descHandle);
	view.hViewD3D12 = descHandle.ptr;

	return view;
}

RdrRenderTargetView RdrContextD3D12::CreateRenderTargetView(RdrResource& rTexArrayRes, uint arrayStartIndex, uint arraySize)
{
	RdrRenderTargetView view;

	D3D12_RENDER_TARGET_VIEW_DESC desc;
	desc.Format = getD3DFormat(rTexArrayRes.texInfo.format);
	desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
	desc.Texture2DArray.MipSlice = 0;
	desc.Texture2DArray.FirstArraySlice = arrayStartIndex;
	desc.Texture2DArray.ArraySize = arraySize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
	descHandle.ptr = m_rtvHeap.AllocateDescriptor();

	m_pDevice->CreateRenderTargetView(rTexArrayRes.pResource12, &desc, descHandle);
	view.hViewD3D12 = descHandle.ptr;

	return view;
}

void RdrContextD3D12::ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView)
{
	m_rtvHeap.FreeDescriptor(renderTargetView.hViewD3D12);
}

void RdrContextD3D12::SetDepthStencilState(RdrDepthTestMode eDepthTest, bool bWriteEnabled)
{
	// DONOTCHECKIN - PSO
	/*
	if (!m_pDepthStencilStates[(int)eDepthTest * 2 + bWriteEnabled])
	{
		D3D12_DEPTH_STENCIL_DESC dsDesc;

		dsDesc.DepthEnable = (eDepthTest != RdrDepthTestMode::None);
		switch (eDepthTest)
		{
		case RdrDepthTestMode::Less:
			dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			break;
		case RdrDepthTestMode::Equal:
			dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
			break;
		default:
			dsDesc.DepthFunc = D3D12_COMPARISON_FUNC_NEVER;
			break;
		}
		dsDesc.DepthWriteMask = bWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;

		dsDesc.StencilEnable = false;

		dsDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_INCR;
		dsDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		dsDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		dsDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_DECR;
		dsDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
		dsDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		ID3D12DepthStencilState* pState;
		HRESULT hr = m_pDevice->CreateDepthStencilState(&dsDesc, &pState);
		assert(hr == S_OK);

		m_pDepthStencilStates[(int)eDepthTest] = pState;
	}

	m_pDevContext->OMSetDepthStencilState(m_pDepthStencilStates[(int)eDepthTest], 1);
	*/
}

void RdrContextD3D12::SetBlendState(RdrBlendMode blendMode)
{
	// DONOTCHECKIN - PSO
	/*
	if (!m_pBlendStates[(int)blendMode])
	{
		D3D12_BLEND_DESC desc = { 0 };
		desc.AlphaToCoverageEnable = false;
		desc.IndependentBlendEnable = false;

		switch (blendMode)
		{
		case RdrBlendMode::kOpaque:
			desc.RenderTarget[0].BlendEnable = false;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kAlpha:
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kAdditive:
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			break;
		case RdrBlendMode::kSubtractive:
			desc.RenderTarget[0].BlendEnable = true;
			desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
			desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
			desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_REV_SUBTRACT;
			desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			break;
		}

		ID3D12BlendState* pBlendState = nullptr;
		HRESULT hr = m_pDevice->CreateBlendState(&desc, &pBlendState);
		assert(hr == S_OK);

		m_pBlendStates[(int)blendMode] = pBlendState;
	}

	m_pDevContext->OMSetBlendState(m_pBlendStates[(int)blendMode], nullptr, 0xffffffff);
	*/
}

void RdrContextD3D12::SetRasterState(const RdrRasterState& rasterState)
{
	// DONOTCHECKIN - PSO
	/*
	uint rasterIndex =
		rasterState.bEnableMSAA * 2 * 2 * 2 * 2
		+ rasterState.bEnableScissor * 2 * 2 * 2
		+ rasterState.bWireframe * 2 * 2
		+ rasterState.bUseSlopeScaledDepthBias * 2
		+ rasterState.bDoubleSided;

	if (!m_pRasterStates[rasterIndex])
	{
		D3D12_RASTERIZER_DESC desc;
		desc.AntialiasedLineEnable = !rasterState.bEnableMSAA;
		desc.MultisampleEnable = rasterState.bEnableMSAA;
		desc.CullMode = rasterState.bDoubleSided ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK;
		desc.SlopeScaledDepthBias = rasterState.bUseSlopeScaledDepthBias ? m_slopeScaledDepthBias : 0.f;
		desc.DepthBiasClamp = 0.f;
		desc.DepthClipEnable = true;
		desc.FrontCounterClockwise = false;
		desc.ScissorEnable = rasterState.bEnableScissor;
		if (rasterState.bWireframe)
		{
			desc.FillMode = D3D12_FILL_MODE_WIREFRAME;
			desc.DepthBias = -2; // Wireframe gets a slight depth bias so z-fighting doesn't occur.
		}
		else
		{
			desc.FillMode = D3D12_FILL_MODE_SOLID;
			desc.DepthBias = 0;
		}

		ID3D12RasterizerState* pRasterState = nullptr;
		HRESULT hr = m_pDevice->CreateRasterizerState(&desc, &pRasterState);
		assert(hr == S_OK);

		m_pRasterStates[rasterIndex] = pRasterState;
	}

	m_pDevContext->RSSetState(m_pRasterStates[rasterIndex]);
	*/
}

void RdrContextD3D12::SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE aRtvs[8];
	assert(numTargets < ARRAYSIZE(aRtvs));

	for (uint i = 0; i < numTargets; ++i)
	{
		aRtvs[i].ptr = aRenderTargets[i].hViewD3D12;
	}
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE dtv;
	dtv.ptr = depthStencilTarget.hViewD3D12;

	m_pCommandList->OMSetRenderTargets(numTargets, aRtvs, false, &dtv);
}

void RdrContextD3D12::BeginEvent(LPCWSTR eventName)
{
	PIXBeginEvent(m_pCommandList.Get(), 0, eventName);
}

void RdrContextD3D12::EndEvent()
{
	PIXEndEvent(m_pCommandList.Get());
}

void RdrContextD3D12::Present()
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

	m_nFrameFenceValues[m_currBackBuffer] = Signal(m_pCommandQueue, m_pFence, m_nFenceValue);


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

	ResetDrawState();
	WaitForFenceValue(m_pFence, m_nFrameFenceValues[m_currBackBuffer], m_hFenceEvent);

	// Transition new backbuffer back into RTV state
	{
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_pBackBuffers[m_currBackBuffer].Get(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_pCommandList->ResourceBarrier(1, &barrier);
	}
}

void RdrContextD3D12::Resize(uint width, uint height)
{
	// Flush the GPU queue to make sure the swap chain's back buffers
	// are not being referenced by an in-flight command list.
	Flush(m_pCommandQueue, m_pFence, m_nFenceValue, m_hFenceEvent);

	for (int i = 0; i < kNumBackBuffers; ++i)
	{
		// Any references to the back buffers must be released
		// before the swap chain can be resized.
		m_pBackBuffers[i].Reset();
		m_nFrameFenceValues[i] = m_nFrameFenceValues[m_currBackBuffer];
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

#if 0 //donotcheckin
	if (m_pPrimaryRenderTarget)
		m_pPrimaryRenderTarget->Release();
	m_pSwapChain->ResizeBuffers(kNumBackBuffers, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

	ID3D12Texture2D* pBackBuffer;
	m_pSwapChain->GetBuffer(0, __uuidof(ID3D12Texture2D), (void**)&pBackBuffer);

	D3D12_TEXTURE2D_DESC bbDesc;
	pBackBuffer->GetDesc(&bbDesc);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = bbDesc.Format;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	HRESULT hr = m_pDevice->CreateRenderTargetView(pBackBuffer, &rtvDesc, &m_pPrimaryRenderTarget);
	assert(hr == S_OK);
	pBackBuffer->Release();
#endif
}

void RdrContextD3D12::ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor)
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv;
	rtv.ptr = renderTarget.hViewD3D12;
	m_pCommandList->ClearRenderTargetView(rtv, clearColor.asFloat4(), 0, nullptr);
}

void RdrContextD3D12::ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal)
{
	D3D12_CLEAR_FLAGS clearFlags = (D3D12_CLEAR_FLAGS)0;
	if (bClearDepth)
		clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
	if (bClearStencil)
		clearFlags |= D3D12_CLEAR_FLAG_STENCIL;

	CD3DX12_CPU_DESCRIPTOR_HANDLE dsv;
	dsv.ptr = depthStencil.hViewD3D12;
	m_pCommandList->ClearDepthStencilView(dsv, clearFlags, depthVal, stencilVal, 0, nullptr);
}

RdrRenderTargetView RdrContextD3D12::GetPrimaryRenderTarget()
{
	RdrRenderTargetView view;
	view.hViewD3D12 = m_hBackBufferRtvs[m_currBackBuffer];
	return view;
}

void RdrContextD3D12::SetViewport(const Rect& viewport)
{
	D3D12_VIEWPORT d3dViewport;
	d3dViewport.TopLeftX = viewport.left;
	d3dViewport.TopLeftY = viewport.top;
	d3dViewport.Width = viewport.width;
	d3dViewport.Height = viewport.height;
	d3dViewport.MinDepth = 0.f;
	d3dViewport.MaxDepth = 1.f;
	m_pCommandList->RSSetViewports(1, &d3dViewport);
}

RdrConstantBufferDeviceObj RdrContextD3D12::CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	RdrConstantBufferDeviceObj devObj;

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

	CD3DX12_HEAP_PROPERTIES heapProperties = selectHeapProperties(cpuAccessFlags, eUsage);
	HRESULT hr = m_pDevice->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&devObj.pBufferD3D12)); //donotcheckin - manage ref count?

	if (FAILED(hr))
	{
		assert(false);
		return devObj;
	}

	CD3DX12_CPU_DESCRIPTOR_HANDLE descHandle;
	descHandle.ptr = m_srvHeap.AllocateDescriptor();

	// Describe and create a constant buffer view.
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = devObj.pBufferD3D12->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = (size + 255) & ~255;	// CB size is required to be 256-byte aligned.
	m_pDevice->CreateConstantBufferView(&cbvDesc, descHandle);

	if (pData)
	{
		UpdateConstantBuffer(devObj, pData, size);
	}

	return devObj;
}

void RdrContextD3D12::UpdateConstantBuffer(const RdrConstantBufferDeviceObj& buffer, const void* pSrcData, const uint dataSize)
{
	void* pDstData;
	CD3DX12_RANGE readRange(0, 0);
	HRESULT hr = buffer.pBufferD3D12->Map(0, &readRange, &pDstData);
	if (FAILED(hr))
	{
		assert(false);
		return;
	}

	memcpy(pDstData, pSrcData, dataSize);

	CD3DX12_RANGE writeRange(0, dataSize);
	buffer.pBufferD3D12->Unmap(0, &writeRange);
}

void RdrContextD3D12::ReleaseConstantBuffer(const RdrConstantBufferDeviceObj& buffer)
{
	buffer.pBufferD3D12->Release();
}

// Compares array data between src and dst, copying differences to dst and returning the range of changed elements.
template<typename DataT>
static inline bool updateDataList(const DataT* aSrcData, DataT* aDstData, uint count, int* pOutFirstChanged, int* pOutLastChanged)
{
	int firstChanged = -1;
	int lastChanged = -1;
	for (uint i = 0; i < count; ++i)
	{
		if (aSrcData[i] != aDstData[i])
		{
			aDstData[i] = aSrcData[i];
			lastChanged = i;
			if (firstChanged < 0)
				firstChanged = i;
		}
	}

	*pOutFirstChanged = firstChanged;
	*pOutLastChanged = lastChanged;
	return (firstChanged >= 0);
}

void RdrContextD3D12::Draw(const RdrDrawState& rDrawState, uint instanceCount)
{
	// Considerations:
	//	* Shader byte code needs to be stored on CPU
	//	* Account for different RTV/DSV modes
	//	donotcheckin
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc;
	desc.BlendState;
	desc.DepthStencilState;
	desc.DS;
	desc.DSVFormat;
	desc.Flags;
	desc.GS;
	desc.HS;
	desc.InputLayout;
	desc.NodeMask;
	desc.NumRenderTargets;
	desc.PrimitiveTopologyType;
	desc.pRootSignature;
	desc.PS;
	desc.RasterizerState;
	desc.RTVFormats;
	desc.SampleDesc;
	desc.SampleMask;
	desc.VS;
	ComPtr<ID3D12PipelineState> pPipelineState;
	m_pDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pPipelineState));


	CD3DX12_ROOT_PARAMETER rootParameters[2];
	rootParameters[0].InitAsConstants(4, 0);
	rootParameters[1].InitAsDescriptorTable(1, &ranges[0]);

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

	m_pDevice->CreateRootSignature(0, );


	// donotcheckin?
	static_assert(sizeof(RdrBuffer) == sizeof(ID3D12Resource*), "RdrBuffer must only contain the device obj pointer.");
	static_assert(sizeof(RdrConstantBufferDeviceObj) == sizeof(ID3D12Resource*), "RdrConstantBufferDeviceObj must only contain the device obj pointer.");
	static_assert(sizeof(RdrShaderResourceView) == sizeof(ID3D12ShaderResourceView*), "RdrVertexBuffer must only contain the device obj pointer.");

	int firstChanged = -1;
	int lastChanged = -1;

	// donotcheckin
	// set root signature, if necessary
	// bind pso
	
	// Vertex buffers
	if (updateDataList<RdrBuffer>(rDrawState.vertexBuffers, m_drawState.vertexBuffers,
		rDrawState.vertexBufferCount, &firstChanged, &lastChanged))
	{
		D3D12_VERTEX_BUFFER_VIEW views[RdrDrawState::kMaxVertexBuffers];
		uint numViews = lastChanged - firstChanged + 1;
		for (uint i = 0; i < numViews; ++i)
		{
			views[i].BufferLocation = rDrawState.vertexBuffers[firstChanged + i].pBuffer12->GetGPUVirtualAddress();
			views[i].BufferLocation += rDrawState.vertexOffsets[firstChanged + i];
			views[i].SizeInBytes = 0; //donotcheckin
			views[i].StrideInBytes = rDrawState.vertexStrides[firstChanged + i];
		}

		m_pCommandList->IASetVertexBuffers(firstChanged, lastChanged - firstChanged + 1, views);
		m_rProfiler.IncrementCounter(RdrProfileCounter::VertexBuffer);
	}

	// VS constants
	if (updateDataList<RdrConstantBufferDeviceObj>(rDrawState.vsConstantBuffers, m_drawState.vsConstantBuffers,
		rDrawState.vsConstantBufferCount, &firstChanged, &lastChanged))
	{
		m_pCommandList->VSSetConstantBuffers(firstChanged, lastChanged - firstChanged + 1, (ID3D12Buffer**)rDrawState.vsConstantBuffers + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsConstantBuffer);
	}

	// VS resources
	if (updateDataList<RdrShaderResourceView>(rDrawState.vsResources, m_drawState.vsResources,
		rDrawState.vsResourceCount, &firstChanged, &lastChanged))
	{
		m_pCommandList->VSSetShaderResources(firstChanged, lastChanged - firstChanged + 1, (ID3D12ShaderResourceView**)rDrawState.vsResources + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsResource);
	}

	// GS constants
	if (updateDataList<RdrConstantBufferDeviceObj>(rDrawState.gsConstantBuffers, m_drawState.gsConstantBuffers,
		rDrawState.gsConstantBufferCount, &firstChanged, &lastChanged))
	{
		m_pCommandList->GSSetConstantBuffers(firstChanged, lastChanged - firstChanged + 1, (ID3D12Buffer**)rDrawState.gsConstantBuffers + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::GsConstantBuffer);
	}


	if (rDrawState.pDomainShader)
	{
		if (updateDataList<RdrConstantBufferDeviceObj>(rDrawState.dsConstantBuffers, m_drawState.dsConstantBuffers,
			rDrawState.dsConstantBufferCount, &firstChanged, &lastChanged))
		{
			m_pCommandList->DSSetConstantBuffers(firstChanged, lastChanged - firstChanged + 1, (ID3D12Buffer**)rDrawState.dsConstantBuffers + firstChanged);
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsConstantBuffer);
		}

		if (updateDataList<RdrShaderResourceView>(rDrawState.dsResources, m_drawState.dsResources,
			rDrawState.dsResourceCount, &firstChanged, &lastChanged))
		{
			m_pCommandList->DSSetShaderResources(firstChanged, lastChanged - firstChanged + 1, (ID3D12ShaderResourceView**)rDrawState.dsResources + firstChanged);
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsResource);
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

	// PS constants
	if (updateDataList<RdrConstantBufferDeviceObj>(rDrawState.psConstantBuffers, m_drawState.psConstantBuffers,
		rDrawState.psConstantBufferCount, &firstChanged, &lastChanged))
	{
		m_pCommandList->PSSetConstantBuffers(firstChanged, lastChanged - firstChanged + 1, (ID3D12Buffer**)rDrawState.psConstantBuffers + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsConstantBuffer);
	}

	// PS resources
	if (updateDataList<RdrShaderResourceView>(rDrawState.psResources, m_drawState.psResources,
		rDrawState.psResourceCount, &firstChanged, &lastChanged))
	{
		m_pCommandList->PSSetShaderResources(firstChanged, lastChanged - firstChanged + 1, (ID3D12ShaderResourceView**)rDrawState.psResources + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsResource);
	}

	// PS samplers
	if (updateDataList<RdrSamplerState>(rDrawState.psSamplers, m_drawState.psSamplers,
		rDrawState.psSamplerCount, &firstChanged, &lastChanged))
	{
		ID3D12SamplerState* apSamplers[ARRAY_SIZE(rDrawState.psSamplers)] = { 0 };
		for (int i = firstChanged; i <= lastChanged; ++i)
		{
			apSamplers[i] = GetSampler(rDrawState.psSamplers[i]);
		}
		m_pCommandList->PSSetSamplers(firstChanged, lastChanged - firstChanged + 1, apSamplers + firstChanged);
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsSamplers);
	}

	if (rDrawState.indexBuffer.pBuffer)
	{
		if (rDrawState.indexBuffer.pBuffer != m_drawState.indexBuffer.pBuffer)
		{
			D3D12_INDEX_BUFFER_VIEW view;
			view.BufferLocation = rDrawState.indexBuffer.pBuffer12->GetGPUVirtualAddress();
			view.Format = DXGI_FORMAT_R16_UINT;
			view.SizeInBytes = 0;//donotcheckin

			m_pCommandList->IASetIndexBuffer(&view);
			m_drawState.indexBuffer.pBuffer = rDrawState.indexBuffer.pBuffer;
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

void RdrContextD3D12::DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ)
{
	PSClearResources();
	ResetDrawState();

	m_pCommandList->CSSetShader(rDrawState.pComputeShader->pCompute, nullptr, 0);

	m_pCommandList->CSSetConstantBuffers(0, rDrawState.csConstantBufferCount, (ID3D12Buffer**)rDrawState.csConstantBuffers);

	uint numResources = ARRAY_SIZE(rDrawState.csResources);
	for (uint i = 0; i < numResources; ++i)
	{
		ID3D12ShaderResourceView* pResource = rDrawState.csResources[i].pViewD3D12;
		m_pCommandList->CSSetShaderResources(i, 1, &pResource);
	}

	uint numSamplers = ARRAY_SIZE(rDrawState.csSamplers);
	for (uint i = 0; i < numSamplers; ++i)
	{
		ID3D12SamplerState* pSampler = GetSampler(rDrawState.csSamplers[i]);
		m_pCommandList->CSSetSamplers(i, 1, &pSampler);
	}

	uint numUavs = ARRAY_SIZE(rDrawState.csUavs);
	for (uint i = 0; i < numUavs; ++i)
	{
		ID3D12UnorderedAccessView* pView = rDrawState.csUavs[i].pViewD3D12;
		m_pCommandList->CSSetUnorderedAccessViews(i, 1, &pView, nullptr);
	}

	m_pCommandList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);

	// Clear resources to avoid binding errors (input bound as output).  todo: don't do this
	for (uint i = 0; i < numResources; ++i)
	{
		ID3D12ShaderResourceView* pResourceView = nullptr;
		m_pCommandList->CSSetShaderResources(i, 1, &pResourceView);
	}
	for (uint i = 0; i < numUavs; ++i)
	{
		ID3D12UnorderedAccessView* pUnorderedAccessView = nullptr;
		m_pCommandList->CSSetUnorderedAccessViews(i, 1, &pUnorderedAccessView, nullptr);
	}

	ResetDrawState();
}

void RdrContextD3D12::PSClearResources()
{
	memset(m_drawState.psResources, 0, sizeof(m_drawState.psResources));

	ID3D12ShaderResourceView* resourceViews[ARRAY_SIZE(RdrDrawState::psResources)] = { 0 };
	m_pCommandList->PSSetShaderResources(0, 20, resourceViews);
}

RdrQuery RdrContextD3D12::CreateQuery(RdrQueryType eType)
{
	RdrQuery query;

	switch (eType)
	{
	case RdrQueryType::Timestamp:
		query.hQueryD3D12 = m_timestampQueryHeap.AllocateQuery();
		break;
	case RdrQueryType::Disjoint:
		assert(false);//donotcheckin
		break;
	}

	return query;
}

void RdrContextD3D12::ReleaseQuery(RdrQuery& rQuery)
{
	switch (rQuery.hQueryD3D12.nType)
	{
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		m_timestampQueryHeap.FreeQuery(rQuery.hQueryD3D12);
		break;
	}
}

void RdrContextD3D12::BeginQuery(RdrQuery& rQuery)
{
	switch (rQuery.hQueryD3D12.nType)
	{
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		// Timestamp queries don't have a begin.
		break;
	}
}

void RdrContextD3D12::EndQuery(RdrQuery& rQuery)
{
	switch (rQuery.hQueryD3D12.nType)
	{
	case D3D12_QUERY_HEAP_TYPE_TIMESTAMP:
		m_pCommandList->EndQuery(m_timestampQueryHeap.GetHeap(), D3D12_QUERY_TYPE_TIMESTAMP, rQuery.hQueryD3D12.nId);
		break;
	}
}

uint64 RdrContextD3D12::GetTimestampQueryData(RdrQuery& rQuery)
{
	uint64 timestamp = -1;
	uint64 nBufferOffset = rQuery.hQueryD3D12.nId * sizeof(timestamp);
	m_pCommandList->ResolveQueryData(m_timestampQueryHeap.GetHeap(), // donotcheckin - do at end of frame and hit all the queries at once?
		D3D12_QUERY_TYPE_TIMESTAMP, 
		rQuery.hQueryD3D12.nId, 
		1, 
		m_timestampResultBuffer.Get(), 
		nBufferOffset);

	D3D12_RANGE readRange = {nBufferOffset, nBufferOffset + sizeof(timestamp)};
	uint64* pTimestampData = nullptr;
	HRESULT hr = m_timestampResultBuffer->Map(0, &readRange, (void**)*pTimestampData);
	if (hr == S_OK)
	{
		timestamp = pTimestampData[0];
	}

	D3D12_RANGE writeRange = { 0, 0 };
	m_timestampResultBuffer->Unmap(0, &writeRange);

	return timestamp;
}

RdrQueryDisjointData RdrContextD3D12::GetDisjointQueryData(RdrQuery& rQuery)
{
	RdrQueryDisjointData data;

	m_pCommandQueue->GetTimestampFrequency(&data.frequency);
	data.isDisjoint = false;//donotcheckin !!d3dData.Disjoint;

	return data;
}

void RdrContextD3D12::ResetDrawState()
{
	memset(&m_drawState, -1, sizeof(m_drawState));
	m_currBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();
}