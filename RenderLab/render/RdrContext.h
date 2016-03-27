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

#define MAX_TEXTURES 1024
#define MAX_SHADERS 1024
#define MAX_GEO 1024

typedef FreeList<RdrResource, MAX_TEXTURES> RdrResourceList;
typedef FreeList<RdrVertexShader, MAX_SHADERS> RdrVertexShaderList;
typedef FreeList<RdrPixelShader, MAX_SHADERS> RdrPixelShaderList;
typedef FreeList<RdrComputeShader, MAX_SHADERS> RdrComputeShaderList;
typedef FreeList<RdrGeometry, MAX_GEO> RdrGeoList;

typedef std::map<std::string, RdrResourceHandle> RdrTextureMap;
typedef std::map<std::string, RdrShaderHandle> RdrShaderMap;
typedef std::map<std::string, RdrGeoHandle> RdrGeoMap;

enum RdrResourceMapMode
{
	kRdrResourceMap_Read,
	kRdrResourceMap_Write,
	kRdrResourceMap_ReadWrite,
	kRdrResourceMap_WriteDiscard,
	kRdrResourceMap_WriteNoOverwrite,

	kRdrResourceMap_Count
};

typedef uint RdrCpuAccessFlags;
enum RdrCpuAccessFlag
{
	kRdrCpuAccessFlag_Read  = 0x1,
	kRdrCpuAccessFlag_Write = 0x2
};

enum RdrResourceUsage
{
	kRdrResourceUsage_Default,
	kRdrResourceUsage_Immutable,
	kRdrResourceUsage_Dynamic,
	kRdrResourceUsage_Staging,

	kRdrResourceUsage_Count
};

class RdrContext
{
public:
	RdrGeoHandle LoadGeo(const char* filename);

	virtual bool Init(HWND hWnd, uint width, uint height, uint msaaLevel) = 0;
	virtual void Release() = 0;

	virtual RdrGeoHandle CreateGeometry(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size) = 0;
	virtual void ReleaseGeometry(RdrGeoHandle hGeo) = 0;
	const RdrGeometry* GetGeometry(RdrGeoHandle hGeo) const;

	virtual RdrResourceHandle LoadTexture(const char* filename) = 0;

	const RdrResource* GetResource(RdrResourceHandle hRes) const;
	virtual void ReleaseResource(RdrResourceHandle hRes) = 0;;

	virtual RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat format) = 0;
	virtual RdrResourceHandle CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount) = 0;
	virtual RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat format) = 0;

	virtual RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat format) = 0;
	virtual RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat format) = 0;

	virtual RdrDepthStencilView CreateDepthStencilView(RdrResourceHandle hDepthTex, RdrResourceFormat format, bool bMultisampled) = 0;
	virtual RdrDepthStencilView CreateDepthStencilView(RdrResourceHandle hDepthTexArray, int arrayIndex, RdrResourceFormat format) = 0;

	virtual RdrRenderTargetView CreateRenderTargetView(RdrResourceHandle hTexArrayRes, int arrayIndex, RdrResourceFormat format) = 0;

	virtual RdrShaderHandle LoadVertexShader(const char* filename, RdrVertexInputElement* inputDesc, uint numElements) = 0;
	virtual RdrShaderHandle LoadPixelShader(const char* filename) = 0;
	virtual RdrShaderHandle LoadComputeShader(const char* filename) = 0;

	virtual RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize) = 0;
	
	virtual RdrResourceHandle CreateVertexBuffer(const void* vertices, int size) = 0;
	virtual RdrResourceHandle CreateIndexBuffer(const void* indices, int size) = 0;

	virtual void DrawGeo(RdrDrawOp* pDrawOp, RdrShaderMode eShaderMode, const LightList* pLightList, RdrResourceHandle hTileLightIndices) = 0;
	virtual void DispatchCompute(RdrDrawOp* pDrawOp) = 0;

	virtual void SetRenderTargets(uint numTargets, RdrRenderTargetView* aRenderTargets, RdrDepthStencilView depthStencilTarget) = 0;
	virtual void SetDepthStencilState(RdrDepthTestMode eDepthTest) = 0;
	virtual void SetBlendState(const bool bAlphaBlend) = 0;
	virtual void SetRasterState(const RdrRasterState& rasterState) = 0;
	virtual void SetViewport(const Rect& viewport) = 0;

	virtual void BeginEvent(LPCWSTR eventName) = 0;
	virtual void EndEvent() = 0;

	virtual void Resize(uint width, uint height) = 0;
	virtual void Present() = 0;

	virtual void ClearRenderTargetView(const RdrRenderTargetView& renderTarget, const Color& clearColor) = 0;
	virtual void ClearDepthStencilView(const RdrDepthStencilView& depthStencil, const bool bClearDepth, const float depthVal, const bool bClearStencil, const uint8 stencilVal) = 0;

	virtual RdrRenderTargetView GetPrimaryRenderTarget() = 0;
	virtual RdrDepthStencilView GetPrimaryDepthStencilTarget() = 0;
	virtual RdrResourceHandle GetPrimaryDepthTexture() = 0;

	virtual void* MapResource(RdrResourceHandle hResource, RdrResourceMapMode mapMode) = 0;
	virtual void UnmapResource(RdrResourceHandle hResource) = 0;

	virtual RdrResourceHandle CreateConstantBuffer(uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage) = 0;
	virtual void VSSetConstantBuffers(uint startSlot, uint numBuffers, RdrResourceHandle* aConstantBuffers) = 0;
	virtual void PSSetConstantBuffers(uint startSlot, uint numBuffers, RdrResourceHandle* aConstantBuffers) = 0;

	virtual void PSClearResources() = 0;

protected:
	RdrTextureMap m_textureCache;
	RdrResourceList m_resources;

	RdrShaderMap m_vertexShaderCache;
	RdrVertexShaderList m_vertexShaders;

	RdrShaderMap m_pixelShaderCache;
	RdrPixelShaderList m_pixelShaders;

	RdrShaderMap m_computeShaderCache;
	RdrComputeShaderList m_computeShaders;

	RdrGeoMap m_geoCache;
	RdrGeoList m_geo;
};

