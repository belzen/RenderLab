#include "Precompiled.h"
#include "RdrResourceSystem.h"
#include "RdrContext.h"
#include "RdrTransientMem.h"
#include "FileLoader.h"
#include "json/json.h"

#include <DirectXTex/include/DirectXTex.h>
#include <DirectXTex/include/Dds.h>

#pragma comment (lib, "DirectXTex.lib")

namespace
{
	typedef std::map<std::string, RdrResourceHandle> RdrResourceHandleMap;

	static const uint kMaxConstantBuffersPerPool = 128;
	static const uint kNumConstantBufferPools = 10;

	struct CmdCreateTexture
	{
		RdrResourceHandle hResource;
		RdrTextureInfo texInfo;
		RdrResourceUsage eUsage;

		char* pHeaderData; // Pointer to start of texture data when loaded from a file.
		char* pData; // Pointer to start of raw data.
		uint dataSize;
	};

	struct CmdCreateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
		uint elementSize;
		uint numElements;
		RdrResourceUsage eUsage;
	};

	struct CmdUpdateBuffer
	{
		RdrResourceHandle hResource;
		const void* pData;
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

	struct ConstantBufferPool
	{
		RdrConstantBufferHandle ahBuffers[kMaxConstantBuffersPerPool];
		uint bufferCount;
	};

	struct 
	{
		RdrResourceHandleMap textureCache;
		RdrResourceList      resources;

		RdrConstantBufferList constantBuffers;
		ConstantBufferPool constantBufferPools[kNumConstantBufferPools];

		RdrRenderTargetViewList renderTargetViews;
		RdrDepthStencilViewList depthStencilViews;
		RdrRenderTargetView primaryRenderTarget;

		FrameState states[2];
		uint       queueState;
	} s_resourceSystem;

	inline FrameState& getQueueState()
	{
		return s_resourceSystem.states[s_resourceSystem.queueState];
	}

	RdrResourceFormat getFormatFromDXGI(DXGI_FORMAT format, bool bIsSrgb)
	{
		switch (format)
		{
		case DXGI_FORMAT_D16_UNORM:
			return RdrResourceFormat::D16;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			return RdrResourceFormat::D24_UNORM_S8_UINT;
		case DXGI_FORMAT_R16_UNORM:
			return RdrResourceFormat::R16_UNORM;
		case DXGI_FORMAT_R16G16_FLOAT:
			return RdrResourceFormat::R16G16_FLOAT;
		case DXGI_FORMAT_R8_UNORM:
			return RdrResourceFormat::R8_UNORM;
		case DXGI_FORMAT_BC3_UNORM:
			return bIsSrgb ? RdrResourceFormat::DXT5_sRGB : RdrResourceFormat::DXT5;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return bIsSrgb ? RdrResourceFormat::B8G8R8A8_UNORM_sRGB : RdrResourceFormat::B8G8R8A8_UNORM;
		default:
			assert(false);
			return RdrResourceFormat::Count;
		}
	}

	RdrResourceHandle createTextureInternal(uint width, uint height, uint mipLevels, uint arraySize, RdrResourceFormat eFormat, uint sampleCount, bool bCubemap, RdrResourceUsage eUsage)
	{
		RdrResource* pResource = s_resourceSystem.resources.allocSafe();

		CmdCreateTexture cmd = { 0 };
		cmd.hResource = s_resourceSystem.resources.getId(pResource);
		cmd.eUsage = eUsage;
		cmd.texInfo.format = eFormat;
		cmd.texInfo.width = width;
		cmd.texInfo.height = height;
		cmd.texInfo.mipLevels = mipLevels;
		cmd.texInfo.arraySize = arraySize;
		cmd.texInfo.sampleCount = sampleCount;
		cmd.texInfo.bCubemap = bCubemap;

		getQueueState().textureCreates.push_back(cmd);

		return cmd.hResource;
	}
}

void RdrResourceSystem::Init()
{
	// Reserve id 1 for the primary render target.
	s_resourceSystem.renderTargetViews.allocSafe();
}

