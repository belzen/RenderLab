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
	static constexpr uint kMaxRootVsConstantBufferViews = 4;
	static constexpr uint kMaxRootVsShaderResourceViews = 1;
	static constexpr uint kMaxRootPsShaderResourceViews = 19;
	static constexpr uint kMaxRootPsSamplers = 16;
	static constexpr uint kMaxRootPsConstantBufferViews = 4;

	enum ERootDescriptorTables
	{
		kRootVsGlobalConstantBufferTable,
		kRootVsPerObjectConstantBufferTable,
		kRootVsShaderResourceViewTable,
		kRootPsMaterialShaderResourceViewTable,
		kRootPsGlobalShaderResourceViewTable,
		kRootPsMaterialSamplerTable,
		kRootPsGlobalSamplerTable,
		kRootPsGlobalConstantBufferTable,
		kRootPsMaterialConstantBufferTable,
		kRootDsShaderResourceViewTable,
		kRootDsSamplerTable,
		kRootDsGlobalConstantBufferTable,
		kRootDsPerObjectConstantBufferTable,
		
		kNumRootDescriptorTables
	};

	static constexpr uint kRootCsConstantBufferTable		= 0;
	static constexpr uint kRootCsShaderResourceViewTable	= 1;
	static constexpr uint kRootCsSamplerTable				= 2;
	static constexpr uint kRootCsUnorderedAccessViewTable	= 3;
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
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to get adapter!"))
		return nullptr;

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

			if (!ValidateHResult(hr, __FUNCTION__, "Failed to create device!"))
				return nullptr;
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

	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create device!"))
		return nullptr;

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

	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create command queue!"))
		return nullptr;

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

	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create DXGI factory!"))
		return nullptr;

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

	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create swap chain!"))
		return nullptr;

	// Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
	// will be handled manually.
	hr = dxgiFactory4->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to make window association!"))
		return nullptr;

	hr = swapChain1.As(&dxgiSwapChain4);
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to setup swap chain!"))
		return nullptr;

	return dxgiSwapChain4;
}

