#pragma once
#include "RdrContext.h"
#include "RdrDrawState.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;
struct ID3D11DepthStencilView;
struct ID3D11RenderTargetView;
struct ID3D11View;
struct ID3D11Resource;
struct ID3D11InputLayout;
struct ID3D11SamplerState;
struct IDXGISwapChain;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11BlendState;
struct ID3DUserDefinedAnnotation;

struct D3D11_INPUT_ELEMENT_DESC;

class RdrProfiler;

#define SAMPLER_TYPES_COUNT (int)RdrComparisonFunc::Count * (int)RdrTexCoordMode::Count * 2
#define RASTER_STATES_COUNT 2 * 2 * 2 * 2

class RdrContextD3D11 : public RdrContext
{
public:
	RdrContextD3D11(RdrProfiler& rProfiler);

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
	ID3D11SamplerState* GetSampler(const RdrSamplerState& state);
	void ResetDrawState();

	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pDevContext;

	ID3DUserDefinedAnnotation* m_pAnnotator;

	IDXGISwapChain*		      m_pSwapChain;
	ID3D11RenderTargetView*   m_pPrimaryRenderTarget;

	ID3D11SamplerState*      m_pSamplers[SAMPLER_TYPES_COUNT];
	ID3D11BlendState*        m_pBlendStates[(int)RdrBlendMode::kCount];
	ID3D11RasterizerState*   m_pRasterStates[RASTER_STATES_COUNT];
	ID3D11DepthStencilState* m_pDepthStencilStates[(int)RdrDepthTestMode::Count * 2];

	RdrDrawState m_drawState;
	RdrProfiler& m_rProfiler;

	uint m_presentFlags;

	// Global depth bias to use for raster states that enable it.
	float m_slopeScaledDepthBias;
};