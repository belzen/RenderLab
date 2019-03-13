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

#if 0 //donotcheckin - unnecessary if keeping dx12 includes here
interface ID3D12Device2;
interface ID3D12CommandQueue;
interface ID3D12GraphicsCommandList;
interface ID3D12Resource;
interface ID3D12CommandAllocator;
interface ID3D12DescriptorHeap;
interface ID3D12Fence;
interface ID3D12Debug;
interface ID3D12InfoQueue;
interface IDXGISwapChain4;
interface ID3D12QueryHeap;
struct CD3DX12_CPU_DESCRIPTOR_HANDLE;
enum D3D12_QUERY_HEAP_TYPE;
enum D3D12_DESCRIPTOR_HEAP_TYPE;
enum D3D12_RESOURCE_STATES;
#endif

#define SAMPLER_TYPES_COUNT (int)RdrComparisonFunc::Count * (int)RdrTexCoordMode::Count * 2
#define RASTER_STATES_COUNT 2 * 2 * 2 * 2

static constexpr RdrResourceFormat kDefaultDepthFormat = RdrResourceFormat::D24_UNORM_S8_UINT;

class DescriptorHeap
{
public:
	void Create(ComPtr<ID3D12Device2> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint nMaxDescriptors, bool bShaderVisible);

	uint AllocateDescriptorId();
	D3D12DescriptorHandle AllocateDescriptor();
	void FreeDescriptorById(uint nId);
	void FreeDescriptor(D3D12DescriptorHandle hDesc);

	void GetDescriptorHandle(uint nId, CD3DX12_CPU_DESCRIPTOR_HANDLE* pOutDesc);
	ID3D12DescriptorHeap* GetHeap() const { return m_pDescriptorHeap.Get(); }

private:
	ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
	uint m_descriptorSize;
	IdSetDynamic m_idSet;
};

class DescriptorRingBuffer
{
public:
	void Create(ComPtr<ID3D12Device2> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint nMaxDescriptors);

	CD3DX12_CPU_DESCRIPTOR_HANDLE AllocateDescriptors(uint numToAllocate, uint& nOutDescriptorStartIndex);
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint nDescriptor) const;

	uint GetDescriptorSize() const { return m_descriptorSize; }
	ID3D12DescriptorHeap* GetHeap() const { return m_pDescriptorHeap.Get(); }

private:
	ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
	uint m_descriptorSize;
	uint m_nextDescriptor;
	uint m_maxDescriptors;
};

class QueryHeap
{
public:
	void Create(ComPtr<ID3D12Device2> pDevice, D3D12_QUERY_HEAP_TYPE type, uint nMaxQueries);

	D3D12QueryHandle AllocateQuery();
	void FreeQuery(D3D12QueryHandle hQuery);

	ID3D12QueryHeap* GetHeap() { return m_pQueryHeap.Get(); }

private:
	ComPtr<ID3D12QueryHeap> m_pQueryHeap;
	D3D12_QUERY_HEAP_TYPE m_type;
	IdSetDynamic m_idSet;
};

class RdrContext
{
public:
	RdrContext(RdrProfiler& rProfiler);

	bool Init(HWND hWnd, uint width, uint height);
	void Resize(uint width, uint height);

	void Release();

	bool IsIdle();

	/////////////////////////////////////////////////////////////
	// Geometry
	bool CreateVertexBuffer(const void* vertices, int size, RdrResourceAccessFlags accessFlags, RdrResource& rResource);
	bool CreateIndexBuffer(const void* indices, int size, RdrResourceAccessFlags accessFlags, RdrResource& rResource);

