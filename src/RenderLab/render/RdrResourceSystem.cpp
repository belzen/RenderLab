#include "Precompiled.h"
#include "RdrResourceSystem.h"
#include "RdrContext.h"
#include "RdrFrameMem.h"
#include "AssetLib/TextureAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "UtilsLib/Hash.h"
#include "UtilsLib/FixedVector.h"

#include <DirectXTex/include/DirectXTex.h>
#include <DirectXTex/include/Dds.h>

#pragma comment (lib, "DirectXTex.lib")

namespace
{
	typedef std::map<Hashing::StringHash, RdrResourceHandle> RdrResourceHandleMap;

	static const uint kMaxConstantBuffersPerPool = 128;
	static const uint kNumConstantBufferPools = 10;

	struct CmdCreateTexture
	{
		RdrResourceHandle hResource;
		RdrTextureInfo texInfo;
		RdrResourceUsage eUsage;

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

	struct ResFrameState
	{
		FixedVector<CmdCreateTexture, 1024>				textureCreates;
		FixedVector<CmdCreateBuffer, 1024>				bufferCreates;
		FixedVector<CmdUpdateBuffer, 1024>				bufferUpdates;
		FixedVector<CmdReleaseResource, 1024>			resourceReleases;
		FixedVector<CmdCreateRenderTarget, 1024>		renderTargetCreates;
		FixedVector<CmdReleaseRenderTarget, 1024>		renderTargetReleases;
		FixedVector<CmdCreateDepthStencil, 1024>		depthStencilCreates;
		FixedVector<CmdReleaseDepthStencil, 1024>		depthStencilReleases;
		FixedVector<CmdCreateShaderResourceView, 1024>  shaderResourceViewCreates;
		FixedVector<CmdReleaseShaderResourceView, 1024> shaderResourceViewReleases;
		FixedVector<CmdUpdateGeo, 1024>					geoUpdates;
		FixedVector<CmdReleaseGeo, 1024>				geoReleases;

		FixedVector<CmdCreateConstantBuffer, 2048>		constantBufferCreates;
		FixedVector<CmdUpdateConstantBuffer, 2048>		constantBufferUpdates;
		FixedVector<CmdReleaseConstantBuffer, 2048>		constantBufferReleases;
		FixedVector<RdrConstantBufferHandle, 2048>		tempConstantBuffers;
	};

	struct ConstantBufferPool
	{
		RdrConstantBufferHandle ahBuffers[kMaxConstantBuffersPerPool];
		uint bufferCount;
	};

	struct 
	{
		RdrResourceHandleMap	textureCache;
		RdrResourceHandle		defaultResourceHandles[(int)RdrDefaultResource::kNumResources];
		RdrResourceList			resources;
		RdrGeoList				geos;

		RdrConstantBufferList constantBuffers;
		ConstantBufferPool constantBufferPools[kNumConstantBufferPools];

		RdrRenderTargetViewList renderTargetViews;
		RdrDepthStencilViewList depthStencilViews;
		RdrShaderResourceViewList shaderResourceViews;
		RdrRenderTargetView primaryRenderTarget;

		ResFrameState states[2];
		uint       queueState;
	} s_resourceSystem;

	inline ResFrameState& getQueueState()
	{
		return s_resourceSystem.states[s_resourceSystem.queueState];
	}

	RdrResourceFormat getFormatFromDXGI(DXGI_FORMAT format)
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
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return RdrResourceFormat::DXT1_sRGB;
		case DXGI_FORMAT_BC1_UNORM:
			return RdrResourceFormat::DXT1;
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return RdrResourceFormat::DXT5_sRGB;
		case DXGI_FORMAT_BC3_UNORM:
			return RdrResourceFormat::DXT5;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return RdrResourceFormat::B8G8R8A8_UNORM_sRGB;
		case DXGI_FORMAT_B8G8R8A8_UNORM:
			return RdrResourceFormat::B8G8R8A8_UNORM;
		default:
			assert(false);
			return RdrResourceFormat::Count;
		}
	}

	RdrResourceHandle createTextureInternal(RdrTextureType texType, uint width, uint height, uint depth, 
		uint mipLevels, RdrResourceFormat eFormat, uint sampleCount, RdrResourceUsage eUsage, char* pTextureData)
	{
		RdrResource* pResource = s_resourceSystem.resources.allocSafe();

		CmdCreateTexture& cmd = getQueueState().textureCreates.pushSafe();
		memset(&cmd, 0, sizeof(cmd));

		cmd.hResource = s_resourceSystem.resources.getId(pResource);
		cmd.eUsage = eUsage;
		cmd.pData = pTextureData;
		cmd.texInfo.texType = texType;
		cmd.texInfo.format = eFormat;
		cmd.texInfo.width = width;
		cmd.texInfo.height = height;
		cmd.texInfo.depth = depth;
		cmd.texInfo.mipLevels = mipLevels;
		cmd.texInfo.sampleCount = sampleCount;

		return cmd.hResource;
	}
}