void DescriptorHeap::Create(ComPtr<ID3D12Device> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, const uint* anDescriptorTableSizes, uint numDescriptorTableSizes, bool bShaderVisible)
{
	m_pDevice = pDevice.Get();
	m_heapType = type;

	uint nTotalNumDescriptors = 0;
	for (uint i = 0; i < numDescriptorTableSizes; ++i)
		nTotalNumDescriptors += anDescriptorTableSizes[i] * (1 << i);

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = nTotalNumDescriptors;
	desc.Type = type;
	desc.NodeMask = 0;
	desc.Flags = (bShaderVisible && (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER))
		? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap));
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create descriptor heap!"))
		return;

	m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(type);

	assert(numDescriptorTableSizes == ARRAYSIZE(m_tables));
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDesc(m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDesc(m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	for (uint iTableSize = 0; iTableSize < ARRAYSIZE(m_tables); iTableSize++)
	{
		DescriptorTableList& tables = m_tables[iTableSize];
		uint numTables = anDescriptorTableSizes[iTableSize];
		tables.resize(numTables);

		uint nTableSize = (1 << iTableSize);
		for (uint iTable = 0; iTable < numTables; ++iTable)
		{
			tables[iTable].hCpuDesc = hCpuDesc;
			tables[iTable].hGpuDesc = hGpuDesc;
			tables[iTable].bInUse = false;
			tables[iTable].bShaderVisible = bShaderVisible;
			tables[iTable].nTableSize = nTableSize;
			tables[iTable].nTableListIndex = iTableSize;

			hCpuDesc.Offset(nTableSize, m_descriptorSize);
			hGpuDesc.Offset(nTableSize, m_descriptorSize);
		}
	}


	//////////////////////////////////////////////////////////////////////////
	if (bShaderVisible)
	{
		uint numCopyDescriptors = anDescriptorTableSizes[0];
		m_copyDescriptors.resize(numCopyDescriptors);

		D3D12_DESCRIPTOR_HEAP_DESC copyDesc = {};
		copyDesc.NumDescriptors = numCopyDescriptors;
		copyDesc.Type = type;
		copyDesc.NodeMask = 0;
		copyDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		HRESULT hr = pDevice->CreateDescriptorHeap(&copyDesc, IID_PPV_ARGS(&m_pCopyDescriptorHeap));
		if (!ValidateHResult(hr, __FUNCTION__, "Failed to create copy descriptor heap!"))
			return;

		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDesc(m_pCopyDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDesc(m_pCopyDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		for (D3D12DescriptorHandles& handles : m_copyDescriptors)
		{
			handles.hCpuDesc = hCpuDesc;
			handles.hGpuDesc = hGpuDesc;
			handles.bInUse = false;
			handles.bShaderVisible = false;
			handles.nTableSize = 1;
			handles.nTableListIndex = 0;

			hCpuDesc.Offset(1, m_descriptorSize);
			hGpuDesc.Offset(1, m_descriptorSize);
		}
	}
}

void DescriptorHeap::Cleanup()
{
	m_pDescriptorHeap->Release();
}

D3D12DescriptorHandles DescriptorHeap::AllocateDescriptor()
{
	AutoScopedLock lock(m_mutex);

	D3D12DescriptorHandles handles = m_tables[0].back();
	m_tables[0].pop_back();

	handles.bInUse = true;

	return handles;
}

D3D12DescriptorHandles DescriptorHeap::AllocateCopyDescriptor()
{
	assert(!m_copyDescriptors.empty());

	AutoScopedLock lock(m_mutex);

	D3D12DescriptorHandles handles = m_copyDescriptors.back();
	m_copyDescriptors.pop_back();

	handles.bInUse = true;
	return handles;
}

D3D12DescriptorHandles DescriptorHeap::CreateDescriptorTable(const D3D12DescriptorHandles* pSrcDescriptors, uint size)
{
	AutoScopedLock lock(m_mutex);

	if (size == 0)
	{
		return D3D12DescriptorHandles();
	}

	uint nTable = 0;
	while (size > (uint)(1 << nTable))
	{
		nTable++;
	}

	if (nTable > ARRAYSIZE(m_tables) || m_tables[nTable].empty())
	{
		assert(false);
		return D3D12DescriptorHandles();
	}

	D3D12DescriptorHandles handles = m_tables[nTable].back();
	m_tables[nTable].pop_back();

	assert(!handles.bInUse);
	assert(handles.nTableListIndex == nTable);
	handles.bInUse = true;

	// Copy descriptors
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescDst = handles.hCpuDesc;
	for (uint i = 0; i < size; ++i)
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescSrc = pSrcDescriptors[i].hCpuDesc;
		assert(!pSrcDescriptors[i].bShaderVisible);

		m_pDevice->CopyDescriptorsSimple(1, hDescDst, hDescSrc, m_heapType);
		hDescDst.Offset(1, m_descriptorSize);
	}
	
	return handles;
}

void DescriptorHeap::FreeDescriptor(D3D12DescriptorHandles desc)
{
	AutoScopedLock lock(m_mutex);

	assert(desc.bInUse);
	desc.bInUse = false;

	if (desc.bShaderVisible)
	{
		m_tables[desc.nTableListIndex].push_back(desc);
	}
	else
	{
		m_copyDescriptors.push_back(desc);
	}
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
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create descriptor heap!"))
		return;

	m_descriptorSize = pDevice->GetDescriptorHandleIncrementSize(type);
	m_nextDescriptor = 0;
	m_maxDescriptors = nMaxDescriptors;

	m_nFrame = 0;
	memset(m_nFrameStartDescriptor, 0, sizeof(m_nFrameStartDescriptor)); 
	memset(m_nFrameDescriptorCount, 0, sizeof(m_nFrameDescriptorCount));
}

void DescriptorRingBuffer::Cleanup()
{
	m_pDescriptorHeap->Release();
}

void DescriptorRingBuffer::BeginFrame()
{
	m_nFrame = (m_nFrame + 1) % ARRAYSIZE(m_nFrameStartDescriptor);
	m_nFrameStartDescriptor[m_nFrame] = m_nextDescriptor;
	m_nFrameDescriptorCount[m_nFrame] = 0;

	m_nFrameAvailableDescriptors = m_maxDescriptors;
	for (uint nDescriptorCount : m_nFrameDescriptorCount)
	{
		m_nFrameAvailableDescriptors -= nDescriptorCount;
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorRingBuffer::AllocateDescriptors(uint numToAllocate, uint& nOutDescriptorStartIndex)
{
	if (m_nextDescriptor + numToAllocate >= m_maxDescriptors)
	{
		uint nSkipped = (m_maxDescriptors - m_nextDescriptor);
		m_nFrameDescriptorCount[m_nFrame] += nSkipped;

		m_nextDescriptor = 0;
	}

	if (m_nFrameDescriptorCount[m_nFrame] + numToAllocate > m_nFrameAvailableDescriptors)
	{
		assert(false);
	}

	m_nFrameDescriptorCount[m_nFrame] += numToAllocate;

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
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create query heap!"))
		return;

	m_nNextQuery = 0;
	m_nFrame = 0;
	m_nMaxQueries = nMaxQueries;
	m_type = type;
}

void QueryHeap::Cleanup()
{
	m_pQueryHeap->Release();
}

void QueryHeap::BeginFrame()
{
	m_nFrameQueryStartEnd[m_nFrame].first = m_nNextQuery;
}

QueryHeap::TQueryRange QueryHeap::EndFrame()
{
	TQueryRange& frameRange = m_nFrameQueryStartEnd[m_nFrame];
	frameRange.second = m_nNextQuery;

	m_nFrame = (m_nFrame + 1) % ARRAYSIZE(m_nFrameQueryStartEnd);

	return frameRange;
}

RdrQuery QueryHeap::AllocateQuery()
{
	RdrQuery handle;
	handle.nId = m_nNextQuery;

	++m_nNextQuery;
	if (m_nNextQuery >= m_nMaxQueries)
	{
		m_nNextQuery = 0;
	}

	return handle;
}

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device> device,
	D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandAllocator> commandAllocator;
	HRESULT hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator));
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create command allocator!"))
		return false;

	return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device> device,
	ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;
	HRESULT hr = device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create command list!"))
		return nullptr;

	hr = commandList->Close();
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to close command list!"))
		return nullptr;

	return commandList;
}

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device> device)
{
	ComPtr<ID3D12Fence> fence;

	HRESULT hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create fence!"))
		return nullptr;

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
	ValidateHResult(hr, __FUNCTION__, "Failed to signal fence!");
}

