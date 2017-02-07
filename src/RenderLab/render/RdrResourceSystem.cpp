#include "Precompiled.h"
#include "RdrResourceSystem.h"
#include "RdrContext.h"
#include "RdrFrameMem.h"
#include "RdrAction.h"
#include "Renderer.h"
#include "AssetLib/TextureAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "UtilsLib/Hash.h"

#include <DirectXTex/include/DirectXTex.h>
#include <DirectXTex/include/Dds.h>

#pragma comment (lib, "DirectXTex.lib")

namespace
{
	typedef std::map<Hashing::StringHash, RdrResourceHandle> RdrResourceHandleMap;

	static const uint kMaxConstantBuffersPerPool = 128;
	static const uint kNumConstantBufferPools = 10;

	struct ConstantBufferPool
	{
		RdrConstantBuffer aBuffers[kMaxConstantBuffersPerPool];
		uint bufferCount;
	};

	struct 
	{
		RdrResourceHandleMap	textureCache;
		RdrResourceHandle		defaultResourceHandles[(int)RdrDefaultResource::kNumResources];
		RdrResourceList			resources;
		RdrGeoList				geos;

		RdrConstantBufferList	constantBuffers;
		ConstantBufferPool		constantBufferPools[kNumConstantBufferPools];

		RdrRenderTargetViewList		renderTargetViews;
		RdrDepthStencilViewList		depthStencilViews;
		RdrShaderResourceViewList	shaderResourceViews;
	} s_resourceSystem;

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
}

//////////////////////////////////////////////////////////////////////////
/// RdrResourceSystem
//////////////////////////////////////////////////////////////////////////

void RdrResourceSystem::Init(Renderer& rRenderer)
{
	// Reserve ids for global resources
	for (int i = 0; i < (int)RdrGlobalRenderTargetHandles::kCount; ++i)
	{
		s_resourceSystem.renderTargetViews.allocSafe();
	}

	for (int i = 0; i < (int)RdrGlobalResourceHandles::kCount; ++i)
	{
		s_resourceSystem.resources.allocSafe();
	}

	// Create default resources.
	static uchar blackTexData[4] = { 0, 0, 0, 255 };
	s_resourceSystem.defaultResourceHandles[(int)RdrDefaultResource::kBlackTex2d] =
		rRenderer.GetPreFrameCommandList().CreateTexture2D(1, 1, RdrResourceFormat::B8G8R8A8_UNORM,
			RdrResourceUsage::Immutable, RdrResourceBindings::kNone, (char*)blackTexData);
	s_resourceSystem.defaultResourceHandles[(int)RdrDefaultResource::kBlackTex3d] =
		rRenderer.GetPreFrameCommandList().CreateTexture3D(1, 1, 1, RdrResourceFormat::B8G8R8A8_UNORM, 
			RdrResourceUsage::Immutable, RdrResourceBindings::kNone, (char*)blackTexData);
}

void RdrResourceSystem::SetActiveGlobalResources(const RdrGlobalResources& rResources)
{
	for (int i = 1; i <= (int)RdrGlobalRenderTargetHandles::kCount; ++i)
	{
		RdrRenderTargetView* pTarget = s_resourceSystem.renderTargetViews.get(i);
		*pTarget = rResources.renderTargets[i];
	}

	for (int i = 1; i <= (int)RdrGlobalResourceHandles::kCount; ++i)
	{
		RdrResource* pSrc = s_resourceSystem.resources.get(rResources.hResources[i]);
		RdrResource* pTarget = s_resourceSystem.resources.get(i);
		*pTarget = *pSrc;
	}
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
	return *s_resourceSystem.renderTargetViews.get(hView);
}

RdrDepthStencilView RdrResourceSystem::GetDepthStencilView(const RdrDepthStencilViewHandle hView)
{
	return *s_resourceSystem.depthStencilViews.get(hView);
}

const RdrGeometry* RdrResourceSystem::GetGeo(const RdrGeoHandle hGeo)
{
	return s_resourceSystem.geos.get(hGeo);
}

//////////////////////////////////////////////////////////////////////////
/// RdrResourceCommandList
//////////////////////////////////////////////////////////////////////////
RdrResourceHandle RdrResourceCommandList::CreateTextureFromFile(const CachedString& texName, RdrTextureInfo* pOutInfo)
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

	CmdCreateTexture& cmd = m_textureCreates.pushSafe();
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

