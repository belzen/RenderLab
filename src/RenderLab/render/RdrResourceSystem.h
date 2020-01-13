#pragma once

#include "RdrResource.h"
#include "RdrGeometry.h"
#include "UtilsLib/FixedVector.h"
#include "UtilsLib/StringCache.h"
#include "RdrDebugBackpointer.h"

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

struct RdrGlobalResources
{
	RdrRenderTargetView renderTargets[(int)RdrGlobalRenderTargetHandles::kCount + 1];
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

	// SRVs
	RdrDescriptors* CreateShaderResourceViewTable(const RdrResourceHandle* ahResources, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateShaderResourceViewTable(const RdrResource** apResources, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateTempShaderResourceViewTable(const RdrResource** apResources, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateTempShaderResourceViewTable(const RdrResourceHandle* ahResources, uint count, const RdrDebugBackpointer& debug);
	// UAVs
	RdrDescriptors* CreateUnorderedAccessViewTable(const RdrResourceHandle* ahResources, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateUnorderedAccessViewTable(const RdrResource** apResources, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateTempUnorderedAccessViewTable(const RdrResource** apResources, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateTempUnorderedAccessViewTable(const RdrResourceHandle* ahResources, uint count, const RdrDebugBackpointer& debug);
	// Samplers
	RdrDescriptors* CreateSamplerTable(const RdrSamplerState* aSamplers, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateTempSamplerTable(const RdrSamplerState* aSamplers, uint count, const RdrDebugBackpointer& debug);
	// Constant buffers
	RdrDescriptors* CreateConstantBufferTable(const RdrConstantBufferHandle* ahBuffers, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateConstantBufferTable(const RdrResource** apBuffers, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateTempConstantBufferTable(const RdrConstantBufferHandle* ahBuffers, uint count, const RdrDebugBackpointer& debug);
	RdrDescriptors* CreateTempConstantBufferTable(const RdrResource** apBuffers, uint count, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateTextureFromFile(const CachedString& texName, RdrTextureInfo* pOutInfo, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, char* pTexData, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateTexture3D(uint width, uint height, uint depth, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, char* pTexData, const RdrDebugBackpointer& debug);

	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const void* pIndexData, int numIndices, RdrIndexBufferFormat eIndexFormat,
		RdrTopology eTopology, const Vec3& boundsMin, const Vec3& boundsMax, const RdrDebugBackpointer& debug);
	RdrGeoHandle CreateGeo(RdrResourceHandle hVertexBuffer, uint nVertexStride, uint nVertexStartByteOffset, uint nVertexCount, 
		RdrResourceHandle hIndexBuffer, uint nIndexStart, uint nIndexCount, RdrIndexBufferFormat eIndexFormat,
		RdrTopology eTopology, const Vec3& boundsMin, const Vec3& boundsMax, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateVertexBuffer(const void* pSrcData, int stride, int numVerts, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateIndexBuffer(const void* pSrcData, uint nSizeInBytes, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	RdrResourceHandle CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	RdrShaderResourceViewHandle CreateShaderResourceView(RdrResourceHandle hResource, uint firstElement, const RdrDebugBackpointer& debug);

	RdrConstantBufferHandle CreateConstantBuffer(const void* pData, uint size, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);
	RdrConstantBufferHandle CreateTempConstantBuffer(const void* pData, uint size, const RdrDebugBackpointer& debug);
	RdrConstantBufferHandle CreateUpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint size, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug);

	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource, const RdrDebugBackpointer& debug);
	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize, const RdrDebugBackpointer& debug);

	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource, const RdrDebugBackpointer& debug);
	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize, const RdrDebugBackpointer& debug);

	RdrRenderTarget InitRenderTarget2d(uint width, uint height, RdrResourceFormat eFormat, int multisampleLevel, const RdrDebugBackpointer& debug);
}

class RdrResourceCommandList
{
public:
	void ProcessPreFrameCommands(RdrContext* pRdrContext);
	void ProcessCleanupCommands(RdrContext* pRdrContext);

	void UpdateGeoVerts(RdrGeoHandle hGeo, const void* pVertData, const RdrDebugBackpointer& debug);
	void ReleaseGeo(const RdrGeoHandle hGeo, const RdrDebugBackpointer& debug);

	void UpdateBuffer(const RdrResourceHandle hResource, const void* pSrcData, int numElements, const RdrDebugBackpointer& debug);

	void ReleasePipelineState(RdrPipelineState* pPipelineState, const RdrDebugBackpointer& debug);

	void ReleaseShaderResourceView(RdrShaderResourceViewHandle hView, const RdrDebugBackpointer& debug);
	void ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer, const RdrDebugBackpointer& debug);
	void ReleaseResource(RdrResourceHandle hRes, const RdrDebugBackpointer& debug);
	void ReleaseDescriptorTable(RdrDescriptors* pTable, const RdrDebugBackpointer& debug);
	void ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView, const RdrDebugBackpointer& debug);
	void ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView, const RdrDebugBackpointer& debug);

	void ReleaseRenderTarget2d(const RdrRenderTarget& rRenderTarget, const RdrDebugBackpointer& debug);

	void UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint dataSize, const RdrDebugBackpointer& debug);
	void UpdateResource(RdrResourceHandle hResource, const void* pData, uint dataSize, const RdrDebugBackpointer& debug);
	void UpdateResourceFromFile(RdrResourceHandle hResource, const void* pFileData, uint nDataStartOffset, uint dataSize, const RdrDebugBackpointer& debug);

private:
	// Command definitions
	struct CmdUpdateResource
	{
		RdrResourceHandle hResource;
		const void* pData;
		uint dataSize;

		const void* pFileData = nullptr;
		RdrDebugBackpointer debug;
	};

	struct CmdReleaseResource
	{
		RdrResourceHandle hResource;
		RdrDebugBackpointer debug;
	};

	struct CmdReleaseDescriptorTable
	{
		RdrDescriptors* pTable;
		RdrDebugBackpointer debug;
	};

	struct CmdUpdateConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
		const void* pData;
		uint dataSize;

		RdrDebugBackpointer debug;
	};

	struct CmdReleaseConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
		RdrDebugBackpointer debug;
	};

	struct CmdReleaseRenderTarget
	{
		RdrRenderTargetViewHandle hView;
		RdrDebugBackpointer debug;
	};

	struct CmdReleaseDepthStencil
	{
		RdrDepthStencilViewHandle hView;
		RdrDebugBackpointer debug;
	};

	struct CmdReleaseShaderResourceView
	{
		RdrShaderResourceViewHandle hView;
		RdrDebugBackpointer debug;
	};

	struct CmdReleaseGeo
	{
		RdrGeoHandle hGeo;
		RdrDebugBackpointer debug;
	};

	struct CmdReleasePipelineState
	{
		RdrPipelineState* pPipelineState;
		RdrDebugBackpointer debug;
	};

private:
	FixedVector<CmdUpdateResource,			  8192>	m_resourceUpdates;
	FixedVector<CmdReleaseResource,			  2048>	m_resourceReleases;
	FixedVector<CmdReleaseDescriptorTable,	  2048>	m_descTableReleases;
	FixedVector<CmdReleaseRenderTarget,		  1024>	m_renderTargetReleases;
	FixedVector<CmdReleaseDepthStencil,		  1024>	m_depthStencilReleases;
	FixedVector<CmdReleaseShaderResourceView, 2048>	m_shaderResourceViewReleases;
	FixedVector<CmdReleaseGeo,				  2048>	m_geoReleases;
	FixedVector<CmdUpdateConstantBuffer,	  2048>	m_constantBufferUpdates;
	FixedVector<CmdReleaseConstantBuffer,	  2048>	m_constantBufferReleases;
	FixedVector<CmdReleasePipelineState,	  2048>	m_pipelineStateReleases;
};
