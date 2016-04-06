#pragma once

#include <map>
#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "Math.h"
#include "FreeList.h"
#include "Camera.h"

struct RdrDrawOp;
class LightList;
class RdrContext;
class RdrDrawState;

class RdrContext
{
public:
	virtual bool Init(HWND hWnd, uint width, uint height, uint msaaLevel) = 0;
	virtual void Release() = 0;

	virtual RdrVertexBuffer CreateVertexBuffer(const void* vertices, int size) = 0;
	virtual void ReleaseVertexBuffer(const RdrVertexBuffer* pBuffer) = 0;

	virtual RdrIndexBuffer CreateIndexBuffer(const void* indices, int size) = 0;
	virtual void ReleaseIndexBuffer(const RdrIndexBuffer* pBuffer) = 0;

	virtual bool CreateTexture(const void* pSrcData, const RdrTextureInfo& rTexInfo, RdrResourceUsage eUsage, RdrResource& rResource) = 0;

	virtual bool CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage, RdrResource& rResource) = 0;
	virtual bool UpdateStructuredBuffer(const void* pSrcData, RdrResource& rResource) = 0;

	virtual void ReleaseResource(RdrResource& rResource) = 0;

	virtual RdrDepthStencilView CreateDepthStencilView(const RdrResource& rDepthTex) = 0;
	virtual RdrDepthStencilView CreateDepthStencilView(const RdrResource& rDepthTexArray, int arrayIndex) = 0;
	virtual void ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal) = 0;
	virtual void ReleaseDepthStencilView(const RdrDepthStencilView& depthStencilView) = 0;

	virtual RdrRenderTargetView CreateRenderTargetView(RdrResource& rTexRes) = 0;
	virtual RdrRenderTargetView CreateRenderTargetView(RdrResource& rTexArrayRes, int arrayIndex) = 0;
	virtual void ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor) = 0;
	virtual void ReleaseRenderTargetView(const RdrRenderTargetView& renderTargetView) = 0;

	virtual bool CompileShader(RdrShaderType eType, const char* pShaderText, uint textLen, void** ppOutCompiledData, uint* pOutDataSize) = 0;
	virtual void* CreateShader(RdrShaderType eType, const void* pCompiledData, uint compiledDataSize) = 0;
	virtual RdrInputLayout CreateInputLayout(const void* pCompiledVertexShader, uint vertexShaderSize, const RdrVertexInputElement* aVertexElements, uint numElements) = 0;

	virtual void Draw(const RdrDrawState& rDrawState) = 0;
	virtual void DispatchCompute(const RdrDrawState& rDrawState, uint threadGroupCountX, uint threadGroupCountY, uint threadGroupCountZ) = 0;

	virtual void SetRenderTargets(uint numTargets, const RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget) = 0;
	virtual void SetDepthStencilState(RdrDepthTestMode eDepthTest) = 0;
	virtual void SetBlendState(const bool bAlphaBlend) = 0;
	virtual void SetRasterState(const RdrRasterState& rasterState) = 0;
	virtual void SetViewport(const Rect& viewport) = 0;

	virtual void BeginEvent(LPCWSTR eventName) = 0;
	virtual void EndEvent() = 0;

	virtual void Resize(uint width, uint height) = 0;
	virtual void Present() = 0;

	virtual RdrRenderTargetView GetPrimaryRenderTarget() = 0;

	virtual RdrConstantBufferDeviceObj CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage) = 0;
	virtual void UpdateConstantBuffer(RdrConstantBuffer& buffer, const void* pData) = 0;
	virtual void ReleaseConstantBuffer(const RdrConstantBufferDeviceObj& buffer) = 0;

	virtual void PSClearResources() = 0;
};