RdrResourceHandle RdrResourceCommandList::CreateTextureCommon(RdrTextureType texType, uint width, uint height, uint depth,
	uint mipLevels, RdrResourceFormat eFormat, uint sampleCount, RdrResourceUsage eUsage, RdrResourceBindings eBindings, char* pTextureData)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	CmdCreateTexture& cmd = m_textureCreates.pushSafe();
	memset(&cmd, 0, sizeof(cmd));

	cmd.hResource = s_resourceSystem.resources.getId(pResource);
	cmd.eUsage = eUsage;
	cmd.eBindings = eBindings;
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

RdrResourceHandle RdrResourceCommandList::CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings, char* pTexData)
{
	return CreateTextureCommon(RdrTextureType::k2D, width, height, 1, 1, eFormat, 1, eUsage, eBindings, pTexData);
}

RdrResourceHandle RdrResourceCommandList::CreateTexture2DMS(uint width, uint height, RdrResourceFormat eFormat, uint sampleCount, RdrResourceUsage eUsage, RdrResourceBindings eBindings)
{
	return CreateTextureCommon(RdrTextureType::k2D, width, height, 1, 1, eFormat, sampleCount, eUsage, eBindings, nullptr);
}

RdrResourceHandle RdrResourceCommandList::CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings)
{
	return CreateTextureCommon(RdrTextureType::k2D, width, height, arraySize, 1, eFormat, 1, eUsage, eBindings, nullptr);
}

RdrResourceHandle RdrResourceCommandList::CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings)
{
	return CreateTextureCommon(RdrTextureType::kCube, width, height, 1, 1, eFormat, 1, eUsage, eBindings, nullptr);
}

RdrResourceHandle RdrResourceCommandList::CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings)
{
	return CreateTextureCommon(RdrTextureType::kCube, width, height, arraySize, 1, eFormat, 1, eUsage, eBindings, nullptr);
}

RdrResourceHandle RdrResourceCommandList::CreateTexture3D(uint width, uint height, uint depth, RdrResourceFormat eFormat, RdrResourceUsage eUsage, RdrResourceBindings eBindings, char* pTexData)
{
	return CreateTextureCommon(RdrTextureType::k3D, width, height, depth, 1, eFormat, 1, eUsage, eBindings, pTexData);
}

RdrResourceHandle RdrResourceCommandList::CreateVertexBuffer(const void* pSrcData, int stride, int numVerts, RdrResourceUsage eUsage)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	CmdCreateBuffer& cmd = m_bufferCreates.pushSafe();
	cmd.hResource = s_resourceSystem.resources.getId(pResource);
	cmd.pData = pSrcData;
	cmd.eUsage = eUsage;
	cmd.info.eType = RdrBufferType::Vertex;
	cmd.info.elementSize = stride;
	cmd.info.numElements = numVerts;

	return cmd.hResource;
}

RdrResourceHandle RdrResourceCommandList::CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceUsage eUsage)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	CmdCreateBuffer& cmd = m_bufferCreates.pushSafe();
	cmd.hResource = s_resourceSystem.resources.getId(pResource);
	cmd.pData = pSrcData;
	cmd.eUsage = eUsage;
	cmd.info.eType = RdrBufferType::Data;
	cmd.info.eFormat = eFormat;
	cmd.info.numElements = numElements;

	return cmd.hResource;
}

RdrResourceHandle RdrResourceCommandList::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	CmdCreateBuffer& cmd = m_bufferCreates.pushSafe();
	cmd.hResource = s_resourceSystem.resources.getId(pResource);
	cmd.pData = pSrcData;
	cmd.eUsage = eUsage;
	cmd.info.eType = RdrBufferType::Structured;
	cmd.info.elementSize = elementSize;
	cmd.info.numElements = numElements;

	return cmd.hResource;
}

RdrResourceHandle RdrResourceCommandList::UpdateBuffer(const RdrResourceHandle hResource, const void* pSrcData, int numElements)
{
	CmdUpdateBuffer& cmd = m_bufferUpdates.pushSafe();
	cmd.hResource = hResource;
	cmd.pData = pSrcData;
	cmd.numElements = numElements;

	return cmd.hResource;
}

RdrShaderResourceViewHandle RdrResourceCommandList::CreateShaderResourceView(RdrResourceHandle hResource, uint firstElement)
{
	assert(hResource);

	RdrShaderResourceView* pView = s_resourceSystem.shaderResourceViews.allocSafe();

	CmdCreateShaderResourceView& cmd = m_shaderResourceViewCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.shaderResourceViews.getId(pView);
	cmd.firstElement = firstElement;

	return cmd.hView;
}