void RdrResourceSystem::Init()
{
	// Reserve id 1 for the primary render target.
	s_resourceSystem.renderTargetViews.allocSafe();

	// Create default resources.
	static uchar blackTexData[4] = { 0, 0, 0, 255 };
	s_resourceSystem.defaultResourceHandles[(int)RdrDefaultResource::kBlackTex2d] = 
		createTextureInternal(RdrTextureType::k2D, 1, 1, 1, 1, RdrResourceFormat::B8G8R8A8_UNORM, 1, RdrResourceUsage::Immutable, (char*)blackTexData);
	 s_resourceSystem.defaultResourceHandles[(int)RdrDefaultResource::kBlackTex3d] =
		 createTextureInternal(RdrTextureType::k3D, 1, 1, 1, 1, RdrResourceFormat::B8G8R8A8_UNORM, 1, RdrResourceUsage::Immutable, (char*)blackTexData);
}

RdrResourceHandle RdrResourceSystem::CreateTextureFromFile(const CachedString& texName, RdrTextureInfo* pOutInfo)
{
	// Find texture in cache
	RdrResourceHandleMap::iterator iter = s_resourceSystem.textureCache.find(texName.getHash());
	if (iter != s_resourceSystem.textureCache.end())
		return iter->second;

	AssetLib::Texture* pTexture = AssetLibrary<AssetLib::Texture>::LoadAsset(texName);
	if (!pTexture)
	{
		assert(false);
		return 0;
	}

	DirectX::TexMetadata metadata;
	DirectX::GetMetadataFromDDSMemory(pTexture->ddsData, pTexture->ddsDataSize, 0, metadata);

	// Create new texture info
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();
	pResource->texInfo.filename = texName.getString();

	CmdCreateTexture& cmd = getQueueState().textureCreates.pushSafe();
	memset(&cmd, 0, sizeof(cmd));
	cmd.pFileData = pTexture->ddsData;
	cmd.dataSize = pTexture->ddsDataSize;
	cmd.hResource = s_resourceSystem.resources.getId(pResource);

	cmd.eUsage = RdrResourceUsage::Immutable;
	cmd.texInfo.format = getFormatFromDXGI(metadata.format);
	cmd.texInfo.width = (uint)metadata.width;
	cmd.texInfo.height = (uint)metadata.height;
	cmd.texInfo.mipLevels = (uint)metadata.mipLevels;
	cmd.texInfo.depth = (uint)metadata.arraySize;
	cmd.texInfo.sampleCount = 1;

	if (metadata.IsCubemap())
	{
		cmd.texInfo.texType = RdrTextureType::kCube;
		cmd.texInfo.depth /= 6;
	}
	else
	{
		cmd.texInfo.texType = RdrTextureType::k2D;
	}

	DirectX::DDS_HEADER* pHeader = (DirectX::DDS_HEADER*)(pTexture->ddsData + sizeof(DWORD));
	cmd.pData = (char*)(pHeader + 1);

	if (pHeader->ddspf.dwFourCC == DirectX::DDSPF_DX10.dwFourCC)
	{
		cmd.pData += sizeof(DirectX::DDS_HEADER_DXT10);
	}

	s_resourceSystem.textureCache.insert(std::make_pair(texName.getHash(), cmd.hResource));

	if (pOutInfo)
	{
		*pOutInfo = cmd.texInfo;
	}
	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceUsage eUsage)
{
	return createTextureInternal(RdrTextureType::k2D, width, height, 1, 1, eFormat, 1, eUsage, nullptr);
}

RdrResourceHandle RdrResourceSystem::CreateTexture2DMS(uint width, uint height, RdrResourceFormat eFormat, uint sampleCount)
{
	return createTextureInternal(RdrTextureType::k2D, width, height, 1, 1, eFormat, sampleCount, RdrResourceUsage::Default, nullptr);
}

RdrResourceHandle RdrResourceSystem::CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat)
{
	return createTextureInternal(RdrTextureType::k2D, width, height, arraySize, 1, eFormat, 1, RdrResourceUsage::Default, nullptr);
}

RdrResourceHandle RdrResourceSystem::CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat)
{
	return createTextureInternal(RdrTextureType::kCube, width, height, 1, 1, eFormat, 1, RdrResourceUsage::Default, nullptr);
}