void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent)
{
	if (fence->GetCompletedValue() < fenceValue)
	{
		HRESULT hr = fence->SetEventOnCompletion(fenceValue, fenceEvent);
		if (!ValidateHResult(hr, __FUNCTION__, "Failed to set event!"))
			return;

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
		if (!ValidateHResult(hr, __FUNCTION__, "Failed to get swap chain buffer!"))
			return;

		if (!m_hBackBufferRtvs[i].IsValid())
		{
			m_hBackBufferRtvs[i] = m_rtvHeap.AllocateDescriptor();
		}

		m_pDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, m_hBackBufferRtvs[i].hCpuDesc);
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

	static constexpr uint akMaxNumRenderTargetViews[] = { 64, 0, 0, 0, 0 };
	m_rtvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, akMaxNumRenderTargetViews, ARRAYSIZE(akMaxNumRenderTargetViews), false);

	static constexpr uint akMaxNumDepthStencilViews[] = { 64, 0, 0, 0, 0 };
	m_dsvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, akMaxNumDepthStencilViews, ARRAYSIZE(akMaxNumDepthStencilViews), false);

	static constexpr uint akMaxNumShaderResourceViews[] = { 8000, 1024, 2048, 2048, 0 };
	m_srvHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, akMaxNumShaderResourceViews, ARRAYSIZE(akMaxNumShaderResourceViews), true);

	static constexpr uint akMaxNumSamplers[] = { 512, 256, 128, 64, 0 };
	m_samplerHeap.Create(m_pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, akMaxNumSamplers, ARRAYSIZE(akMaxNumSamplers), true);

	for (int i = 0; i < kNumBackBuffers; ++i)
	{
		m_pCommandAllocators[i] = CreateCommandAllocator(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
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

	if (!ValidateHResult(hr, __FUNCTION__, "Failed to create timestamp buffer!"))
		return false;

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
		CD3DX12_DESCRIPTOR_RANGE ranges[kNumRootDescriptorTables];
		ranges[kRootVsGlobalConstantBufferTable].Init(		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,		1, 0);	// Vertex constants (GLOBAL)
		ranges[kRootVsPerObjectConstantBufferTable].Init(	D3D12_DESCRIPTOR_RANGE_TYPE_CBV,		1, 1);	// Vertex constants (PER-OBJECT)
		ranges[kRootVsShaderResourceViewTable].Init(		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,		1, 0);	// Vertex buffers
		ranges[kRootPsMaterialShaderResourceViewTable].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,		5, 0);	// Pixel resources (MATERIAL)
		ranges[kRootPsGlobalShaderResourceViewTable].Init(	D3D12_DESCRIPTOR_RANGE_TYPE_SRV,		8, 11);	// Pixel resources (GLOBAL)
		ranges[kRootPsMaterialSamplerTable].Init(			D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,	3, 0);  // Pixel samplers (MATERIAL)
		ranges[kRootPsGlobalSamplerTable].Init(				D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,	2, 14); // Pixel samplers (GLOBAL)
		ranges[kRootPsGlobalConstantBufferTable].Init(		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,		3, 0);	// Pixel constants (GLOBAL)
		ranges[kRootPsMaterialConstantBufferTable].Init(	D3D12_DESCRIPTOR_RANGE_TYPE_CBV,		1, 3);	// Pixel constants (MATERIAL)
		ranges[kRootDsShaderResourceViewTable].Init(		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,		4, 0);	// Domain resources
		ranges[kRootDsSamplerTable].Init(					D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,	4, 0);  // Domain samplers
		ranges[kRootDsGlobalConstantBufferTable].Init(		D3D12_DESCRIPTOR_RANGE_TYPE_CBV,		1, 0);	// Domain constants (GLOBAL)
		ranges[kRootDsPerObjectConstantBufferTable].Init(	D3D12_DESCRIPTOR_RANGE_TYPE_CBV,		1, 1);	// Domain constants (PER-OBJECT)

		CD3DX12_ROOT_PARAMETER rootParameters[kNumRootDescriptorTables];
		rootParameters[kRootVsGlobalConstantBufferTable].InitAsDescriptorTable(			1, &ranges[kRootVsGlobalConstantBufferTable], D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[kRootVsPerObjectConstantBufferTable].InitAsDescriptorTable(		1, &ranges[kRootVsPerObjectConstantBufferTable], D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[kRootVsShaderResourceViewTable].InitAsDescriptorTable(			1, &ranges[kRootVsShaderResourceViewTable], D3D12_SHADER_VISIBILITY_VERTEX);
		rootParameters[kRootPsMaterialShaderResourceViewTable].InitAsDescriptorTable(	1, &ranges[kRootPsMaterialShaderResourceViewTable], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[kRootPsGlobalShaderResourceViewTable].InitAsDescriptorTable(		1, &ranges[kRootPsGlobalShaderResourceViewTable], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[kRootPsMaterialSamplerTable].InitAsDescriptorTable(				1, &ranges[kRootPsMaterialSamplerTable], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[kRootPsGlobalSamplerTable].InitAsDescriptorTable(				1, &ranges[kRootPsGlobalSamplerTable], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[kRootPsGlobalConstantBufferTable].InitAsDescriptorTable(			1, &ranges[kRootPsGlobalConstantBufferTable], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[kRootPsMaterialConstantBufferTable].InitAsDescriptorTable(		1, &ranges[kRootPsMaterialConstantBufferTable], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[kRootDsShaderResourceViewTable].InitAsDescriptorTable(			1, &ranges[kRootDsShaderResourceViewTable], D3D12_SHADER_VISIBILITY_DOMAIN);
		rootParameters[kRootDsSamplerTable].InitAsDescriptorTable(						1, &ranges[kRootDsSamplerTable], D3D12_SHADER_VISIBILITY_DOMAIN);
		rootParameters[kRootDsGlobalConstantBufferTable].InitAsDescriptorTable(			1, &ranges[kRootDsGlobalConstantBufferTable], D3D12_SHADER_VISIBILITY_DOMAIN);
		rootParameters[kRootDsPerObjectConstantBufferTable].InitAsDescriptorTable(		1, &ranges[kRootDsPerObjectConstantBufferTable], D3D12_SHADER_VISIBILITY_DOMAIN);

		// Allow input layout and deny unnecessary access to certain pipeline stages.
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
#endif
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);


		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		if (!ValidateHResult(hr, __FUNCTION__, "Failed to serialize root signature!", error))
			return false;

		hr = m_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pGraphicsRootSignature));
		ValidateHResult(hr, __FUNCTION__, "Failed to graphics root signature!");
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
		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
			| D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		if (!ValidateHResult(hr, __FUNCTION__, "Failed to serialize root signature!", error))
			return false;

		hr = m_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pComputeRootSignature));
		if (!ValidateHResult(hr, __FUNCTION__, "Failed to create compute root signature!"))
			return false;
	}

	// Null resources
	{
		// RTV
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;

		m_nullRenderTargetView.hView = m_rtvHeap.AllocateDescriptor();
		m_nullRenderTargetView.pResource = nullptr;
		m_pDevice->CreateRenderTargetView(nullptr, &rtvDesc, m_nullRenderTargetView.hView.hCpuDesc);

		// DSV
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.Texture2D.MipSlice = 0;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		m_nullDepthStencilView.hView = m_dsvHeap.AllocateDescriptor();
		m_nullDepthStencilView.pResource = nullptr;
		m_pDevice->CreateDepthStencilView(nullptr, &dsvDesc, m_nullDepthStencilView.hView.hCpuDesc);

		// SRV
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 0;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.f;

		m_nullShaderResourceView.pResource = nullptr;
		m_nullShaderResourceView.hCopyableView = m_srvHeap.AllocateCopyDescriptor();
		m_nullShaderResourceView.hShaderVisibleView = m_srvHeap.AllocateDescriptor();
		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, m_nullShaderResourceView.hCopyableView.hCpuDesc);
		m_pDevice->CreateShaderResourceView(nullptr, &srvDesc, m_nullShaderResourceView.hShaderVisibleView.hCpuDesc);

		// UAV
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		m_nullUnorderedAccessView.pResource = nullptr;
		m_nullUnorderedAccessView.hCopyableView = m_srvHeap.AllocateCopyDescriptor();
		m_nullUnorderedAccessView.hShaderVisibleView = m_srvHeap.AllocateDescriptor();
		m_pDevice->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_nullUnorderedAccessView.hCopyableView.hCpuDesc);
		m_pDevice->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, m_nullUnorderedAccessView.hShaderVisibleView.hCpuDesc);

		// CBV
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = 0;
		cbvDesc.SizeInBytes = 0;

		m_nullConstantBufferView.pResource = nullptr;
		m_nullConstantBufferView.hCopyableView = m_srvHeap.AllocateCopyDescriptor();
		m_nullConstantBufferView.hShaderVisibleView = m_srvHeap.AllocateDescriptor();
		m_pDevice->CreateConstantBufferView(&cbvDesc, m_nullConstantBufferView.hCopyableView.hCpuDesc);
		m_pDevice->CreateConstantBufferView(&cbvDesc, m_nullConstantBufferView.hShaderVisibleView.hCpuDesc);
	}

	Resize(width, height);
	m_drawState.Reset();
	return true;
}