void RdrResourceCommandList::ReleaseShaderResourceView(RdrShaderResourceViewHandle hView)
{
	CmdReleaseShaderResourceView& cmd = m_shaderResourceViewReleases.pushSafe();
	cmd.hView = hView;
}

RdrConstantBufferHandle RdrResourceCommandList::CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.allocSafe();

	CmdCreateConstantBuffer& cmd = m_constantBufferCreates.pushSafe();
	cmd.hBuffer = s_resourceSystem.constantBuffers.getId(pBuffer);
	cmd.pData = pData;
	cmd.cpuAccessFlags = cpuAccessFlags;
	cmd.eUsage = eUsage;
	cmd.size = size;

	assert((size % sizeof(Vec4)) == 0);
	return cmd.hBuffer;
}

RdrConstantBufferHandle RdrResourceCommandList::CreateUpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
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

RdrConstantBufferHandle RdrResourceCommandList::CreateTempConstantBuffer(const void* pData, uint size)
{
	assert(size % sizeof(Vec4) == 0);

	RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.allocSafe();

	CmdCreateConstantBuffer& cmd = m_constantBufferCreates.pushSafe();
	cmd.hBuffer = s_resourceSystem.constantBuffers.getId(pBuffer);
	cmd.pData = pData;
	cmd.cpuAccessFlags = RdrCpuAccessFlags::Write;
	cmd.eUsage = RdrResourceUsage::Dynamic;
	cmd.size = size;

	// Queue post-frame destroy of the temporary buffer.
	g_pRenderer->GetPostFrameCommandList().ReleaseConstantBuffer(cmd.hBuffer);

	return cmd.hBuffer;
}

void RdrResourceCommandList::UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData)
{
	CmdUpdateConstantBuffer& cmd = m_constantBufferUpdates.pushSafe();
	cmd.hBuffer = hBuffer;
	cmd.pData = pData;
	cmd.bIsTemp = false;
}

void RdrResourceCommandList::ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer)
{
	CmdReleaseConstantBuffer& cmd = m_constantBufferReleases.pushSafe();
	cmd.hBuffer = hBuffer;
}

void RdrResourceCommandList::ReleaseResource(RdrResourceHandle hRes)
{
	CmdReleaseResource& cmd = m_resourceReleases.pushSafe();
	cmd.hResource = hRes;
}

RdrRenderTargetViewHandle RdrResourceCommandList::CreateRenderTargetView(RdrResourceHandle hResource)
{
	assert(hResource);

	RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.allocSafe();

	CmdCreateRenderTarget& cmd = m_renderTargetCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.renderTargetViews.getId(pView);
	cmd.arrayStartIndex = -1;

	return cmd.hView;
}

RdrRenderTargetViewHandle RdrResourceCommandList::CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize)
{
	assert(hResource);

	RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.allocSafe();

	CmdCreateRenderTarget& cmd = m_renderTargetCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.renderTargetViews.getId(pView);
	cmd.arrayStartIndex = arrayStartIndex;
	cmd.arraySize = arraySize;

	return cmd.hView;
}

void RdrResourceCommandList::ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView)
{
	CmdReleaseRenderTarget& cmd = m_renderTargetReleases.pushSafe();
	cmd.hView = hView;
}

void RdrResourceCommandList::ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView)
{
	CmdReleaseDepthStencil& cmd = m_depthStencilReleases.pushSafe();
	cmd.hView = hView;
}

RdrDepthStencilViewHandle RdrResourceCommandList::CreateDepthStencilView(RdrResourceHandle hResource)
{
	assert(hResource);

	RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.allocSafe();

	CmdCreateDepthStencil& cmd = m_depthStencilCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.depthStencilViews.getId(pView);
	cmd.arrayStartIndex = -1;

	return cmd.hView;
}

RdrDepthStencilViewHandle RdrResourceCommandList::CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize)
{
	assert(hResource);

	RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.allocSafe();

	CmdCreateDepthStencil& cmd = m_depthStencilCreates.pushSafe();
	cmd.hResource = hResource;
	cmd.hView = s_resourceSystem.depthStencilViews.getId(pView);
	cmd.arrayStartIndex = arrayStartIndex;
	cmd.arraySize = arraySize;

	return cmd.hView;
}