	/////////////////////////////////////////////////////////////
	// Resources
	bool CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceAccessFlags accessFlags, RdrResource& rResource);

	bool CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, RdrResource& rResource);
	bool CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceAccessFlags accessFlags, RdrResource& rResource);
	bool UpdateBuffer(const void* pSrcData, RdrResource& rResource, int numElements);

	void CopyResourceRegion(const RdrResource& rSrcResource, const RdrBox& srcRegion, const RdrResource& rDstResource, const IVec3& dstOffset);
	void ReadResource(const RdrResource& rSrcResource, void* pDstData, uint dstDataSize);

	void ReleaseResource(RdrResource& rResource);

	void ResolveResource(const RdrResource& rSrc, const RdrResource& rDst);

	RdrShaderResourceView CreateShaderResourceViewTexture(const RdrResource& rResource);
	RdrShaderResourceView CreateShaderResourceViewBuffer(const RdrResource& rResource, uint firstElement);
	void ReleaseShaderResourceView(RdrShaderResourceView& resourceView);

	/////////////////////////////////////////////////////////////
	// Depth Stencil Views
	RdrDepthStencilView CreateDepthStencilView(const RdrResource& rDepthTex);
	RdrDepthStencilView CreateDepthStencilView(const RdrResource& rDepthTexArray, uint arrayStartIndex, uint arraySize);
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

	/////////////////////////////////////////////////////////////
	// Shaders
	bool CompileShader(RdrShaderStage eType, const char* pShaderText, uint textLen, void** ppOutCompiledData, uint* pOutDataSize) const;
	void ReleaseShader(RdrShader* pShader) const;

	RdrPipelineState CreateGraphicsPipelineState(
		const RdrShader* pVertexShader, const RdrShader* pPixelShader,
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

	void PSClearResources();

	void TransitionResource(RdrResource* pResource, D3D12_RESOURCE_STATES eState);

	/////////////////////////////////////////////////////////////
	// Events
	void BeginEvent(LPCWSTR eventName);
	void EndEvent();

	/////////////////////////////////////////////////////////////
	// Constant Buffers
	bool CreateConstantBuffer(const void* pData, uint size, RdrResourceAccessFlags accessFlags, RdrResource& rResource);
	void UpdateResource(RdrResource& rResource, const void* pData, const uint dataSize);

	/////////////////////////////////////////////////////////////
	// Queries
	RdrQuery CreateQuery(RdrQueryType eType);
	void ReleaseQuery(RdrQuery& rQuery);

	void BeginQuery(RdrQuery& rQuery);
	void EndQuery(RdrQuery& rQuery);

	uint64 GetTimestampQueryData(RdrQuery& rQuery);
	uint64 GetTimestampFrequency() const;

private:
	static constexpr int kNumBackBuffers = 2;
	static constexpr int kMaxNumSamplers = 64;

	void SetDescriptorHeaps();
	void UpdateRenderTargetViews();
	bool CreateBuffer(const void* pSrcData, const int size, RdrResourceAccessFlags accessFlags, const D3D12_RESOURCE_STATES initialState, RdrResource& rResource);

	D3D12DescriptorHandle GetSampler(const RdrSamplerState& state);

	ComPtr<ID3D12Device2> m_pDevice;
	ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	uint64 m_commandQueueTimestampFreqency;
	ComPtr<ID3D12GraphicsCommandList> m_pCommandList;

	ComPtr<ID3D12Resource> m_pBackBuffers[kNumBackBuffers];
	D3D12DescriptorHandle m_hBackBufferRtvs[kNumBackBuffers];

	ComPtr<ID3D12CommandAllocator> m_pCommandAllocators[kNumBackBuffers];

	struct RdrUploadBuffer
	{
		ComPtr<ID3D12Resource> pBuffer;
		uint8* pStart;
		uint8* pEnd;
		uint8* pCurr;
	};
	RdrUploadBuffer m_uploadBuffers[kNumBackBuffers];

	ComPtr<ID3D12Fence> m_pFence;
	HANDLE m_hFenceEvent;
	uint64 m_nFenceValue;
	uint64 m_nFrameFenceValues[kNumBackBuffers];

	DescriptorHeap m_rtvHeap;
	DescriptorHeap m_samplerHeap;
	DescriptorHeap m_srvHeap;
	DescriptorHeap m_dsvHeap;

	RdrDepthStencilView m_nullDepthStencilView;
	RdrRenderTargetView m_nullRenderTargetView;

	DescriptorRingBuffer m_dynamicDescriptorHeap;
	DescriptorRingBuffer m_dynamicSamplerDescriptorHeap;

	QueryHeap m_timestampQueryHeap;
	ComPtr<ID3D12Resource> m_timestampResultBuffer;

	ComPtr<ID3D12Debug> m_pDebug;
	ComPtr<ID3D12InfoQueue> m_pInfoQueue;

	ComPtr<IDXGISwapChain4> m_pSwapChain;
	ComPtr<ID3D12RootSignature> m_pGraphicsRootSignature;
	ComPtr<ID3D12RootSignature> m_pComputeRootSignature;

	D3D12DescriptorHandle m_samplers[kMaxNumSamplers];
	
	RdrDrawState m_drawState;
	RdrProfiler& m_rProfiler;

	uint m_presentFlags;
	uint m_currBackBuffer;

	// Global depth bias to use for raster states that enable it.
	float m_slopeScaledDepthBias;
};