RdrResourceHandle RdrResourceSystem::CreateTextureFromFile(const char* texName, RdrTextureInfo* pOutInfo)
{
	// Find texture in cache
	RdrResourceHandleMap::iterator iter = s_resourceSystem.textureCache.find(texName);
	if (iter != s_resourceSystem.textureCache.end())
		return iter->second;

	CmdCreateTexture cmd = { 0 };

	char filepath[MAX_PATH];
	sprintf_s(filepath, "data/textures/%s.texture", texName);

	// Read in settings from ".texture" file, including the name of the actual image file.
	Json::Value jRoot;
	if (!FileLoader::LoadJson(filepath, jRoot))
	{
		assert(false);
		return 0;
	}

	Json::Value jTexFilename = jRoot.get("filename", Json::Value::null);
	sprintf_s(filepath, "data/textures/%s", jTexFilename.asCString());

	bool bIsSrgb = jRoot.get("srgb", false).asBool();
	bool bIsCubemap = jRoot.get("isCubemap", false).asBool();

	// Load the texture image file.
	if (!FileLoader::Load(filepath, &cmd.pHeaderData, &cmd.dataSize))
	{
		assert(false);
		return 0;
	}

	char* pPos = (char*)cmd.pHeaderData;
	
	DirectX::TexMetadata metadata;
	DirectX::GetMetadataFromDDSMemory(cmd.pHeaderData, cmd.dataSize, 0, metadata);

	// Create new texture info
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();
	pResource->texInfo.filename = _strdup(texName); // todo: string cache
	cmd.hResource = s_resourceSystem.resources.getId(pResource);

	cmd.eUsage = RdrResourceUsage::Immutable;
	cmd.texInfo.format = getFormatFromDXGI(metadata.format, bIsSrgb);
	cmd.texInfo.width = (uint)metadata.width;
	cmd.texInfo.height = (uint)metadata.height;
	cmd.texInfo.mipLevels = (uint)metadata.mipLevels;
	cmd.texInfo.arraySize = (uint)metadata.arraySize;
	cmd.texInfo.bCubemap = bIsCubemap;
	cmd.texInfo.sampleCount = 1;

	if (bIsCubemap)
	{
		cmd.texInfo.arraySize /= 6;
	}

	pPos += sizeof(DWORD); // magic?
	pPos += sizeof(DirectX::DDS_HEADER);

	cmd.pData = pPos;

	getQueueState().textureCreates.push_back(cmd);

	s_resourceSystem.textureCache.insert(std::make_pair(texName, cmd.hResource));

	if (pOutInfo)
	{
		*pOutInfo = cmd.texInfo;
	}
	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceUsage eUsage)
{
	return createTextureInternal(width, height, 1, 1, eFormat, 1, false, eUsage);
}

RdrResourceHandle RdrResourceSystem::CreateTexture2DMS(uint width, uint height, RdrResourceFormat eFormat, uint sampleCount)
{
	return createTextureInternal(width, height, 1, 1, eFormat, sampleCount, false, RdrResourceUsage::Default);
}

RdrResourceHandle RdrResourceSystem::CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat)
{
	return createTextureInternal(width, height, 1, arraySize, eFormat, 1, false, RdrResourceUsage::Default);
}

RdrResourceHandle RdrResourceSystem::CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat)
{
	return createTextureInternal(width, height, 1, 1, eFormat, 1, true, RdrResourceUsage::Default);
}

RdrResourceHandle RdrResourceSystem::CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat)
{
	return createTextureInternal(width, height, 1, arraySize, eFormat, 1, true, RdrResourceUsage::Default);
}

RdrResourceHandle RdrResourceSystem::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	CmdCreateBuffer cmd = { 0 };
	cmd.hResource = s_resourceSystem.resources.getId(pResource);
	cmd.pData = pSrcData;
	cmd.numElements = numElements;
	cmd.elementSize = elementSize;
	cmd.eUsage = eUsage;

	getQueueState().bufferCreates.push_back(cmd);

	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::UpdateStructuredBuffer(const RdrResourceHandle hResource, const void* pSrcData)
{
	CmdUpdateBuffer cmd = { 0 };
	cmd.hResource = hResource;
	cmd.pData = pSrcData;

	getQueueState().bufferUpdates.push_back(cmd);

	return cmd.hResource;
}