RdrRenderTarget2d RdrResourceCommandList::InitRenderTarget2d(uint width, uint height, RdrResourceFormat eFormat, int multisampleLevel)
{
	RdrRenderTarget2d renderTarget;

	renderTarget.hTexture = CreateTexture2D(width, height, eFormat, RdrResourceUsage::Default, RdrResourceBindings::kRenderTarget, nullptr);

	if (multisampleLevel > 1)
	{
		renderTarget.hTextureMultisampled = CreateTexture2DMS(width, height, eFormat,
			multisampleLevel, RdrResourceUsage::Default, RdrResourceBindings::kRenderTarget);
		renderTarget.hRenderTarget = CreateRenderTargetView(renderTarget.hTextureMultisampled);
	}
	else
	{
		renderTarget.hTextureMultisampled = 0;
		renderTarget.hRenderTarget = CreateRenderTargetView(renderTarget.hTexture);
	}

	return renderTarget;
}

void RdrResourceCommandList::ReleaseRenderTarget2d(const RdrRenderTarget2d& rRenderTarget)
{
	if (rRenderTarget.hTexture)
		ReleaseResource(rRenderTarget.hTexture);
	if (rRenderTarget.hTextureMultisampled)
		ReleaseResource(rRenderTarget.hTextureMultisampled);
	if (rRenderTarget.hRenderTarget)
		ReleaseRenderTargetView(rRenderTarget.hRenderTarget);
}

void RdrResourceCommandList::ReleaseGeo(const RdrGeoHandle hGeo)
{
	CmdReleaseGeo& cmd = m_geoReleases.pushSafe();
	cmd.hGeo = hGeo;
}