void RdrContext::Release()
{
	if (g_userConfig.debugDevice)
	{
		ComPtr<ID3D12DebugDevice> pDebugDevice;
		if (SUCCEEDED(m_pDevice->QueryInterface(IID_PPV_ARGS(&pDebugDevice))))
		{
			HRESULT hr = pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY);
			assert(hr == S_OK);
		}
	}

	// Wait until the active frame is done.
	WaitForFenceValue(m_pFence, m_nFrameNum - 1, m_hFenceEvent);

	m_drawState.Reset();

	for (uint i = 0; i < kNumBackBuffers; ++i)
	{
		m_rtvHeap.FreeDescriptor(m_hBackBufferRtvs[i]);
		m_pBackBuffers[i]->Release();
		m_pCommandAllocators[i]->Release();
	}

	m_pFence->Release();

	m_rtvHeap.FreeDescriptor(m_nullRenderTargetView.hView);
	m_dsvHeap.FreeDescriptor(m_nullDepthStencilView.hView);

	m_rtvHeap.Cleanup();
	m_samplerHeap.Cleanup();
	m_srvHeap.Cleanup();
	m_dsvHeap.Cleanup();

	m_timestampQueryHeap.Cleanup();
	m_timestampResultBuffer->Release();
	m_pDebug->Release();
	m_pGraphicsRootSignature->Release();
	m_pComputeRootSignature->Release();
	m_pSwapChain->Release();
	m_pCommandList->Release();
	m_pCommandQueue->Release();
	m_pDevice->Release();
}