RdrResourceHandle RdrResourceSystem::CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat)
{
	return createTextureInternal(RdrTextureType::kCube, width, height, arraySize, 1, eFormat, 1, RdrResourceUsage::Default, nullptr);
}

RdrResourceHandle RdrResourceSystem::CreateTexture3D(uint width, uint height, uint depth, RdrResourceFormat eFormat, RdrResourceUsage eUsage)
{
	return createTextureInternal(RdrTextureType::k3D, width, height, depth, 1, eFormat, 1, eUsage, nullptr);
}

RdrResourceHandle RdrResourceSystem::CreateVertexBuffer(const void* pSrcData, int stride, int numVerts, RdrResourceUsage eUsage)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	CmdCreateBuffer& cmd = getQueueState().bufferCreates.pushSafe();
	cmd.hResource = s_resourceSystem.resources.getId(pResource);
	cmd.pData = pSrcData;
	cmd.eUsage = eUsage;
	cmd.info.eType = RdrBufferType::Vertex;
	cmd.info.elementSize = stride;
	cmd.info.numElements = numVerts;

	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceUsage eUsage)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	CmdCreateBuffer& cmd = getQueueState().bufferCreates.pushSafe();
	cmd.hResource = s_resourceSystem.resources.getId(pResource);
	cmd.pData = pSrcData;
	cmd.eUsage = eUsage;
	cmd.info.eType = RdrBufferType::Data;
	cmd.info.eFormat = eFormat;
	cmd.info.numElements = numElements;

	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	CmdCreateBuffer& cmd = getQueueState().bufferCreates.pushSafe();
	cmd.hResource = s_resourceSystem.resources.getId(pResource);
	cmd.pData = pSrcData;
	cmd.eUsage = eUsage;
	cmd.info.eType = RdrBufferType::Structured;
	cmd.info.elementSize = elementSize;
	cmd.info.numElements = numElements;

	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::UpdateBuffer(const RdrResourceHandle hResource, const void* pSrcData, int numElements)
{
	CmdUpdateBuffer& cmd = getQueueState().bufferUpdates.pushSafe();
	cmd.hResource = hResource;
	cmd.pData = pSrcData;
	cmd.numElements = numElements;

	return cmd.hResource;
}

RdrShaderResourceViewHandle RdrResourceSystem::CreateShaderResourceView(RdrResourceHandle hResource, uint firstElement)
{
	assert(hResource);

	RdrShaderResourceView* pView = s_resourceSystem.shaderResourceViews.allocSafe();

	CmdCreateShaderResourceView& cmd = getQueueState().shaderResourceViewCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.shaderResourceViews.getId(pView);
	cmd.firstElement = firstElement;

	return cmd.hView;
}

void RdrResourceSystem::ReleaseShaderResourceView(RdrShaderResourceViewHandle hView)
{
	CmdReleaseShaderResourceView& cmd = getQueueState().shaderResourceViewReleases.pushSafe();
	cmd.hView = hView;
}

RdrConstantBufferHandle RdrResourceSystem::CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.allocSafe();

	CmdCreateConstantBuffer& cmd = getQueueState().constantBufferCreates.pushSafe();
	cmd.hBuffer = s_resourceSystem.constantBuffers.getId(pBuffer);
	cmd.pData = pData;
	cmd.cpuAccessFlags = cpuAccessFlags;
	cmd.eUsage = eUsage;
	cmd.size = size;

	assert((size % sizeof(Vec4)) == 0);
	return cmd.hBuffer;
}

RdrConstantBufferHandle RdrResourceSystem::CreateUpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	if (hBuffer)
	{
		UpdateConstantBuffer(hBuffer, pData);
		return hBuffer;
	}
	else
	{
		return CreateConstantBuffer(pData, size, cpuAccessFlags, eUsage);
	}
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
			CmdUpdateConstantBuffer& cmd = getQueueState().constantBufferUpdates.pushSafe();
			cmd.hBuffer = hBuffer;
			cmd.pData = pData;
			cmd.bIsTemp = true;

			return hBuffer;
		}
	}

	RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.allocSafe();

	CmdCreateConstantBuffer& cmd = getQueueState().constantBufferCreates.pushSafe();
	cmd.hBuffer = s_resourceSystem.constantBuffers.getId(pBuffer);
	cmd.pData = pData;
	cmd.cpuAccessFlags = RdrCpuAccessFlags::Write;
	cmd.eUsage = RdrResourceUsage::Dynamic;
	cmd.size = size;
	cmd.bIsTemp = true;

	return cmd.hBuffer;
}

