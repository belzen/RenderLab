#pragma once

#include "RdrResource.h"
#include "RdrGeometry.h"
#include "UtilsLib/FixedVector.h"
#include "UtilsLib/StringCache.h"

class Renderer;
class RdrContext;

class RdrLighting;
class Decal;
class ModelComponent;
class ModelData;
class RdrSky;
class Terrain;
class Sprite;
class Ocean;
class RdrAction;
struct RdrMaterial;
class RdrPostProcess;
struct RdrDebugBackpointer
{
	// donotcheckin - smoother type handling
	RdrDebugBackpointer() = default;
	RdrDebugBackpointer(const RdrLighting* pSrc)	: pSource(pSrc), nDataType(0) {}
	RdrDebugBackpointer(const RdrSky* pSrc)			: pSource(pSrc), nDataType(1) {}
	RdrDebugBackpointer(const Decal* pSrc)			: pSource(pSrc), nDataType(2) {}
	RdrDebugBackpointer(const ModelComponent* pSrc) : pSource(pSrc), nDataType(3) {}
	RdrDebugBackpointer(const ModelData* pSrc)		: pSource(pSrc), nDataType(4) {}
	RdrDebugBackpointer(const Terrain* pSrc)		: pSource(pSrc), nDataType(5) {}
	RdrDebugBackpointer(const Sprite* pSrc)			: pSource(pSrc), nDataType(6) {}
	RdrDebugBackpointer(const Ocean* pSrc)			: pSource(pSrc), nDataType(7) {}
	RdrDebugBackpointer(const RdrAction* pSrc)		: pSource(pSrc), nDataType(8) {}
	RdrDebugBackpointer(const RdrMaterial* pSrc)	: pSource(pSrc), nDataType(9) {}
	RdrDebugBackpointer(const Renderer* pSrc)		: pSource(pSrc), nDataType(10) {}
	RdrDebugBackpointer(const RdrPostProcess* pSrc)	: pSource(pSrc), nDataType(11) {}

	~RdrDebugBackpointer() = default;

	const void* pSource;
	uint nDataType;
};

enum class RdrDefaultResource
{
	kBlackTex2d,
	kBlackTex3d,

	kNumResources
};

struct RdrRenderTarget
{
	RdrResourceHandle hTexture;
	RdrResourceHandle hTextureMultisampled;
	RdrRenderTargetViewHandle hRenderTarget;
};

struct RdrGlobalRenderTargetHandles
{
	static const RdrRenderTargetViewHandle kPrimary = 1;
	static const int kCount = 1;
};

struct RdrGlobalResourceHandles
{
	static const RdrResourceHandle kDepthBuffer = 1;
	static const int kCount = 1;
};

struct RdrGlobalResources
{
	RdrRenderTargetView renderTargets[(int)RdrGlobalRenderTargetHandles::kCount + 1];
	RdrResourceHandle hResources[(int)RdrGlobalResourceHandles::kCount + 1];
};

namespace RdrResourceSystem
{
	void Init(Renderer& rRenderer);

	// Update the active global resources.  
	// These should only be updated on the render thread at the start of each action. 
	void SetActiveGlobalResources(const RdrGlobalResources& rResources);

	//
	const RdrGeometry* GetGeo(const RdrGeoHandle hGeo);
	RdrDepthStencilView GetDepthStencilView(const RdrDepthStencilViewHandle hView);
	RdrRenderTargetView GetRenderTargetView(const RdrRenderTargetViewHandle hView);

	const RdrResource* GetDefaultResource(const RdrDefaultResource resType);
	RdrResourceHandle GetDefaultResourceHandle(const RdrDefaultResource resType);

	const RdrResource* GetResource(const RdrResourceHandle hRes);
	const RdrResource* GetConstantBuffer(const RdrConstantBufferHandle hBuffer);
}

class RdrResourceCommandList
{
public:
	void ProcessPreFrameCommands(RdrContext* pRdrContext);
	void ProcessCleanupCommands(RdrContext* pRdrContext);

	RdrResourceHandle CreateTextureFromFile(const CachedString& texName, RdrTextureInfo* pOutInfo, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, char* pTexData, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateTexture3D(uint width, uint height, uint depth, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, char* pTexData, const RdrDebugBackpointer& debug);

	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices,
		RdrTopology eTopology, const Vec3& boundsMin, const Vec3& boundsMax, const RdrDebugBackpointer& debug);
	RdrGeoHandle UpdateGeoVerts(RdrGeoHandle hGeo, const void* pVertData, const RdrDebugBackpointer& debug);
	void ReleaseGeo(const RdrGeoHandle hGeo, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateVertexBuffer(const void* pSrcData, int stride, int numVerts, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrResourceHandle UpdateBuffer(const RdrResourceHandle hResource, const void* pSrcData, int numElements, const RdrDebugBackpointer& debug);

	RdrShaderResourceViewHandle CreateShaderResourceView(RdrResourceHandle hResource, uint firstElement, const RdrDebugBackpointer& debug);
	void ReleaseShaderResourceView(RdrShaderResourceViewHandle hView, const RdrDebugBackpointer& debug);

	RdrConstantBufferHandle CreateConstantBuffer(const void* pData, uint size, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrConstantBufferHandle CreateTempConstantBuffer(const void* pData, uint size, const RdrDebugBackpointer& debug);
	RdrConstantBufferHandle CreateUpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint size, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	void ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer, const RdrDebugBackpointer& debug);

	void ReleaseResource(RdrResourceHandle hRes, const RdrDebugBackpointer& debug);

	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource, const RdrDebugBackpointer& debug);
	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize, const RdrDebugBackpointer& debug);
	void ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView, const RdrDebugBackpointer& debug);

	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource, const RdrDebugBackpointer& debug);
	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize, const RdrDebugBackpointer& debug);
	void ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView, const RdrDebugBackpointer& debug);

	// Utility functions to create resource batches
	RdrRenderTarget InitRenderTarget2d(uint width, uint height, RdrResourceFormat eFormat, int multisampleLevel, const RdrDebugBackpointer& debug);
	void ReleaseRenderTarget2d(const RdrRenderTarget& rRenderTarget, const RdrDebugBackpointer& debug);