bool RdrContext::IsIdle()
{
	return (m_presentFlags & DXGI_PRESENT_TEST);
}

const RdrSampler& RdrContext::GetSampler(const RdrSamplerState& state)
{
	uint samplerIndex =
		state.cmpFunc * ((uint)RdrTexCoordMode::Count * 2)
		+ state.texcoordMode * 2
		+ state.bPointSample;

	RdrSampler& sampler = m_samplers[samplerIndex];
	if (!sampler.hCopyable.IsValid())
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

		sampler.hCopyable = m_samplerHeap.AllocateCopyDescriptor();
		sampler.hShaderVisible = m_samplerHeap.AllocateDescriptor();
		m_pDevice->CreateSampler(&desc, sampler.hCopyable.hCpuDesc);
		m_pDevice->CreateSampler(&desc, sampler.hShaderVisible.hCpuDesc);
	}

	return sampler;
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
	m_pDevice->CreateDepthStencilView(rDepthTex.GetResource(), &dsvDesc, view.hView.hCpuDesc);

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
	m_pDevice->CreateDepthStencilView(rDepthTex.GetResource(), &dsvDesc, view.hView.hCpuDesc);

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

	view.hView = m_rtvHeap.AllocateDescriptor();
	m_pDevice->CreateRenderTargetView(rTexRes.GetResource(), &desc, view.hView.hCpuDesc);
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

	view.hView = m_rtvHeap.AllocateDescriptor();
	m_pDevice->CreateRenderTargetView(rTexArrayRes.GetResource(), &desc, view.hView.hCpuDesc);
	view.pResource = &rTexArrayRes;

	return view;
}