void RdrResourceSystem::UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData)
{
	CmdUpdateConstantBuffer& cmd = getQueueState().constantBufferUpdates.pushSafe();
	cmd.hBuffer = hBuffer;
	cmd.pData = pData;
	cmd.bIsTemp = false;
}

void RdrResourceSystem::ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer)
{
	CmdReleaseConstantBuffer& cmd = getQueueState().constantBufferReleases.pushSafe();
	cmd.hBuffer = hBuffer;
}

void RdrResourceSystem::ReleaseResource(RdrResourceHandle hRes)
{
	CmdReleaseResource& cmd = getQueueState().resourceReleases.pushSafe();
	cmd.hResource = hRes;
}

const RdrResource* RdrResourceSystem::GetDefaultResource(const RdrDefaultResource resType)
{
	return GetResource(s_resourceSystem.defaultResourceHandles[(int)resType]);
}

RdrResourceHandle RdrResourceSystem::GetDefaultResourceHandle(const RdrDefaultResource resType)
{
	return s_resourceSystem.defaultResourceHandles[(int)resType];
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

	CmdCreateRenderTarget& cmd = getQueueState().renderTargetCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.renderTargetViews.getId(pView);
	cmd.arrayStartIndex = -1;

	return cmd.hView;
}

RdrRenderTargetViewHandle RdrResourceSystem::CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize)
{
	assert(hResource);

	RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.allocSafe();

	CmdCreateRenderTarget& cmd = getQueueState().renderTargetCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.renderTargetViews.getId(pView);
	cmd.arrayStartIndex = arrayStartIndex;
	cmd.arraySize = arraySize;

	return cmd.hView;
}

void RdrResourceSystem::ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView)
{
	CmdReleaseRenderTarget& cmd = getQueueState().renderTargetReleases.pushSafe();
	cmd.hView = hView;
}

RdrDepthStencilView RdrResourceSystem::GetDepthStencilView(const RdrDepthStencilViewHandle hView)
{
	return *s_resourceSystem.depthStencilViews.get(hView);
}

void RdrResourceSystem::ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView)
{
	CmdReleaseDepthStencil& cmd = getQueueState().depthStencilReleases.pushSafe();
	cmd.hView = hView;
}

RdrDepthStencilViewHandle RdrResourceSystem::CreateDepthStencilView(RdrResourceHandle hResource)
{
	assert(hResource);

	RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.allocSafe();

	CmdCreateDepthStencil& cmd = getQueueState().depthStencilCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.depthStencilViews.getId(pView);
	cmd.arrayStartIndex = -1;

	return cmd.hView;
}

RdrDepthStencilViewHandle RdrResourceSystem::CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize)
{
	assert(hResource);

	RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.allocSafe();

	CmdCreateDepthStencil& cmd = getQueueState().depthStencilCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.depthStencilViews.getId(pView);
	cmd.arrayStartIndex = arrayStartIndex;
	cmd.arraySize = arraySize;

	return cmd.hView;
}

const RdrGeometry* RdrResourceSystem::GetGeo(const RdrGeoHandle hGeo)
{
	return s_resourceSystem.geos.get(hGeo);
}

void RdrResourceSystem::ReleaseGeo(const RdrGeoHandle hGeo)
{
	CmdReleaseGeo& cmd = getQueueState().geoReleases.pushSafe();
	cmd.hGeo = hGeo;
}

RdrGeoHandle RdrResourceSystem::CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices,
	RdrTopology eTopology, const Vec3& boundsMin, const Vec3& boundsMax)
{
	RdrGeometry* pGeo = s_resourceSystem.geos.allocSafe();

	CmdUpdateGeo& cmd = getQueueState().geoUpdates.pushSafe();
	cmd.hGeo = s_resourceSystem.geos.getId(pGeo);
	cmd.pVertData = pVertData;
	cmd.pIndexData = pIndexData;
	cmd.info.eTopology = eTopology;
	cmd.info.vertStride = vertStride;
	cmd.info.numVerts = numVerts;
	cmd.info.numIndices = numIndices;
	cmd.info.boundsMin = boundsMin;
	cmd.info.boundsMax = boundsMax;

	return cmd.hGeo;
}

