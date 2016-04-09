#pragma once

#include <map>
#include <vector>
#include "RdrResource.h"
#include "FreeList.h"

class RdrContext;

typedef std::map<std::string, RdrResourceHandle> RdrResourceHandleMap;
typedef FreeList<RdrResource, 1024> RdrResourceList;
typedef FreeList<RdrConstantBuffer, 1024> RdrConstantBufferList;
typedef FreeList<RdrRenderTargetView, 128> RdrRenderTargetViewList;
typedef FreeList<RdrDepthStencilView, 128> RdrDepthStencilViewList;

class RdrResourceSystem
{
public:
	static const uint kPrimaryRenderTargetHandle = 1;

	void Init(RdrContext* pRdrContext);

	RdrResourceHandle CreateTextureFromFile(const char* filename, RdrTextureInfo* pOutInfo);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat);
	RdrResourceHandle CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount);
	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat);

	RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat);
	RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat);

	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, bool bFreeData, RdrResourceUsage eUsage);
	RdrResourceHandle UpdateStructuredBuffer(const RdrResourceHandle hResource, const void* pSrcData, bool bFreeData);

	RdrConstantBufferHandle CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	RdrConstantBufferHandle CreateTempConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	void UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData);
	void ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer);

	void ReleaseResource(RdrResourceHandle hRes);

	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource);
	RdrDepthStencilViewHandle CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize);
	RdrDepthStencilView GetDepthStencilView(const RdrDepthStencilViewHandle hView);
	void ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView);

	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource);
	RdrRenderTargetViewHandle CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize);
	RdrRenderTargetView GetRenderTargetView(const RdrRenderTargetViewHandle hView);
	void ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView);

	const RdrResource* GetResource(const RdrResourceHandle hRes);
	const RdrConstantBuffer* GetConstantBuffer(const RdrConstantBufferHandle hBuffer);

	void FlipState();
	void ProcessCommands();

private:
	struct CmdCreateTexture
	{
		RdrResourceHandle hResource;
		RdrTextureInfo texInfo;
		RdrResourceUsage eUsage;

		void* pHeaderData; // Pointer to start of texture data when loaded from a file.
		void* pData; // Pointer to start of raw data.
		uint dataSize;
		bool bFreeData;
	};

	struct CmdCreateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
		uint elementSize;
		uint numElements;
		RdrResourceUsage eUsage;
		bool bFreeData;
	};

	struct CmdUpdateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
		bool bFreeData;
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
		bool bIsTemp;
	};

	struct CmdUpdateConstantBuffer
	{
		RdrConstantBufferHandle hBuffer;
		const void* pData;
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

	struct FrameState
	{
		std::vector<CmdCreateTexture>         textureCreates;
		std::vector<CmdCreateBuffer>          bufferCreates;
		std::vector<CmdUpdateBuffer>          bufferUpdates;
		std::vector<CmdReleaseResource>       resourceReleases;
		std::vector<CmdCreateConstantBuffer>  constantBufferCreates;
		std::vector<CmdUpdateConstantBuffer>  constantBufferUpdates;
		std::vector<CmdReleaseConstantBuffer> constantBufferReleases;
		std::vector<CmdCreateRenderTarget>    renderTargetCreates;
		std::vector<CmdReleaseRenderTarget>   renderTargetReleases;
		std::vector<CmdCreateDepthStencil>    depthStencilCreates;
		std::vector<CmdReleaseDepthStencil>   depthStencilReleases;

		std::vector<RdrConstantBufferHandle>  tempConstantBuffers;
	};

	RdrResourceHandle CreateTextureInternal(uint width, uint height, uint mipLevels, uint ararySize, RdrResourceFormat eFormat, uint sampleCount, bool bCubemap);

private:
	RdrContext* m_pRdrContext;

	RdrResourceHandleMap m_textureCache;
	RdrResourceList      m_resources;

	RdrConstantBufferList m_constantBuffers;

	RdrRenderTargetViewList m_renderTargetViews;
	RdrDepthStencilViewList m_depthStencilViews;

	FrameState m_states[2];
	uint       m_queueState;
};