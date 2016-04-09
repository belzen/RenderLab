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

#define SAMPLER_TYPES_COUNT kComparisonFunc_Count * kRdrTexCoordMode_Count * 2
#define RASTER_STATES_COUNT 2 * 2 * 2

class RdrContextD3D11 : public RdrContext
{
	bool Init(HWND hWnd, uint width, uint height, uint msaaLevel);
	void Release();

	bool IsIdle();

	RdrVertexBuffer CreateVertexBuffer(const void* vertices, int size);
	void ReleaseVertexBuffer(const RdrVertexBuffer* pBuffer);

	RdrIndexBuffer CreateIndexBuffer(const void* indices, int size);
	void ReleaseIndexBuffer(const RdrIndexBuffer* pBuffer);

	bool CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource);

	bool CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage, RdrResource& rResource);
	bool UpdateStructuredBuffer(const void* pSrcData, RdrResource& rResource);

	void ReleaseResource(RdrResource& rResource);

	RdrDepthStencilView CreateDepthStencilView(const RdrResource& rDepthTex);
	RdrDepthStencilView CreateDepthStencilView(const RdrResource& rDepthTexArray, uint arrayStartIndex, uint arraySize);
	void ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal);
	void ReleaseDepthStencilView(const RdrDepthStencilView& depthStencilView);

	RdrRenderTargetView CreateRenderTargetView(RdrResource& rTexRes);
	RdrRenderTargetView CreateRenderTargetView(RdrResource& rTexArrayRes, uint arrayStartIndex, uint arraySize);
	void ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor);
	void ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView);

	bool CompileShader(RdrShaderType eType, const char* pShaderText, uint textLen, void** ppOutCompiledData, uint* pOutDataSize);
	void* CreateShader(RdrShaderType eType, const void* pCompiledData, uint compiledDataSize);
	RdrInputLayout CreateInputLayout(const void* pCompiledVertexShader, uint vertexShaderSize, const RdrVertexInputElement* aVertexElements, uint numElements);

	void Draw(const RdrDrawState& rDrawState);
	void DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ);
	
	void SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget);
	void SetDepthStencilState(RdrDepthTestMode eDepthTest);
	void SetBlendState(const bool bAlphaBlend);
	void SetRasterState(const RdrRasterState& rasterState);
	void SetViewport(const Rect& viewport);

	void BeginEvent(LPCWSTR eventName);
	void EndEvent();

	void Resize(uint width, uint height);
	void Present();

	RdrRenderTargetView GetPrimaryRenderTarget();

	RdrConstantBufferDeviceObj CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	void UpdateConstantBuffer(RdrConstantBuffer& buffer, const void* pData);
	void ReleaseConstantBuffer(const RdrConstantBufferDeviceObj& buffer);

	void PSClearResources();

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
	ID3D11DepthStencilState* m_pDepthStencilStates[kRdrDepthTestMode_Count];

	uint m_msaaLevel;
	uint m_presentFlags;
};