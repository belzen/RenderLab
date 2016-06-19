#pragma once
#include "RdrContext.h"

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

#define SAMPLER_TYPES_COUNT (int)RdrComparisonFunc::Count * (int)RdrTexCoordMode::Count * 2
#define RASTER_STATES_COUNT 2 * 2 * 2

class RdrContextD3D11 : public RdrContext
{
	bool Init(HWND hWnd, uint width, uint height);
	void Resize(uint width, uint height);

	void Release();

	bool IsIdle();

	/////////////////////////////////////////////////////////////
	// Geometry
	RdrVertexBuffer CreateVertexBuffer(const void* vertices, int size);
	void ReleaseVertexBuffer(const RdrVertexBuffer& rBuffer);

	RdrIndexBuffer CreateIndexBuffer(const void* indices, int size);
	void ReleaseIndexBuffer(const RdrIndexBuffer& rBuffer);

	/////////////////////////////////////////////////////////////
	// Resources
	bool CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource);

	bool CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResource& rResource);
	bool CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage, RdrResource& rResource);
	bool UpdateBuffer(const void* pSrcData, RdrResource& rResource);

	void CopyResourceRegion(const RdrResource& rSrcResource, const RdrBox& srcRegion, const RdrResource& rDstResource, const IVec3& dstOffset);
	void ReadResource(const RdrResource& rSrcResource, void* pDstData, uint dstDataSize);
	
	void ReleaseResource(RdrResource& rResource);

	void ResolveSurface(const RdrResource& rSrc, const RdrResource& rDst);

	void PSClearResources();

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
	void Draw(const RdrDrawState& rDrawState);
	void DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ);

	void Present();

	/////////////////////////////////////////////////////////////
	// Pipeline state
	void SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget);
	void SetDepthStencilState(RdrDepthTestMode eDepthTest);
	void SetBlendState(const bool bAlphaBlend);
	void SetRasterState(const RdrRasterState& rasterState);
	void SetViewport(const Rect& viewport);

	/////////////////////////////////////////////////////////////
	// Events
	void BeginEvent(LPCWSTR eventName);
	void EndEvent();

	/////////////////////////////////////////////////////////////
	// Constant Buffers
	RdrConstantBufferDeviceObj CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	void UpdateConstantBuffer(RdrConstantBuffer& buffer, const void* pData);
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

	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pDevContext;

	ID3DUserDefinedAnnotation* m_pAnnotator;

	IDXGISwapChain*		      m_pSwapChain;
	ID3D11RenderTargetView*   m_pPrimaryRenderTarget;

	ID3D11SamplerState*      m_pSamplers[SAMPLER_TYPES_COUNT];
	ID3D11BlendState*        m_pBlendStates[2];
	ID3D11RasterizerState*   m_pRasterStates[RASTER_STATES_COUNT];
	ID3D11DepthStencilState* m_pDepthStencilStates[(int)RdrDepthTestMode::Count];

	uint m_presentFlags;
};