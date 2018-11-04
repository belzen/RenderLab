#pragma once
#include "RdrContext.h"
#include "RdrDrawState.h"
#include "IDSet.h"
#include <wrl.h>
using namespace Microsoft::WRL;

class RdrProfiler;

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

#define SAMPLER_TYPES_COUNT (int)RdrComparisonFunc::Count * (int)RdrTexCoordMode::Count * 2
#define RASTER_STATES_COUNT 2 * 2 * 2 * 2

class DescriptorHeap
{
public:
	void Create(ComPtr<ID3D12Device2> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint nMaxDescriptors);

	uint AllocateDescriptorId();
	D3D12DescriptorHandle AllocateDescriptor();
	void FreeDescriptorById(uint nId);
	void FreeDescriptor(D3D12DescriptorHandle hDesc);

	void GetDescriptorHandle(uint nId, CD3DX12_CPU_DESCRIPTOR_HANDLE* pOutDesc);

private:
	ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
	uint m_descriptorSize;
	IdSetDynamic m_idSet;
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

class RdrContextD3D12 : public RdrContext
{
public:
	RdrContextD3D12(RdrProfiler& rProfiler);

	bool Init(HWND hWnd, uint width, uint height);
	void Resize(uint width, uint height);

	void Release();

	bool IsIdle();

	/////////////////////////////////////////////////////////////
	// Geometry
	RdrBuffer CreateVertexBuffer(const void* vertices, int size, RdrResourceUsage eUsage);
	RdrBuffer CreateIndexBuffer(const void* indices, int size, RdrResourceUsage eUsage);
	void ReleaseBuffer(const RdrBuffer& rBuffer);

	/////////////////////////////////////////////////////////////
	// Resources
	bool CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResourceBindings eBindings, RdrResource& rResource);

	bool CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResource& rResource);
	bool CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage, RdrResource& rResource);
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
	void ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal);
	void ReleaseDepthStencilView(const RdrDepthStencilView& depthStencilView);

	/////////////////////////////////////////////////////////////
	// Render Target Views
	RdrRenderTargetView CreateRenderTargetView(RdrResource& rTexRes);
	RdrRenderTargetView CreateRenderTargetView(RdrResource& rTexArrayRes, uint arrayStartIndex, uint arraySize);
	void ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor);
	void ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView);

	RdrRenderTargetView GetPrimaryRenderTarget();

	/////////////////////////////////////////////////////////////
	// Shaders
	bool CompileShader(RdrShaderStage eType, const char* pShaderText, uint textLen, void** ppOutCompiledData, uint* pOutDataSize);
	void* CreateShader(RdrShaderStage eType, const void* pCompiledData, uint compiledDataSize);
	void ReleaseShader(RdrShader* pShader);

	RdrInputLayout CreateInputLayout(const void* pCompiledVertexShader, uint vertexShaderSize, const RdrVertexInputElement* aVertexElements, uint numElements);

	/////////////////////////////////////////////////////////////
	// Draw commands
	void Draw(const RdrDrawState& rDrawState, uint instanceCount);
	void DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ);

	void Present();

	/////////////////////////////////////////////////////////////
	// Pipeline state
	void SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget);
	void SetDepthStencilState(RdrDepthTestMode eDepthTest, bool bWriteEnabled);
	void SetBlendState(RdrBlendMode blendMode);
	void SetRasterState(const RdrRasterState& rasterState);
	void SetViewport(const Rect& viewport);

	void PSClearResources();

	/////////////////////////////////////////////////////////////
	// Events
	void BeginEvent(LPCWSTR eventName);
	void EndEvent();

	/////////////////////////////////////////////////////////////
	// Constant Buffers
	RdrConstantBufferDeviceObj CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	void UpdateConstantBuffer(const RdrConstantBufferDeviceObj& buffer, const void* pData, const uint dataSize);
	void ReleaseConstantBuffer(const RdrConstantBufferDeviceObj& buffer);

	/////////////////////////////////////////////////////////////
	// Queries
	RdrQuery CreateQuery(RdrQueryType eType);
	void ReleaseQuery(RdrQuery& rQuery);

	void BeginQuery(RdrQuery& rQuery);
	void EndQuery(RdrQuery& rQuery);

	uint64 GetTimestampQueryData(RdrQuery& rQuery);
	RdrQueryDisjointData GetDisjointQueryData(RdrQuery& rQuery);

private:
	static const int kNumBackBuffers = 2;

	void ResetDrawState();
	void UpdateRenderTargetViews();
	void CreateBuffer(const void* pSrcData, const int size, const D3D12_RESOURCE_STATES initialState, RdrBuffer& rBuffer);

	ComPtr<ID3D12Device2> m_pDevice;
	ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	ComPtr<ID3D12GraphicsCommandList> m_pCommandList;

	ComPtr<ID3D12Resource> m_pBackBuffers[kNumBackBuffers];
	D3D12DescriptorHandle m_hBackBufferRtvs[kNumBackBuffers];

	ComPtr<ID3D12CommandAllocator> m_pCommandAllocators[kNumBackBuffers];

	ComPtr<ID3D12Fence> m_pFence;
	HANDLE m_hFenceEvent;
	uint64 m_nFenceValue;
	uint64 m_nFrameFenceValues[kNumBackBuffers];

	DescriptorHeap m_rtvHeap;
	DescriptorHeap m_samplerHeap;
	DescriptorHeap m_srvHeap;
	DescriptorHeap m_dsvHeap;

	QueryHeap m_timestampQueryHeap;
	ComPtr<ID3D12Resource> m_timestampResultBuffer;

	ComPtr<ID3D12Debug> m_pDebug;
	ComPtr<ID3D12InfoQueue> m_pInfoQueue;

	ComPtr<IDXGISwapChain4> m_pSwapChain;

	RdrDrawState m_drawState;
	RdrProfiler& m_rProfiler;

	uint m_presentFlags;
	uint m_currBackBuffer;

	// Global depth bias to use for raster states that enable it.
	float m_slopeScaledDepthBias;
};