void RdrResourceSystem::FlipState(RdrContext* pRdrContext)
{
	// Free temp constant buffers for this frame.
	ResFrameState& state = s_resourceSystem.states[!s_resourceSystem.queueState];
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
	ResFrameState& state = s_resourceSystem.states[!s_resourceSystem.queueState];
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

	// Free shader resource views
	numCmds = (uint)state.shaderResourceViewReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseShaderResourceView& cmd = state.shaderResourceViewReleases[i];
		RdrShaderResourceView* pView = s_resourceSystem.shaderResourceViews.get(cmd.hView);
		pRdrContext->ReleaseShaderResourceView(*pView);
		s_resourceSystem.shaderResourceViews.releaseIdSafe(cmd.hView);
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

	// Free geos
	s_resourceSystem.geos.AcquireLock();
	numCmds = (uint)state.geoReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseGeo& cmd = state.geoReleases[i];
		RdrGeometry* pGeo = s_resourceSystem.geos.get(cmd.hGeo);

		pRdrContext->ReleaseBuffer(pGeo->vertexBuffer);
		pGeo->vertexBuffer.pBuffer = nullptr;

		pRdrContext->ReleaseBuffer(pGeo->indexBuffer);
		pGeo->indexBuffer.pBuffer = nullptr;

		s_resourceSystem.geos.releaseId(cmd.hGeo);
	}
	s_resourceSystem.geos.ReleaseLock();

	// Create textures
	numCmds = (uint)state.textureCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateTexture& cmd = state.textureCreates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		pRdrContext->CreateTexture(cmd.pData, cmd.texInfo, cmd.eUsage, *pResource);
		pResource->texInfo = cmd.texInfo;
		pResource->eUsage = cmd.eUsage;
		pResource->bIsTexture = true;

		SAFE_DELETE(cmd.pFileData);
	}

	// Update buffers
	numCmds = (uint)state.bufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateBuffer& cmd = state.bufferUpdates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		pRdrContext->UpdateBuffer(cmd.pData, *pResource, cmd.numElements);
	}

	// Create buffers
	numCmds = (uint)state.bufferCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateBuffer& cmd = state.bufferCreates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);

		pResource->bufferInfo = cmd.info;
		pResource->eUsage = cmd.eUsage;
		pResource->bIsTexture = false;

		switch (cmd.info.eType)
		{
		case RdrBufferType::Data:
			pRdrContext->CreateDataBuffer(cmd.pData, cmd.info.numElements, cmd.info.eFormat, cmd.eUsage, *pResource);
			break;
		case RdrBufferType::Structured:
			pRdrContext->CreateStructuredBuffer(cmd.pData, cmd.info.numElements, cmd.info.elementSize, cmd.eUsage, *pResource);
			break;
		case RdrBufferType::Vertex:
			pResource->pBuffer = pRdrContext->CreateVertexBuffer(cmd.pData, cmd.info.elementSize * cmd.info.numElements, cmd.eUsage).pBuffer;
			break;
		}
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

	// Create shader resource views
	numCmds = (uint)state.shaderResourceViewCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateShaderResourceView& cmd = state.shaderResourceViewCreates[i];
		RdrShaderResourceView* pView = s_resourceSystem.shaderResourceViews.get(cmd.hView);
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		if (pResource->bIsTexture)
		{
			*pView = pRdrContext->CreateShaderResourceViewTexture(*pResource);
		}
		else
		{
			*pView = pRdrContext->CreateShaderResourceViewBuffer(*pResource, cmd.firstElement);
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
			state.tempConstantBuffers.pushSafe(cmd.hBuffer);
		}
	}

	// Update constant buffers
	numCmds = (uint)state.constantBufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		const CmdUpdateConstantBuffer& cmd = state.constantBufferUpdates[i];
		RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);
		pRdrContext->UpdateConstantBuffer(*pBuffer, cmd.pData);

		if (cmd.bIsTemp)
		{
			state.tempConstantBuffers.pushSafe(cmd.hBuffer);
		}
	}

	// Create/Update geos
	s_resourceSystem.geos.AcquireLock();
	numCmds = (uint)state.geoUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateGeo& cmd = state.geoUpdates[i];
		RdrGeometry* pGeo = s_resourceSystem.geos.get(cmd.hGeo);
		if (!pGeo->vertexBuffer.pBuffer)
		{
			// Creating
			pGeo->geoInfo = cmd.info;
			pGeo->vertexBuffer = pRdrContext->CreateVertexBuffer(cmd.pVertData, cmd.info.vertStride * cmd.info.numVerts, RdrResourceUsage::Default);
			if (cmd.pIndexData)
			{
				pGeo->indexBuffer = pRdrContext->CreateIndexBuffer(cmd.pIndexData, sizeof(uint16) * cmd.info.numIndices, RdrResourceUsage::Default);
			}
		}
		else
		{
			// Updating
			assert(false); //todo
		}
	}
	s_resourceSystem.geos.ReleaseLock();

	state.geoUpdates.clear();
	state.geoReleases.clear();
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
