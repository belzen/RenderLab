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

	RdrGeoHandle CreateGeometry(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size);
	void ReleaseGeometry(RdrGeoHandle hGeo);

	RdrResourceHandle LoadTexture(const char* filename);
	void ReleaseResource(RdrResourceHandle hRes);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat format);
	RdrResourceHandle CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount);

	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat format);

	RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat format);
	RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat format);

	RdrDepthStencilView CreateDepthStencilView(RdrResourceHandle hDepthTex, RdrResourceFormat format, bool bMultisampled);
	RdrDepthStencilView CreateDepthStencilView(RdrResourceHandle hDepthTexArray, int arrayIndex, RdrResourceFormat format);
	void ReleaseDepthStencilView(RdrDepthStencilView depthStencilView);

	RdrRenderTargetView CreateRenderTargetView(RdrResourceHandle hTexArrayRes, int arrayIndex, RdrResourceFormat format);

	RdrShaderHandle LoadVertexShader(const char* filename, RdrVertexInputElement* inputDesc, uint numElements);
	RdrShaderHandle LoadGeometryShader(const char* filename);
	RdrShaderHandle LoadPixelShader(const char* filename);
	RdrShaderHandle LoadComputeShader(const char* filename);

	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize);

	RdrResourceHandle CreateVertexBuffer(const void* vertices, int size);
	RdrResourceHandle CreateIndexBuffer(const void* indices, int size);

	void DrawGeo(RdrDrawOp* pDrawOp, RdrShaderMode eShaderMode, const LightList* pLightList, RdrResourceHandle hTileLightIndices);
	void DispatchCompute(RdrDrawOp* pDrawOp);
	
	void SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget);
	void SetDepthStencilState(RdrDepthTestMode eDepthTest);
	void SetBlendState(const bool bAlphaBlend);
	void SetRasterState(const RdrRasterState& rasterState);
	void SetViewport(const Rect& viewport);

	void BeginEvent(LPCWSTR eventName);
	void EndEvent();

	void Resize(uint width, uint height);
	void Present();

	void ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor);
	void ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal);

	RdrRenderTargetView GetPrimaryRenderTarget();
	RdrDepthStencilView GetPrimaryDepthStencilTarget();
	RdrResourceHandle GetPrimaryDepthTexture();

	void* MapResource(RdrResourceHandle hResource, RdrResourceMapMode mapMode);
	void UnmapResource(RdrResourceHandle hResource);

	RdrResourceHandle CreateConstantBuffer(uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	void VSSetConstantBuffers(uint startSlot, uint numBuffers, RdrResourceHandle* aConstantBuffers);
	void PSSetConstantBuffers(uint startSlot, uint numBuffers, RdrResourceHandle* aConstantBuffers);
	void GSSetConstantBuffers(uint startSlot, uint numBuffers, RdrResourceHandle* aConstantBuffers);

	void PSClearResources();

private:
	ID3D11SamplerState* GetSampler(const RdrSamplerState& state);

	ID3D11Device* m_pDevice;
	ID3D11DeviceContext* m_pDevContext;

	ID3DUserDefinedAnnotation* m_pAnnotator;

	IDXGISwapChain*		      m_pSwapChain;
	ID3D11RenderTargetView*   m_pPrimaryRenderTarget;
	RdrDepthStencilView       m_primaryDepthStencilView;
	RdrResourceHandle         m_hPrimaryDepthBuffer;

	ID3D11SamplerState*      m_pSamplers[SAMPLER_TYPES_COUNT];
	ID3D11BlendState*        m_pBlendStates[2];
	ID3D11RasterizerState*   m_pRasterStates[RASTER_STATES_COUNT];
	ID3D11DepthStencilState* m_pDepthStencilStates[kRdrDepthTestMode_Count];

	uint m_msaaLevel;
};