void RdrContext::ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView)
{
	m_rtvHeap.FreeDescriptor(renderTargetView.hView);
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
		hRenderTargetViews[i] = aRenderTargets[i].hView.hCpuDesc;
		if (aRenderTargets[i].pResource)
		{
			aRenderTargets[i].pResource->TransitionState(*this, D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
	}

	hDepthStencilView = depthStencilTarget.hView.hCpuDesc;
	if (depthStencilTarget.pResource)
	{
		depthStencilTarget.pResource->TransitionState(*this, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}

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
	HRESULT hr = m_pCommandAllocators[m_currBackBuffer]->Reset();
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to reset command allocator!"))
		return;

	hr = m_pCommandList->Reset(m_pCommandAllocators[m_currBackBuffer].Get(), nullptr);
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to reset command list!"))
		return;
	
	m_timestampQueryHeap.BeginFrame();

	// Set descriptor heaps
	ID3D12DescriptorHeap* ppHeaps[] = {
		m_srvHeap.GetHeap(),
		m_samplerHeap.GetHeap()
	};
	m_pCommandList->SetDescriptorHeaps(ARRAYSIZE(ppHeaps), ppHeaps);

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

	{
		ID3D12Resource* pTimestampResultsBuffer = m_timestampResultBuffer.Get();
		QueryHeap::TQueryRange queryRange = m_timestampQueryHeap.EndFrame();

		// If the timestamp query indices wrap around, we need to split the resolve into 2 calls
		if (queryRange.second < queryRange.first)
		{
			uint nNumQueries = m_timestampQueryHeap.GetMaxQueries() - queryRange.first;
			m_pCommandList->ResolveQueryData(m_timestampQueryHeap.GetHeap(),
				D3D12_QUERY_TYPE_TIMESTAMP,
				queryRange.first,
				nNumQueries,
				pTimestampResultsBuffer,
				queryRange.first * sizeof(uint64));

			queryRange.first = 0;
		}

		uint nNumQueries = queryRange.second - queryRange.first;
		m_pCommandList->ResolveQueryData(m_timestampQueryHeap.GetHeap(),
			D3D12_QUERY_TYPE_TIMESTAMP,
			queryRange.first,
			nNumQueries,
			pTimestampResultsBuffer,
			queryRange.first * sizeof(uint64));
	}

	HRESULT hr = m_pCommandList->Close();
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to close command list!"))
		return;

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
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	HRESULT hr = m_pSwapChain->GetDesc(&swapChainDesc);
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to get swap chain desc!"))
		return;

	hr = m_pSwapChain->ResizeBuffers(kNumBackBuffers, width, height,
		swapChainDesc.BufferDesc.Format, swapChainDesc.Flags);
	if (!ValidateHResult(hr, __FUNCTION__, "Failed to resize buffers!"))
		return;

	m_currBackBuffer = m_pSwapChain->GetCurrentBackBufferIndex();

	UpdateRenderTargetViews();
}

void RdrContext::ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor)
{
	renderTarget.pResource->TransitionState(*this, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_pCommandList->ClearRenderTargetView(renderTarget.hView.hCpuDesc, clearColor.asFloat4(), 0, nullptr);
}

void RdrContext::ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal)
{
	D3D12_CLEAR_FLAGS clearFlags = (D3D12_CLEAR_FLAGS)0;
	if (bClearDepth)
		clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
	if (bClearStencil)
		clearFlags |= D3D12_CLEAR_FLAG_STENCIL;

	depthStencil.pResource->TransitionState(*this, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_pCommandList->ClearDepthStencilView(depthStencil.hView.hCpuDesc, clearFlags, depthVal, stencilVal, 0, nullptr);
}

RdrRenderTargetView RdrContext::GetPrimaryRenderTarget()
{
	RdrRenderTargetView view;
	view.hView = m_hBackBufferRtvs[m_currBackBuffer];
	view.pResource = nullptr;
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

void RdrContext::Draw(const RdrDrawState& rDrawState, uint instanceCount)
{
	m_pCommandList->SetGraphicsRootSignature(m_pGraphicsRootSignature.Get());
	m_pCommandList->SetPipelineState(rDrawState.pipelineState.pPipelineState);

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
	if (rDrawState.hVsGlobalConstantBufferTable != m_drawState.hVsGlobalConstantBufferTable)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootVsGlobalConstantBufferTable, rDrawState.hVsGlobalConstantBufferTable.hGpuDesc);
		m_drawState.hVsGlobalConstantBufferTable = rDrawState.hVsGlobalConstantBufferTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsConstantBuffer);
	}
	if (rDrawState.hVsPerObjectConstantBuffer != m_drawState.hVsPerObjectConstantBuffer)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootVsPerObjectConstantBufferTable, rDrawState.hVsPerObjectConstantBuffer.hGpuDesc);
		m_drawState.hVsPerObjectConstantBuffer = rDrawState.hVsPerObjectConstantBuffer;
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsConstantBuffer);
	}

	// VS resources
	if (rDrawState.hVsShaderResourceViewTable != m_drawState.hVsShaderResourceViewTable)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootVsShaderResourceViewTable, rDrawState.hVsShaderResourceViewTable.hGpuDesc);
		m_drawState.hVsShaderResourceViewTable = rDrawState.hVsShaderResourceViewTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::VsResource);
	}

	// Tessellation
	if (rDrawState.IsShaderStageActive(RdrShaderStageFlags::Domain))
	{
		// DS constants
		if (rDrawState.hDsGlobalConstantBufferTable != m_drawState.hDsGlobalConstantBufferTable)
		{
			m_pCommandList->SetGraphicsRootDescriptorTable(kRootDsGlobalConstantBufferTable, rDrawState.hDsGlobalConstantBufferTable.hGpuDesc);
			m_drawState.hDsGlobalConstantBufferTable = rDrawState.hDsGlobalConstantBufferTable;
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsConstantBuffer);
		}
		if (rDrawState.hDsPerObjectConstantBuffer != m_drawState.hDsPerObjectConstantBuffer)
		{
			m_pCommandList->SetGraphicsRootDescriptorTable(kRootDsPerObjectConstantBufferTable, rDrawState.hDsPerObjectConstantBuffer.hGpuDesc);
			m_drawState.hDsPerObjectConstantBuffer = rDrawState.hDsPerObjectConstantBuffer;
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsConstantBuffer);
		}

		// DS resources
		if (rDrawState.hDsShaderResourceViewTable != m_drawState.hDsShaderResourceViewTable)
		{
			m_pCommandList->SetGraphicsRootDescriptorTable(kRootDsShaderResourceViewTable, rDrawState.hDsShaderResourceViewTable.hGpuDesc);
			m_drawState.hDsShaderResourceViewTable = rDrawState.hDsShaderResourceViewTable;
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsResource);
		}

		// DS samplers
		if (rDrawState.hDsSamplerTable != m_drawState.hDsSamplerTable)
		{
			m_pCommandList->SetGraphicsRootDescriptorTable(kRootDsSamplerTable, rDrawState.hDsSamplerTable.hGpuDesc);
			m_drawState.hDsSamplerTable = rDrawState.hDsSamplerTable;
			m_rProfiler.IncrementCounter(RdrProfileCounter::DsSamplers);
		}
	}

	// PS constants
	if (rDrawState.hPsGlobalConstantBufferTable != m_drawState.hPsGlobalConstantBufferTable)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsGlobalConstantBufferTable, rDrawState.hPsGlobalConstantBufferTable.hGpuDesc);
		m_drawState.hPsGlobalConstantBufferTable = rDrawState.hPsGlobalConstantBufferTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsConstantBuffer);
	}
	if (rDrawState.hPsMaterialConstantBufferTable != m_drawState.hPsMaterialConstantBufferTable)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsMaterialConstantBufferTable, rDrawState.hPsMaterialConstantBufferTable.hGpuDesc);
		m_drawState.hPsMaterialConstantBufferTable = rDrawState.hPsMaterialConstantBufferTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsConstantBuffer);
	}

	// PS resources
	if (rDrawState.hPsMaterialShaderResourceViewTable != m_drawState.hPsMaterialShaderResourceViewTable)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsMaterialShaderResourceViewTable, rDrawState.hPsMaterialShaderResourceViewTable.hGpuDesc);
		m_drawState.hPsMaterialShaderResourceViewTable = rDrawState.hPsMaterialShaderResourceViewTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsResource);
	}
	if (rDrawState.hPsGlobalShaderResourceViewTable != m_drawState.hPsGlobalShaderResourceViewTable)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsGlobalShaderResourceViewTable, rDrawState.hPsGlobalShaderResourceViewTable.hGpuDesc);
		m_drawState.hPsGlobalShaderResourceViewTable = rDrawState.hPsGlobalShaderResourceViewTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsResource);
	}

	// PS samplers
	if (rDrawState.hPsMaterialSamplerTable != m_drawState.hPsMaterialSamplerTable)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsMaterialSamplerTable, rDrawState.hPsMaterialSamplerTable.hGpuDesc);
		m_drawState.hPsMaterialSamplerTable = rDrawState.hPsMaterialSamplerTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::PsSamplers);
	}
	if (rDrawState.hPsGlobalSamplerTable != m_drawState.hPsGlobalSamplerTable)
	{
		m_pCommandList->SetGraphicsRootDescriptorTable(kRootPsGlobalSamplerTable, rDrawState.hPsGlobalSamplerTable.hGpuDesc);
		m_drawState.hPsGlobalSamplerTable = rDrawState.hPsGlobalSamplerTable;
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
}

