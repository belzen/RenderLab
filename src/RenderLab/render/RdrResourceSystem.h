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

namespace RdrResourceSystem
{
	static const uint kPrimaryRenderTargetHandle = 1;

	void Init(Renderer& rRenderer);

	const RdrGeometry* GetGeo(const RdrGeoHandle hGeo);
	RdrDepthStencilView GetDepthStencilView(const RdrDepthStencilViewHandle hView);
	RdrRenderTargetView GetRenderTargetView(const RdrRenderTargetViewHandle hView);

	const RdrResource* GetDefaultResource(const RdrDefaultResource resType);
	RdrResourceHandle GetDefaultResourceHandle(const RdrDefaultResource resType);

	const RdrResource* GetResource(const RdrResourceHandle hRes);
	const RdrConstantBuffer* GetConstantBuffer(const RdrConstantBufferHandle hBuffer);
}

class RdrResourceCommandList
{
public:
	void ProcessCommands(RdrContext* pRdrContext);

	RdrResourceHandle CreateTextureFromFile(const CachedString& texName, RdrTextureInfo* pOutInfo);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings, char* pTexData);
	RdrResourceHandle CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount, RdrResourceUsage eUsage, RdrResourceBindings eBindings);
	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings);

	RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings);
	RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings);

	RdrResourceHandle CreateTexture3D(uint width, uint height, uint depth, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings, char* pTexData);

	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices,
		RdrTopology eTopology, const Vec3& boundsMin, const Vec3& boundsMax);
	void ReleaseGeo(const RdrGeoHandle hGeo);

	RdrResourceHandle CreateVertexBuffer(const void* pSrcData, int stride, int numVerts, RdrResourceUsage eUsage);

	RdrResourceHandle CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceUsage eUsage);
	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage);
	RdrResourceHandle UpdateBuffer(const RdrResourceHandle hResource, const void* pSrcData, int numElements = -1);

	RdrShaderResourceViewHandle CreateShaderResourceView(RdrResourceHandle hResource, uint firstElement);
	void ReleaseShaderResourceView(RdrShaderResourceViewHandle hView);

	RdrConstantBufferHandle CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	RdrConstantBufferHandle CreateTempConstantBuffer(const void* pData, uint size);
	RdrConstantBufferHandle CreateUpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	void UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData);
	void ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer);

	void ReleaseResource(RdrResourceHandle hRes);

	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource);
	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize);
	void ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView);

	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource);
	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize);
	void ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView);

private:
	RdrResourceHandle CreateTextureCommon(RdrTextureType texType, uint width, uint height, uint depth,
		uint mipLevels, RdrResourceFormat eFormat, uint sampleCount, RdrResourceUsage eUsage, RdrResourceBindings eBindings, char* pTextureData);

private:
	// Command definitions
	struct CmdCreateTexture
	{
		RdrResourceHandle hResource;
		RdrTextureInfo texInfo;
		RdrResourceUsage eUsage;
		RdrResourceBindings eBindings;

		char* pFileData; // Pointer to start of texture data when loaded from a file.
		char* pData; // Pointer to start of raw data.
		uint dataSize;
	};

	struct CmdCreateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
		RdrBufferInfo info;
		RdrResourceUsage eUsage;
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
		RdrCpuAccessFlags cpuAccessFlags;
		RdrResourceUsage eUsage;
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