RdrGeoHandle RdrResourceCommandList::CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices,
	RdrTopology eTopology, const Vec3& boundsMin, const Vec3& boundsMax)
{
	RdrGeometry* pGeo = s_resourceSystem.geos.allocSafe();

	CmdUpdateGeo& cmd = m_geoUpdates.pushSafe();
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

void RdrResourceCommandList::ProcessCommands(RdrContext* pRdrContext)
{
	uint numCmds;

	// Free resources
	s_resourceSystem.resources.AcquireLock();
	{
		numCmds = (uint)m_resourceReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseResource& cmd = m_resourceReleases[i];
			RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
			pRdrContext->ReleaseResource(*pResource);
			s_resourceSystem.resources.releaseId(cmd.hResource);
		}
	}
	s_resourceSystem.resources.ReleaseLock();

	// Free render targets
	numCmds = (uint)m_renderTargetReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseRenderTarget& cmd = m_renderTargetReleases[i];
		RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.get(cmd.hView);
		pRdrContext->ReleaseRenderTargetView(*pView);
		s_resourceSystem.renderTargetViews.releaseIdSafe(cmd.hView);
	}

	// Free depth stencils
	numCmds = (uint)m_depthStencilReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseDepthStencil& cmd = m_depthStencilReleases[i];
		RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.get(cmd.hView);
		pRdrContext->ReleaseDepthStencilView(*pView);
		s_resourceSystem.depthStencilViews.releaseIdSafe(cmd.hView);
	}

	// Free shader resource views
	numCmds = (uint)m_shaderResourceViewReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseShaderResourceView& cmd = m_shaderResourceViewReleases[i];
		RdrShaderResourceView* pView = s_resourceSystem.shaderResourceViews.get(cmd.hView);
		pRdrContext->ReleaseShaderResourceView(*pView);
		s_resourceSystem.shaderResourceViews.releaseIdSafe(cmd.hView);
	}

	// Free constant buffers
	s_resourceSystem.constantBuffers.AcquireLock();
	{
		numCmds = (uint)m_constantBufferReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseConstantBuffer& cmd = m_constantBufferReleases[i];
			RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);

			// Return the buffer to a pool if there is room and the buffer is CPU writable.
			// If it can't be written to from the CPU, then we can't ever update it.
			uint poolIndex = pBuffer->size / sizeof(Vec4);
			if (poolIndex < kNumConstantBufferPools 
				&& s_resourceSystem.constantBufferPools[poolIndex].bufferCount < kMaxConstantBuffersPerPool
				&& (int)(pBuffer->cpuAccessFlags & RdrCpuAccessFlags::Write) != 0)
			{
				ConstantBufferPool& rPool = s_resourceSystem.constantBufferPools[poolIndex];
				rPool.aBuffers[rPool.bufferCount] = *pBuffer;
				++rPool.bufferCount;
			}
			else
			{
				pRdrContext->ReleaseConstantBuffer(pBuffer->bufferObj);
			}

			s_resourceSystem.constantBuffers.releaseId(cmd.hBuffer);
		}
	}
	s_resourceSystem.constantBuffers.ReleaseLock();

	// Free geos
	s_resourceSystem.geos.AcquireLock();
	numCmds = (uint)m_geoReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseGeo& cmd = m_geoReleases[i];
		RdrGeometry* pGeo = s_resourceSystem.geos.get(cmd.hGeo);

		pRdrContext->ReleaseBuffer(pGeo->vertexBuffer);
		pGeo->vertexBuffer.pBuffer = nullptr;

		pRdrContext->ReleaseBuffer(pGeo->indexBuffer);
		pGeo->indexBuffer.pBuffer = nullptr;

		s_resourceSystem.geos.releaseId(cmd.hGeo);
	}
	s_resourceSystem.geos.ReleaseLock();

	// Create textures
	numCmds = (uint)m_textureCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateTexture& cmd = m_textureCreates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		pRdrContext->CreateTexture(cmd.pData, cmd.texInfo, cmd.eUsage, cmd.eBindings, *pResource);
		pResource->texInfo = cmd.texInfo;
		pResource->eUsage = cmd.eUsage;
		pResource->bIsTexture = true;

		SAFE_DELETE(cmd.pFileData);
	}

	// Update buffers
	numCmds = (uint)m_bufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateBuffer& cmd = m_bufferUpdates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
		pRdrContext->UpdateBuffer(cmd.pData, *pResource, cmd.numElements);
	}

	// Create buffers
	numCmds = (uint)m_bufferCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateBuffer& cmd = m_bufferCreates[i];
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
	numCmds = (uint)m_renderTargetCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateRenderTarget& cmd = m_renderTargetCreates[i];
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
	numCmds = (uint)m_depthStencilCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateDepthStencil& cmd = m_depthStencilCreates[i];
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
	numCmds = (uint)m_shaderResourceViewCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateShaderResourceView& cmd = m_shaderResourceViewCreates[i];
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
	numCmds = (uint)m_constantBufferCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateConstantBuffer& cmd = m_constantBufferCreates[i];
		RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);

		// Use a pooled constant buffer if there are some available and this buffer needs CPU write access
		// Only CPU writable buffers are pooled because immutable/GPU buffers are rare and can benefit from not using the dynamic flags.
		uint poolIndex = cmd.size / sizeof(Vec4);
		if (poolIndex < kNumConstantBufferPools 
			&& s_resourceSystem.constantBufferPools[poolIndex].bufferCount > 0
			&& ((int)(cmd.cpuAccessFlags & RdrCpuAccessFlags::Write) != 0))
		{
			ConstantBufferPool& rPool = s_resourceSystem.constantBufferPools[poolIndex];
			*pBuffer = rPool.aBuffers[--rPool.bufferCount];
			pRdrContext->UpdateConstantBuffer(pBuffer->bufferObj, cmd.pData, cmd.size);
		}
		else
		{
			pBuffer->bufferObj = pRdrContext->CreateConstantBuffer(cmd.pData, cmd.size, cmd.cpuAccessFlags, cmd.eUsage);
			pBuffer->size = cmd.size;
			pBuffer->cpuAccessFlags = cmd.cpuAccessFlags;
		}
	}

	// Update constant buffers
	numCmds = (uint)m_constantBufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		const CmdUpdateConstantBuffer& cmd = m_constantBufferUpdates[i];
		RdrConstantBuffer* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);
		pRdrContext->UpdateConstantBuffer(pBuffer->bufferObj, cmd.pData, pBuffer->size);
	}

	// Create/Update geos
	s_resourceSystem.geos.AcquireLock();
	numCmds = (uint)m_geoUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateGeo& cmd = m_geoUpdates[i];
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

	m_geoUpdates.clear();
	m_geoReleases.clear();
	m_textureCreates.clear();
	m_bufferCreates.clear();
	m_bufferUpdates.clear();
	m_resourceReleases.clear();
	m_constantBufferCreates.clear();
	m_constantBufferUpdates.clear();
	m_constantBufferReleases.clear();
	m_renderTargetCreates.clear();
	m_renderTargetReleases.clear();
	m_depthStencilCreates.clear();
	m_depthStencilReleases.clear();
}