RdrConstantBufferHandle RdrResourceSystem::CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.allocSafe();

	CmdCreateConstantBuffer cmd = { 0 };
	cmd.hBuffer = s_resourceSystem.constantBuffers.getId(pBuffer);
	cmd.pData = pData;
	cmd.cpuAccessFlags = cpuAccessFlags;
	cmd.eUsage = eUsage;
	cmd.size = size;

	getQueueState().constantBufferCreates.push_back(cmd);

	return cmd.hBuffer;
}

RdrConstantBufferHandle RdrResourceSystem::CreateTempConstantBuffer(const void* pData, uint size)
{
	assert(size % sizeof(Vec4) == 0);

	uint poolIndex = size / sizeof(Vec4);
	if (poolIndex < kNumConstantBufferPools)
	{
		ConstantBufferPool& rPool = s_resourceSystem.constantBufferPools[poolIndex];
		if (rPool.bufferCount > 0)
		{
			RdrConstantBufferHandle hBuffer = rPool.ahBuffers[--rPool.bufferCount];

			// Queue update command with temp flag.
			CmdUpdateConstantBuffer cmd = { 0 };
			cmd.hBuffer = hBuffer;
			cmd.pData = pData;
			cmd.bIsTemp = true;
			getQueueState().constantBufferUpdates.push_back(cmd);

			return hBuffer;
		}
	}

	RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.allocSafe();

	CmdCreateConstantBuffer cmd = { 0 };
	cmd.hBuffer = s_resourceSystem.constantBuffers.getId(pBuffer);
	cmd.pData = pData;
	cmd.cpuAccessFlags = RdrCpuAccessFlags::Write;
	cmd.eUsage = RdrResourceUsage::Dynamic;
	cmd.size = size;
	cmd.bIsTemp = true;

	getQueueState().constantBufferCreates.push_back(cmd);

	return cmd.hBuffer;
}

void RdrResourceSystem::UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData)
{
	CmdUpdateConstantBuffer cmd = { 0 };
	cmd.hBuffer = hBuffer;
	cmd.pData = pData;

	getQueueState().constantBufferUpdates.push_back(cmd);
}

void RdrResourceSystem::ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer)
{
	CmdReleaseConstantBuffer cmd = { 0 };
	cmd.hBuffer = hBuffer;

	getQueueState().constantBufferReleases.push_back(cmd);
}

void RdrResourceSystem::ReleaseResource(RdrResourceHandle hRes)
{
	CmdReleaseResource cmd = { 0 };
	cmd.hResource = hRes;

	getQueueState().resourceReleases.push_back(cmd);
}

const RdrResource* RdrResourceSystem::GetResource(const RdrResourceHandle hRes)
{
	return s_resourceSystem.resources.get(hRes);
}

const RdrConstantBuffer* RdrResourceSystem::GetConstantBuffer(const RdrConstantBufferHandle hBuffer)
{
	return s_resourceSystem.constantBuffers.get(hBuffer);
}

RdrRenderTargetView RdrResourceSystem::GetRenderTargetView(const RdrRenderTargetViewHandle hView)
{
	return (hView == kPrimaryRenderTargetHandle) ? s_resourceSystem.primaryRenderTarget : *s_resourceSystem.renderTargetViews.get(hView);
}

RdrRenderTargetViewHandle RdrResourceSystem::CreateRenderTargetView(RdrResourceHandle hResource)
{
	assert(hResource);

	RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.allocSafe();

	CmdCreateRenderTarget cmd = { 0 };
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.renderTargetViews.getId(pView);
	cmd.arrayStartIndex = -1;

	getQueueState().renderTargetCreates.push_back(cmd);

	return cmd.hView;
}

