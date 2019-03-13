#pragma once

#include "RdrResource.h"
#include "RdrGeometry.h"
#include "UtilsLib/FixedVector.h"
#include "UtilsLib/StringCache.h"

class Renderer;
class RdrContext;

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
	void ProcessCommands(RdrContext* pRdrContext);

	RdrResourceHandle CreateTextureFromFile(const CachedString& texName, RdrTextureInfo* pOutInfo);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, char* pTexData);
	RdrResourceHandle CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount, RdrResourceAccessFlags accessFlags);
	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags);

	RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags);
	RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags);

	RdrResourceHandle CreateTexture3D(uint width, uint height, uint depth, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, char* pTexData);

	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices,
		RdrTopology eTopology, const Vec3& boundsMin, const Vec3& boundsMax);
	RdrGeoHandle UpdateGeoVerts(RdrGeoHandle hGeo, const void* pVertData);
	void ReleaseGeo(const RdrGeoHandle hGeo);

	RdrResourceHandle CreateVertexBuffer(const void* pSrcData, int stride, int numVerts, RdrResourceAccessFlags accessFlags);

	RdrResourceHandle CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags);
	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceAccessFlags accessFlags);
	RdrResourceHandle UpdateBuffer(const RdrResourceHandle hResource, const void* pSrcData, int numElements = -1);

	RdrShaderResourceViewHandle CreateShaderResourceView(RdrResourceHandle hResource, uint firstElement);
	void ReleaseShaderResourceView(RdrShaderResourceViewHandle hView);

	RdrConstantBufferHandle CreateConstantBuffer(const void* pData, uint size, RdrResourceAccessFlags accessFlags);
	RdrConstantBufferHandle CreateTempConstantBuffer(const void* pData, uint size);
	RdrConstantBufferHandle CreateUpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint size, RdrResourceAccessFlags accessFlags);
	void UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData);
	void ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer);

	void ReleaseResource(RdrResourceHandle hRes);

	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource);
	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize);
	void ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView);

	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource);
	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize);
	void ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView);

	// Utility functions to create resource batches
	RdrRenderTarget InitRenderTarget2d(uint width, uint height, RdrResourceFormat eFormat, int multisampleLevel);
	void ReleaseRenderTarget2d(const RdrRenderTarget& rRenderTarget);

private:
	RdrResourceHandle CreateTextureCommon(RdrTextureType texType, uint width, uint height, uint depth,
		uint mipLevels, RdrResourceFormat eFormat, uint sampleCount, RdrResourceAccessFlags accessFlags, char* pTextureData);

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
	};

	struct CmdCreateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
		RdrBufferInfo info;
		RdrResourceAccessFlags accessFlags;
	};

	struct CmdUpdateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
		uint numElements;
	};

	struct CmdReleaseResource
	{
		RdrResourceHandle hResource;
	};

	struct CmdCreateConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
		RdrResourceAccessFlags accessFlags;
		const void* pData;
		uint size;
	};

	struct CmdUpdateConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
		const void* pData;
		bool bIsTemp;
	};

	struct CmdReleaseConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
	};

	struct CmdCreateRenderTarget
	{
		RdrRenderTargetViewHandle hView;
		RdrResourceHandle hResource;
		int arrayStartIndex;
		int arraySize;
	};

	struct CmdReleaseRenderTarget
	{
		RdrRenderTargetViewHandle hView;
	};

	struct CmdCreateDepthStencil
	{
		RdrDepthStencilViewHandle hView;
		RdrResourceHandle hResource;
		int arrayStartIndex;
		int arraySize;
	};

	struct CmdReleaseDepthStencil
	{
		RdrDepthStencilViewHandle hView;
	};

	struct CmdCreateShaderResourceView
	{
		RdrShaderResourceViewHandle hView;
		RdrResourceHandle hResource;
		int firstElement;
	};

	struct CmdReleaseShaderResourceView
	{
		RdrShaderResourceViewHandle hView;
	};

	struct CmdUpdateGeo
	{
		RdrGeoHandle hGeo;
		const void* pVertData;
		const uint16* pIndexData;
		RdrGeoInfo info;
	};

	struct CmdReleaseGeo
	{
		RdrGeoHandle hGeo;
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
