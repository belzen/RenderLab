#pragma once

#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "Math.h"
#include "Camera.h"
#include "RdrDrawState.h"
#include "IDSet.h"
#include <wrl.h>
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
using namespace Microsoft::WRL;

class RdrProfiler;
struct Rect;
struct RdrDrawOp;
class LightList;
class RdrContext;
class RdrDrawState;

#define SAMPLER_TYPES_COUNT (int)RdrComparisonFunc::Count * (int)RdrTexCoordMode::Count * 2
#define RASTER_STATES_COUNT 2 * 2 * 2 * 2

static constexpr RdrResourceFormat kDefaultDepthFormat = RdrResourceFormat::D24_UNORM_S8_UINT;
static constexpr int kNumBackBuffers = 2;

class DescriptorHeap
{
public:
	void Create(ComPtr<ID3D12Device> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, const uint* anDescriptorTableSizes, uint numDescriptorTableSizes, bool bShaderVisible);
	void Cleanup();

	D3D12DescriptorHandles AllocateDescriptor();
	D3D12DescriptorHandles AllocateCopyDescriptor();

	D3D12DescriptorHandles CreateDescriptorTable(const D3D12DescriptorHandles* pSrcDescriptors, uint size);
	void FreeDescriptor(D3D12DescriptorHandles desc);

	void GetDescriptorHandle(uint nId, CD3DX12_CPU_DESCRIPTOR_HANDLE* pOutDesc);
	ID3D12DescriptorHeap* GetHeap() const { return m_pDescriptorHeap.Get(); }

private:
	ID3D12Device* m_pDevice;

	ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
	ComPtr<ID3D12DescriptorHeap> m_pCopyDescriptorHeap;
	D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
	uint m_descriptorSize;

	typedef std::vector<D3D12DescriptorHandles> DescriptorTableList;
	DescriptorTableList m_tables[5];
	DescriptorTableList m_copyDescriptors;

	ThreadMutex m_mutex;
};

class DescriptorRingBuffer
{
public:
	void Create(ComPtr<ID3D12Device> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint nMaxDescriptors);
	void Cleanup();

	void BeginFrame();

	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateDescriptors(uint numToAllocate, uint& nOutDescriptorStartIndex);
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint nDescriptor) const;

	uint GetDescriptorSize() const { return m_descriptorSize; }
	ID3D12DescriptorHeap* GetHeap() const { return m_pDescriptorHeap.Get(); }

private:
	ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;

	uint m_nFrameStartDescriptor[kNumBackBuffers];
	uint m_nFrameDescriptorCount[kNumBackBuffers];
	uint m_nFrame;
	uint m_nFrameAvailableDescriptors;

	uint m_descriptorSize;
	uint m_nextDescriptor;
	uint m_maxDescriptors;
};

class QueryHeap
{
public:
	typedef std::pair<uint, uint> TQueryRange;

	void Create(ComPtr<ID3D12Device> pDevice, D3D12_QUERY_HEAP_TYPE type, uint nMaxQueries);
	void Cleanup();

	RdrQuery AllocateQuery();

	void BeginFrame();
	TQueryRange EndFrame();

	ID3D12QueryHeap* GetHeap() { return m_pQueryHeap.Get(); }
	TQueryRange GetFrameRange() const { return m_nFrameQueryStartEnd[m_nFrame]; }
	uint GetMaxQueries() const { return m_nMaxQueries; }

private:
	ComPtr<ID3D12QueryHeap> m_pQueryHeap;
	D3D12_QUERY_HEAP_TYPE m_type;

	TQueryRange m_nFrameQueryStartEnd[kNumBackBuffers];
	uint m_nFrame;
	uint m_nNextQuery;
	uint m_nMaxQueries;
};

struct RdrSampler
{
	D3D12DescriptorHandles hCopyable;
	D3D12DescriptorHandles hShaderVisible;
};

class RdrContext
{
public:
	RdrContext(RdrProfiler& rProfiler);

	bool Init(HWND hWnd, uint width, uint height);
	void Resize(uint width, uint height);

	void Release();

	bool IsIdle();

	ID3D12GraphicsCommandList* GetCommandList() { return m_pCommandList.Get(); }
	ID3D12Device* GetDevice() { return m_pDevice.Get(); }

	DescriptorHeap& GetSrvHeap() { return m_srvHeap; }
	DescriptorHeap& GetSamplerHeap() { return m_samplerHeap; }

	const RdrSampler& GetSampler(const RdrSamplerState& state);