RdrRenderTargetViewHandle RdrResourceSystem::CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize)
{
	assert(hResource);

	RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.allocSafe();

	CmdCreateRenderTarget cmd = { 0 };
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.renderTargetViews.getId(pView);
	cmd.arrayStartIndex = arrayStartIndex;
	cmd.arraySize = arraySize;

	getQueueState().renderTargetCreates.push_back(cmd);

	return cmd.hView;
}

void RdrResourceSystem::ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView)
{
	CmdReleaseRenderTarget cmd = { 0 };
	cmd.hView = hView;

	getQueueState().renderTargetReleases.push_back(cmd);
}

RdrDepthStencilView RdrResourceSystem::GetDepthStencilView(const RdrDepthStencilViewHandle hView)
{
	return *s_resourceSystem.depthStencilViews.get(hView);
}

void RdrResourceSystem::ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView)
{
	CmdReleaseDepthStencil cmd = { 0 };
	cmd.hView = hView;

	getQueueState().depthStencilReleases.push_back(cmd);
}

RdrDepthStencilViewHandle RdrResourceSystem::CreateDepthStencilView(RdrResourceHandle hResource)
{
	assert(hResource);

	RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.allocSafe();

	CmdCreateDepthStencil cmd = { 0 };
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.depthStencilViews.getId(pView);
	cmd.arrayStartIndex = -1;

	getQueueState().depthStencilCreates.push_back(cmd);

	return cmd.hView;
}

RdrDepthStencilViewHandle RdrResourceSystem::CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize)
{
	assert(hResource);

	RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.allocSafe();

	CmdCreateDepthStencil cmd = { 0 };
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.depthStencilViews.getId(pView);
	cmd.arrayStartIndex = arrayStartIndex;
	cmd.arraySize = arraySize;

	getQueueState().depthStencilCreates.push_back(cmd);

	return cmd.hView;
}

void RdrResourceSystem::FlipState(RdrContext* pRdrContext)
{
	// Free temp constant buffers for this frame.
	FrameState& state = s_resourceSystem.states[!s_resourceSystem.queueState];
	uint numTempBuffers = (uint)state.tempConstantBuffers.size();
	for (uint i = 0; i < numTempBuffers; ++i)
	{
		RdrConstantBufferHandle hBuffer = state.tempConstantBuffers[i];
		RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.get(hBuffer);
		uint poolIndex = pBuffer->size / sizeof(Vec4);
		if (poolIndex < kNumConstantBufferPools && s_resourceSystem.constantBufferPools[poolIndex].bufferCount < kMaxConstantBuffersPerPool)
		{
			ConstantBufferPool& rPool = s_resourceSystem.constantBufferPools[poolIndex];
			rPool.ahBuffers[rPool.bufferCount] = hBuffer;
			++rPool.bufferCount;
		}
		else
		{
			pRdrContext->ReleaseConstantBuffer(pBuffer->bufferObj);
			s_resourceSystem.constantBuffers.releaseId(hBuffer);
		}
	}
	state.tempConstantBuffers.clear();

	// Flip state
	s_resourceSystem.queueState = !s_resourceSystem.queueState;
}