private:
	RdrResourceHandle CreateTextureCommon(RdrTextureType texType, uint width, uint height, uint depth,
		uint mipLevels, RdrResourceFormat eFormat, uint sampleCount, RdrResourceAccessFlags accessFlags, char* pTextureData, const RdrDebugBackpointer& debug);

private:
	// Command definitions
	struct CmdCreateTexture
	{
		RdrResourceHandle hResource;
		RdrTextureInfo texInfo;
		RdrResourceAccessFlags accessFlags;

		char* pFileData; // Pointer to start of texture data when loaded from a file.
		char* pData; // Pointer to start of raw data.
		uint dataSize;

		RdrDebugBackpointer debug;
	};

	struct CmdCreateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
		RdrBufferInfo info;
		RdrResourceAccessFlags accessFlags;

		RdrDebugBackpointer debug;
	};

	struct CmdUpdateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
		uint numElements;

		RdrDebugBackpointer debug;
	};

	struct CmdReleaseResource
	{
		RdrResourceHandle hResource;
		RdrDebugBackpointer debug;
	};

	struct CmdCreateConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
		RdrResourceAccessFlags accessFlags;
		const void* pData;
		uint size;

		RdrDebugBackpointer debug;
	};

	struct CmdUpdateConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
		const void* pData;

		RdrDebugBackpointer debug;
	};

	struct CmdReleaseConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
		RdrDebugBackpointer debug;
	};

	struct CmdCreateRenderTarget
	{
		RdrRenderTargetViewHandle hView;
		RdrResourceHandle hResource;
		int arrayStartIndex;
		int arraySize;

		RdrDebugBackpointer debug;
	};

	struct CmdReleaseRenderTarget
	{
		RdrRenderTargetViewHandle hView;
		RdrDebugBackpointer debug;
	};

	struct CmdCreateDepthStencil
	{
		RdrDepthStencilViewHandle hView;
		RdrResourceHandle hResource;
		int arrayStartIndex;
		int arraySize;

		RdrDebugBackpointer debug;
	};

	struct CmdReleaseDepthStencil
	{
		RdrDepthStencilViewHandle hView;
		RdrDebugBackpointer debug;
	};

	struct CmdCreateShaderResourceView
	{
		RdrShaderResourceViewHandle hView;
		RdrResourceHandle hResource;
		int firstElement;

		RdrDebugBackpointer debug;
	};

	struct CmdReleaseShaderResourceView
	{
		RdrShaderResourceViewHandle hView;
		RdrDebugBackpointer debug;
	};

	struct CmdUpdateGeo
	{
		RdrGeoHandle hGeo;
		const void* pVertData;
		const uint16* pIndexData;
		RdrGeoInfo info;

		RdrDebugBackpointer debug;
	};

	struct CmdReleaseGeo
	{
		RdrGeoHandle hGeo;
		RdrDebugBackpointer debug;
	};

private:
	FixedVector<CmdCreateTexture, 1024>				m_textureCreates;
	FixedVector<CmdCreateBuffer, 1024>				m_bufferCreates;
	FixedVector<CmdUpdateBuffer, 1024>				m_bufferUpdates;
	FixedVector<CmdReleaseResource, 1024>			m_resourceReleases;
	FixedVector<CmdCreateRenderTarget, 1024>		m_renderTargetCreates;
	FixedVector<CmdReleaseRenderTarget, 1024>		m_renderTargetReleases;
	FixedVector<CmdCreateDepthStencil, 1024>		m_depthStencilCreates;
	FixedVector<CmdReleaseDepthStencil, 1024>		m_depthStencilReleases;
	FixedVector<CmdCreateShaderResourceView, 1024>  m_shaderResourceViewCreates;
	FixedVector<CmdReleaseShaderResourceView, 1024> m_shaderResourceViewReleases;
	FixedVector<CmdUpdateGeo, 1024>					m_geoUpdates;
	FixedVector<CmdReleaseGeo, 1024>				m_geoReleases;
	FixedVector<CmdCreateConstantBuffer, 2048>		m_constantBufferCreates;
	FixedVector<CmdUpdateConstantBuffer, 2048>		m_constantBufferUpdates;
	FixedVector<CmdReleaseConstantBuffer, 2048>		m_constantBufferReleases;
};