void RdrContext::DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ)
{
	m_drawState.Reset(); //donotcheckin?

	m_pCommandList->SetComputeRootSignature(m_pComputeRootSignature.Get());
	m_pCommandList->SetPipelineState(rDrawState.pipelineState.pPipelineState);

	// Constants
	if (rDrawState.hCsConstantBufferTable != m_drawState.hCsConstantBufferTable)
	{
		m_pCommandList->SetComputeRootDescriptorTable(kRootCsConstantBufferTable, rDrawState.hCsConstantBufferTable.hGpuDesc);
		m_drawState.hCsConstantBufferTable = rDrawState.hCsConstantBufferTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::CsConstantBuffer);
	}

	// Resources
	if (rDrawState.hCsShaderResourceViewTable != m_drawState.hCsShaderResourceViewTable)
	{
		m_pCommandList->SetComputeRootDescriptorTable(kRootCsShaderResourceViewTable, rDrawState.hCsShaderResourceViewTable.hGpuDesc);
		m_drawState.hCsShaderResourceViewTable = rDrawState.hCsShaderResourceViewTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::CsResource);
	}

	// Samplers
	if (rDrawState.hCsSamplerTable != m_drawState.hCsSamplerTable)
	{
		m_pCommandList->SetComputeRootDescriptorTable(kRootCsSamplerTable, rDrawState.hCsSamplerTable.hGpuDesc);
		m_drawState.hCsSamplerTable = rDrawState.hCsSamplerTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::CsSampler);
	}

	// UAVs
	if (rDrawState.hCsUnorderedAccessViewTable != m_drawState.hCsUnorderedAccessViewTable)
	{
		m_pCommandList->SetComputeRootDescriptorTable(kRootCsUnorderedAccessViewTable, rDrawState.hCsUnorderedAccessViewTable.hGpuDesc);
		m_drawState.hCsUnorderedAccessViewTable = rDrawState.hCsUnorderedAccessViewTable;
		m_rProfiler.IncrementCounter(RdrProfileCounter::CsUnorderedAccess);
	}

	m_pCommandList->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