void RdrResourceSystem::ProcessCommands(RdrContext* pRdrContext)
{
	FrameState& state = s_resourceSystem.states[!s_resourceSystem.queueState];
	uint numCmds;

	// Update primary render target for this frame in case it changed.
	s_resourceSystem.primaryRenderTarget = pRdrContext->GetPrimaryRenderTarget();

	// Free resources
	s_resourceSystem.resources.AcquireLock();
	{
		numCmds = (uint)state.resourceReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseResource& cmd = state.resourceReleases[i];
			RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
			pRdrContext->ReleaseResource(*pResource);
			s_resourceSystem.resources.releaseId(cmd.hResource);
		}
	}
	s_resourceSystem.resources.ReleaseLock();

	// Free render targets
	numCmds = (uint)state.renderTargetReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseRenderTarget& cmd = state.renderTargetReleases[i];
		RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.get(cmd.hView);
		pRdrContext->ReleaseRenderTargetView(*pView);
		s_resourceSystem.renderTargetViews.releaseIdSafe(cmd.hView);
	}

	// Free depth stencils
	numCmds = (uint)state.depthStencilReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseDepthStencil& cmd = state.depthStencilReleases[i];
		RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.get(cmd.hView);
		pRdrContext->ReleaseDepthStencilView(*pView);
		s_resourceSystem.depthStencilViews.releaseIdSafe(cmd.hView);
	}

	// Free constant buffers
	s_resourceSystem.constantBuffers.AcquireLock();
	{
		numCmds = (uint)state.constantBufferReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseConstantBuffer& cmd = state.constantBufferReleases[i];
			RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);
			pRdrContext->ReleaseConstantBuffer(pBuffer->bufferObj);
			s_resourceSystem.constantBuffers.releaseId(cmd.hBuffer);
		}
	}
	s_resourceSystem.constantBuffers.ReleaseLock();

	// Create textures
	numCmds = (uint)state.textureCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateTexture& cmd = state.textureCreates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		pRdrContext->CreateTexture(cmd.pData, cmd.texInfo, cmd.eUsage, *pResource);
		pResource->texInfo = cmd.texInfo;

		SAFE_DELETE(cmd.pHeaderData);
	}

	// Update buffers
	numCmds = (uint)state.bufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateBuffer& cmd = state.bufferUpdates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		pRdrContext->UpdateStructuredBuffer(cmd.pData, *pResource);
	}

	// Create buffers
	numCmds = (uint)state.bufferCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateBuffer& cmd = state.bufferCreates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		pResource->bufferInfo.elementSize = cmd.elementSize;
		pResource->bufferInfo.numElements = cmd.numElements;
		pRdrContext->CreateStructuredBuffer(cmd.pData, cmd.numElements, cmd.elementSize, cmd.eUsage, *pResource);
	}

	// Create render targets
	numCmds = (uint)state.renderTargetCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateRenderTarget& cmd = state.renderTargetCreates[i];
		RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.get(cmd.hView);
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		if (cmd.arrayStartIndex >= 0)
		{
			*pView = pRdrContext->CreateRenderTargetView(*pResource, cmd.arrayStartIndex, cmd.arraySize);
		}
		else
		{
			*pView = pRdrContext->CreateRenderTargetView(*pResource);
		}
	}

	// Create depth stencils
	numCmds = (uint)state.depthStencilCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateDepthStencil& cmd = state.depthStencilCreates[i];
		RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.get(cmd.hView);
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		if (cmd.arrayStartIndex >= 0)
		{
			*pView = pRdrContext->CreateDepthStencilView(*pResource, cmd.arrayStartIndex, cmd.arraySize);
		}
		else
		{
			*pView = pRdrContext->CreateDepthStencilView(*pResource);
		}
	}

	// Create constant buffers
	numCmds = (uint)state.constantBufferCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateConstantBuffer& cmd = state.constantBufferCreates[i];
		RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);
		pBuffer->bufferObj = pRdrContext->CreateConstantBuffer(cmd.pData, cmd.size, cmd.cpuAccessFlags, cmd.eUsage);
		pBuffer->size = cmd.size;

		if (cmd.bIsTemp)
		{
			state.tempConstantBuffers.push_back(cmd.hBuffer);
		}
	}

	// Update constant buffers
	numCmds = (uint)state.constantBufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateConstantBuffer& cmd = state.constantBufferUpdates[i];
		RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);
		pRdrContext->UpdateConstantBuffer(*pBuffer, cmd.pData);

		if (cmd.bIsTemp)
		{
			state.tempConstantBuffers.push_back(cmd.hBuffer);
		}
	}


	state.textureCreates.clear();
	state.bufferCreates.clear();
	state.bufferUpdates.clear();
	state.resourceReleases.clear();
	state.constantBufferCreates.clear();
	state.constantBufferUpdates.clear();
	state.constantBufferReleases.clear();
	state.renderTargetCreates.clear();
	state.renderTargetReleases.clear();
	state.depthStencilCreates.clear();
	state.depthStencilReleases.clear();
}