	/////////////////////////////////////////////////////////////
	// Depth Stencil Views
	RdrDepthStencilView CreateDepthStencilView(RdrResource& rDepthTex);
	RdrDepthStencilView CreateDepthStencilView(RdrResource& rDepthTexArray, uint arrayStartIndex, uint arraySize);
	RdrDepthStencilView GetNullDepthStencilView() const { return m_nullDepthStencilView; }
	void ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal);
	void ReleaseDepthStencilView(const RdrDepthStencilView& depthStencilView);

	/////////////////////////////////////////////////////////////
	// Render Target Views
	RdrRenderTargetView CreateRenderTargetView(RdrResource& rTexRes);
	RdrRenderTargetView CreateRenderTargetView(RdrResource& rTexArrayRes, uint arrayStartIndex, uint arraySize);
	RdrRenderTargetView GetNullRenderTargetView() const { return m_nullRenderTargetView; }
	void ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor);
	void ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView);

	RdrRenderTargetView GetPrimaryRenderTarget();

	RdrShaderResourceView GetNullShaderResourceView() const { return m_nullShaderResourceView; }
	RdrConstantBufferView GetNullConstantBufferView() const { return m_nullConstantBufferView; }

	/////////////////////////////////////////////////////////////
	// Shaders
	bool CompileShader(RdrShaderStage eType, const char* pShaderText, uint textLen, void** ppOutCompiledData, uint* pOutDataSize) const;
	void ReleaseShader(RdrShader* pShader) const;

	RdrPipelineState CreateGraphicsPipelineState(
		const RdrShader* pVertexShader, const RdrShader* pPixelShader, 
		const RdrShader* pHullShader, const RdrShader* pDomainShader,
		const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
		const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
		const RdrBlendMode eBlendMode,
		const RdrRasterState& rasterState,
		const RdrDepthStencilState& depthStencilState);

	RdrPipelineState CreateComputePipelineState(const RdrShader& computeShader);

	/////////////////////////////////////////////////////////////
	// Draw commands
	void Draw(const RdrDrawState& rDrawState, uint instanceCount);
	void DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ);

	void UAVBarrier(const RdrResource* pResource);

	void BeginFrame();
	void Present();

	/////////////////////////////////////////////////////////////
	// Pipeline state
	void SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget);
	void SetViewport(const Rect& viewport);

	/////////////////////////////////////////////////////////////
	// Events
	void BeginEvent(LPCWSTR eventName);
	void EndEvent();

	/////////////////////////////////////////////////////////////
	// Queries
	RdrQuery InsertTimestampQuery();

	uint64 GetTimestampQueryData(RdrQuery& rQuery);
	uint64 GetTimestampFrequency() const;

	uint64 GetLastCompletedFrame() const { return m_nFrameNum - kNumBackBuffers; }
	uint64 GetFrameNum() const { return m_nFrameNum; }

private:
	static constexpr int kMaxNumSamplers = 64;

	void SetDescriptorHeaps(bool bCompute);
	void UpdateRenderTargetViews();

	ComPtr<ID3D12Device> m_pDevice;
	ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	uint64 m_commandQueueTimestampFreqency;
	ComPtr<ID3D12GraphicsCommandList> m_pCommandList;

	ComPtr<ID3D12Resource> m_pBackBuffers[kNumBackBuffers];
	D3D12DescriptorHandles m_hBackBufferRtvs[kNumBackBuffers];

	ComPtr<ID3D12CommandAllocator> m_pCommandAllocators[kNumBackBuffers];

	ComPtr<ID3D12Fence> m_pFence;
	HANDLE m_hFenceEvent;
	uint64 m_nFenceValue;
	uint64 m_nFrameNum;

	DescriptorHeap m_rtvHeap;
	DescriptorHeap m_samplerHeap;
	DescriptorHeap m_srvHeap;
	DescriptorHeap m_dsvHeap;

	RdrDepthStencilView m_nullDepthStencilView;
	RdrRenderTargetView m_nullRenderTargetView;
	RdrShaderResourceView m_nullShaderResourceView;
	RdrConstantBufferView m_nullConstantBufferView;

	DescriptorRingBuffer m_dynamicDescriptorHeap;
	DescriptorRingBuffer m_dynamicSamplerDescriptorHeap;

	QueryHeap m_timestampQueryHeap;
	ComPtr<ID3D12Resource> m_timestampResultBuffer;

	ComPtr<ID3D12Debug> m_pDebug;

	ComPtr<IDXGISwapChain4> m_pSwapChain;
	ComPtr<ID3D12RootSignature> m_pGraphicsRootSignature;
	ComPtr<ID3D12RootSignature> m_pComputeRootSignature;

	RdrSampler m_samplers[kMaxNumSamplers];
	
	RdrDrawState m_drawState;
	RdrProfiler& m_rProfiler;

	uint m_presentFlags;
	uint m_currBackBuffer;

	// Global depth bias to use for raster states that enable it.
	float m_slopeScaledDepthBias;
};