RdrQuery RdrContext::InsertTimestampQuery()
{
	RdrQuery query = m_timestampQueryHeap.AllocateQuery();
	m_pCommandList->EndQuery(m_timestampQueryHeap.GetHeap(), D3D12_QUERY_TYPE_TIMESTAMP, query.nId);
	return query;
}

uint64 RdrContext::GetTimestampQueryData(RdrQuery& rQuery)
{
	uint64 timestamp = -1;

	uint64 nBufferOffset = rQuery.nId * sizeof(timestamp);
	CD3DX12_RANGE readRange(nBufferOffset, nBufferOffset + sizeof(timestamp));

	uint64* pTimestampData = nullptr;
	HRESULT hr = m_timestampResultBuffer->Map(0, &readRange, (void**)&pTimestampData);
	if (hr == S_OK)
	{
		timestamp = pTimestampData[rQuery.nId];
	}

	D3D12_RANGE writeRange = { 0, 0 };
	m_timestampResultBuffer->Unmap(0, &writeRange);

	return timestamp;
}

uint64 RdrContext::GetTimestampFrequency() const
{
	return m_commandQueueTimestampFreqency;
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
	const RdrShader* pHullShader, const RdrShader* pDomainShader,
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
	psoDesc.DS = pDomainShader ? CD3DX12_SHADER_BYTECODE(pDomainShader->pCompiledData, pDomainShader->compiledSize) : CD3DX12_SHADER_BYTECODE(0, 0);
	psoDesc.HS = pHullShader ? CD3DX12_SHADER_BYTECODE(pHullShader->pCompiledData, pHullShader->compiledSize) : CD3DX12_SHADER_BYTECODE(0, 0);
	psoDesc.DepthStencilState.DepthEnable = depthStencilState.bTestDepth;
	psoDesc.DepthStencilState.DepthWriteMask = depthStencilState.bWriteDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = getComparisonFuncD3d(depthStencilState.eDepthFunc);
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DSVFormat = (psoDesc.DepthStencilState.DepthEnable ? getD3DDepthFormat(kDefaultDepthFormat) : DXGI_FORMAT_UNKNOWN);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.SampleDesc.Count = (rasterState.bEnableMSAA ? g_debugState.msaaLevel : 1);
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.PrimitiveTopologyType = pDomainShader ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = nNumRtvFormats;
	for (uint i = 0; i < nNumRtvFormats; ++i)
	{
		psoDesc.RTVFormats[i] = getD3DFormat(pRtvFormats[i]);
	}

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