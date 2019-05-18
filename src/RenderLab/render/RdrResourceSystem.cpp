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
		RdrResource aBuffers[kMaxConstantBuffersPerPool];
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

		RdrDescriptorTableList descriptorTables;
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

	RdrResourceHandle CreateTextureCommon(RdrTextureType texType, uint width, uint height, uint depth,
		uint mipLevels, RdrResourceFormat eFormat, uint sampleCount, RdrResourceAccessFlags accessFlags, char* pTextureData, const RdrDebugBackpointer& debug)
	{
		RdrResource* pResource = s_resourceSystem.resources.allocSafe();

		RdrTextureInfo texInfo;
		texInfo.texType = texType;
		texInfo.format = eFormat;
		texInfo.width = width;
		texInfo.height = height;
		texInfo.depth = depth;
		texInfo.mipLevels = mipLevels;
		texInfo.sampleCount = sampleCount;
		pResource->CreateTexture(*g_pRenderer->GetContext(), texInfo, accessFlags, debug);

		RdrResourceHandle hResource = s_resourceSystem.resources.getId(pResource);
		if (pTextureData)
		{
			g_pRenderer->GetResourceCommandList().UpdateResource(hResource, pTextureData, 0, debug);
		}

		return hResource;
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
		RdrResourceSystem::CreateTexture2D(1, 1, RdrResourceFormat::B8G8R8A8_UNORM,
			RdrResourceAccessFlags::CpuRO_GpuRO, (char*)blackTexData, CREATE_NULL_BACKPOINTER);
	s_resourceSystem.defaultResourceHandles[(int)RdrDefaultResource::kBlackTex3d] =
		RdrResourceSystem::CreateTexture3D(1, 1, 1, RdrResourceFormat::B8G8R8A8_UNORM,
			RdrResourceAccessFlags::CpuRO_GpuRO, (char*)blackTexData, CREATE_NULL_BACKPOINTER);
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

const RdrResource* RdrResourceSystem::GetConstantBuffer(const RdrConstantBufferHandle hBuffer)
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
RdrResourceHandle RdrResourceSystem::CreateTextureFromFile(const CachedString& texName, RdrTextureInfo* pOutInfo, const RdrDebugBackpointer& debug)
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
	pResource->SetName(texName.getString());

	RdrTextureInfo texInfo;
	texInfo.format = getFormatFromDXGI(metadata.format);
	texInfo.width = (uint)metadata.width;
	texInfo.height = (uint)metadata.height;
	texInfo.mipLevels = (uint)metadata.mipLevels;
	texInfo.depth = (uint)metadata.arraySize;
	texInfo.sampleCount = 1;

	if (metadata.IsCubemap())
	{
		texInfo.texType = RdrTextureType::kCube;
		texInfo.depth /= 6;
	}
	else
	{
		texInfo.texType = RdrTextureType::k2D;
	}

	pResource->CreateTexture(*g_pRenderer->GetContext(), texInfo, RdrResourceAccessFlags::CpuRO_GpuRO, debug);

	RdrResourceHandle hResource = s_resourceSystem.resources.getId(pResource);
	s_resourceSystem.textureCache.insert(std::make_pair(texName.getHash(), hResource));

	// Determine offset to skip over header data
	uint nDataStartOffset = sizeof(DWORD) + sizeof(DirectX::DDS_HEADER);
	DirectX::DDS_HEADER* pHeader = (DirectX::DDS_HEADER*)(pTexture->ddsData + sizeof(DWORD));
	if (pHeader->ddspf.dwFourCC == DirectX::DDSPF_DX10.dwFourCC)
	{
		nDataStartOffset += sizeof(DirectX::DDS_HEADER_DXT10);
	}

	// Queue update of texture data.
	g_pRenderer->GetResourceCommandList().UpdateResourceFromFile(hResource, pTexture->ddsData, nDataStartOffset, pTexture->ddsDataSize - nDataStartOffset, debug);

	if (pOutInfo)
	{
		*pOutInfo = texInfo;
	}
	return hResource;
}

void RdrResourceCommandList::UpdateResource(RdrResourceHandle hResource, const void* pData, uint dataSize, const RdrDebugBackpointer& debug)
{
	CmdUpdateResource& cmd = m_resourceUpdates.pushSafe();
	cmd.debug = debug;
	cmd.hResource = hResource;
	cmd.pData = pData;
	cmd.dataSize = dataSize;
	cmd.pFileData = nullptr;
}

void RdrResourceCommandList::UpdateResourceFromFile(RdrResourceHandle hResource, const void* pFileData, uint nDataStartOffset, uint dataSize, const RdrDebugBackpointer& debug)
{
	CmdUpdateResource& cmd = m_resourceUpdates.pushSafe();
	cmd.pFileData = pFileData;
	cmd.dataSize = dataSize;
	cmd.hResource = hResource;
	cmd.pData = ((char*)cmd.pFileData) + nDataStartOffset;
	cmd.debug = debug;
}

RdrResourceHandle RdrResourceSystem::CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, char* pTexData, const RdrDebugBackpointer& debug)
{
	return CreateTextureCommon(RdrTextureType::k2D, width, height, 1, 1, eFormat, 1, accessFlags, pTexData, debug);
}

RdrResourceHandle RdrResourceSystem::CreateTexture2DMS(uint width, uint height, RdrResourceFormat eFormat, uint sampleCount, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	return CreateTextureCommon(RdrTextureType::k2D, width, height, 1, 1, eFormat, sampleCount, accessFlags, nullptr, debug);
}

RdrResourceHandle RdrResourceSystem::CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	return CreateTextureCommon(RdrTextureType::k2D, width, height, arraySize, 1, eFormat, 1, accessFlags, nullptr, debug);
}

RdrResourceHandle RdrResourceSystem::CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	return CreateTextureCommon(RdrTextureType::kCube, width, height, 1, 1, eFormat, 1, accessFlags, nullptr, debug);
}

RdrResourceHandle RdrResourceSystem::CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	return CreateTextureCommon(RdrTextureType::kCube, width, height, arraySize, 1, eFormat, 1, accessFlags, nullptr, debug);
}

RdrResourceHandle RdrResourceSystem::CreateTexture3D(uint width, uint height, uint depth, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, char* pTexData, const RdrDebugBackpointer& debug)
{
	return CreateTextureCommon(RdrTextureType::k3D, width, height, depth, 1, eFormat, 1, accessFlags, pTexData, debug);
}

RdrResourceHandle RdrResourceSystem::CreateVertexBuffer(const void* pSrcData, int stride, int numVerts, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	RdrBufferInfo bufferInfo;
	bufferInfo.eType = RdrBufferType::Vertex;
	bufferInfo.elementSize = stride;
	bufferInfo.numElements = numVerts;
	pResource->CreateBuffer(*g_pRenderer->GetContext(), bufferInfo, accessFlags, debug);

	RdrResourceHandle hResource = s_resourceSystem.resources.getId(pResource);
	if (pSrcData)
	{
		g_pRenderer->GetResourceCommandList().UpdateResource(hResource, pSrcData, 0, debug);
	}

	return hResource;
}

RdrResourceHandle RdrResourceSystem::CreateDataBuffer(const void* pSrcData, int numElements, RdrResourceFormat eFormat, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	RdrBufferInfo bufferInfo;
	bufferInfo.eType = RdrBufferType::Data;
	bufferInfo.eFormat = eFormat;
	bufferInfo.elementSize = 0;
	bufferInfo.numElements = numElements;
	pResource->CreateBuffer(*g_pRenderer->GetContext(), bufferInfo, accessFlags, debug);

	RdrResourceHandle hResource = s_resourceSystem.resources.getId(pResource);
	if (pSrcData)
	{
		g_pRenderer->GetResourceCommandList().UpdateResource(hResource, pSrcData, 0, debug);
	}

	return hResource;
}

RdrResourceHandle RdrResourceSystem::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	RdrResource* pResource = s_resourceSystem.resources.allocSafe();

	RdrBufferInfo bufferInfo;
	bufferInfo.eType = RdrBufferType::Structured;
	bufferInfo.eFormat = RdrResourceFormat::UNKNOWN;
	bufferInfo.elementSize = elementSize;
	bufferInfo.numElements = numElements;
	pResource->CreateBuffer(*g_pRenderer->GetContext(), bufferInfo, accessFlags, debug);

	RdrResourceHandle hResource = s_resourceSystem.resources.getId(pResource);
	if (pSrcData)
	{
		g_pRenderer->GetResourceCommandList().UpdateResource(hResource, pSrcData, 0, debug);
	}

	return hResource;
}

void RdrResourceCommandList::UpdateBuffer(const RdrResourceHandle hResource, const void* pSrcData, int numElements, const RdrDebugBackpointer& debug)
{
	CmdUpdateResource& cmd = m_resourceUpdates.pushSafe();
	cmd.hResource = hResource;
	cmd.pData = pSrcData;
	cmd.debug = debug;

	RdrResource* pResource = s_resourceSystem.resources.get(hResource);
	if (numElements == -1)
	{
		numElements = pResource->GetBufferInfo().numElements;
	}

	cmd.dataSize = pResource->GetBufferInfo().elementSize
		? (pResource->GetBufferInfo().elementSize * numElements)
		: rdrGetTexturePitch(1, pResource->GetBufferInfo().eFormat) * numElements;
}

RdrShaderResourceViewHandle RdrResourceSystem::CreateShaderResourceView(RdrResourceHandle hResource, uint firstElement, const RdrDebugBackpointer& debug)
{
	assert(hResource);
	RdrContext* pRdrContext = g_pRenderer->GetContext();

	RdrShaderResourceView* pView = s_resourceSystem.shaderResourceViews.allocSafe();
	RdrResource* pResource = s_resourceSystem.resources.get(hResource);
	if (pResource->IsTexture())
	{
		*pView = pResource->CreateShaderResourceViewTexture(*pRdrContext);
	}
	else
	{
		*pView = pResource->CreateShaderResourceViewBuffer(*pRdrContext, firstElement);
	}

	return s_resourceSystem.shaderResourceViews.getId(pView);
}

void RdrResourceCommandList::ReleaseShaderResourceView(RdrShaderResourceViewHandle hView, const RdrDebugBackpointer& debug)
{
	CmdReleaseShaderResourceView& cmd = m_shaderResourceViewReleases.pushSafe();
	cmd.hView = hView;
	cmd.debug = debug;
}

RdrConstantBufferHandle RdrResourceSystem::CreateConstantBuffer(const void* pData, uint size, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	assert((size % sizeof(Vec4)) == 0);

	RdrContext* pRdrContext = g_pRenderer->GetContext();
	RdrResource* pBuffer = s_resourceSystem.constantBuffers.allocSafe();

	// Use a pooled constant buffer if there are some available and this buffer needs CPU write access
	// Only CPU writable buffers are pooled because immutable/GPU buffers are rare and can benefit from not using the dynamic flags.
	uint poolIndex = size / sizeof(Vec4);
	if (poolIndex < kNumConstantBufferPools
		&& s_resourceSystem.constantBufferPools[poolIndex].bufferCount > 0
		&& IsFlagSet(accessFlags, RdrResourceAccessFlags::CpuWrite))
	{
		ConstantBufferPool& rPool = s_resourceSystem.constantBufferPools[poolIndex];
		*pBuffer = rPool.aBuffers[--rPool.bufferCount];
	}
	else
	{
		pBuffer->CreateConstantBuffer(*pRdrContext, size, accessFlags, debug);
	}

	RdrConstantBufferHandle hBuffer = s_resourceSystem.constantBuffers.getId(pBuffer);
	if (pData)
	{
		g_pRenderer->GetResourceCommandList().UpdateConstantBuffer(hBuffer, pData, size, debug);
	}

	return hBuffer;
}

void RdrResourceCommandList::UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint dataSize, const RdrDebugBackpointer& debug)
{
	CmdUpdateConstantBuffer& cmd = m_constantBufferUpdates.pushSafe();
	cmd.debug = debug;
	cmd.hBuffer = hBuffer;
	cmd.pData = pData;
	cmd.dataSize = dataSize;
}

RdrConstantBufferHandle RdrResourceSystem::CreateUpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData, uint size, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	if (hBuffer)
	{
		g_pRenderer->GetResourceCommandList().ReleaseConstantBuffer(hBuffer, debug);
		return CreateConstantBuffer(pData, size, accessFlags, debug);
	}
	else
	{
		return CreateConstantBuffer(pData, size, accessFlags, debug);
	}
}

RdrConstantBufferHandle RdrResourceSystem::CreateTempConstantBuffer(const void* pData, uint size, const RdrDebugBackpointer& debug)
{
	RdrConstantBufferHandle hBuffer = CreateConstantBuffer(pData, size, RdrResourceAccessFlags::CpuRW_GpuRO, debug);
	// Queue post-frame destroy of the temporary buffer.
	g_pRenderer->GetResourceCommandList().ReleaseConstantBuffer(hBuffer, debug);

	return hBuffer;
}

void RdrResourceCommandList::ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer, const RdrDebugBackpointer& debug)
{
	CmdReleaseConstantBuffer& cmd = m_constantBufferReleases.pushSafe();
	cmd.hBuffer = hBuffer;
	cmd.debug = debug;
}

void RdrResourceCommandList::ReleaseResource(RdrResourceHandle hRes, const RdrDebugBackpointer& debug)
{
	CmdReleaseResource& cmd = m_resourceReleases.pushSafe();
	cmd.hResource = hRes;
	cmd.debug = debug;
}

RdrRenderTargetViewHandle RdrResourceSystem::CreateRenderTargetView(RdrResourceHandle hResource, const RdrDebugBackpointer& debug)
{
	assert(hResource);
	RdrContext* pRdrContext = g_pRenderer->GetContext();

	RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.allocSafe();
	RdrResource* pResource = s_resourceSystem.resources.get(hResource);
	*pView = pRdrContext->CreateRenderTargetView(*pResource);

	return s_resourceSystem.renderTargetViews.getId(pView);
}

RdrRenderTargetViewHandle RdrResourceSystem::CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize, const RdrDebugBackpointer& debug)
{
	assert(hResource);
	RdrContext* pRdrContext = g_pRenderer->GetContext();

	RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.allocSafe();
	RdrResource* pResource = s_resourceSystem.resources.get(hResource);
	*pView = pRdrContext->CreateRenderTargetView(*pResource, arrayStartIndex, arraySize);

	return s_resourceSystem.renderTargetViews.getId(pView);
}

void RdrResourceCommandList::ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView, const RdrDebugBackpointer& debug)
{
	CmdReleaseRenderTarget& cmd = m_renderTargetReleases.pushSafe();
	cmd.hView = hView;
	cmd.debug = debug;
}

void RdrResourceCommandList::ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView, const RdrDebugBackpointer& debug)
{
	CmdReleaseDepthStencil& cmd = m_depthStencilReleases.pushSafe();
	cmd.hView = hView;
	cmd.debug = debug;
}

RdrDepthStencilViewHandle RdrResourceSystem::CreateDepthStencilView(RdrResourceHandle hResource, const RdrDebugBackpointer& debug)
{
	assert(hResource);
	RdrContext* pRdrContext = g_pRenderer->GetContext();

	RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.allocSafe();
	RdrResource* pResource = s_resourceSystem.resources.get(hResource);
	*pView = pRdrContext->CreateDepthStencilView(*pResource);

	return s_resourceSystem.depthStencilViews.getId(pView);
}

RdrDepthStencilViewHandle RdrResourceSystem::CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize, const RdrDebugBackpointer& debug)
{
	assert(hResource);
	RdrContext* pRdrContext = g_pRenderer->GetContext();

	RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.allocSafe();
	RdrResource* pResource = s_resourceSystem.resources.get(hResource);
	*pView = pRdrContext->CreateDepthStencilView(*pResource, arrayStartIndex, arraySize);

	return s_resourceSystem.depthStencilViews.getId(pView);
}

RdrRenderTarget RdrResourceSystem::InitRenderTarget2d(uint width, uint height, RdrResourceFormat eFormat, int multisampleLevel, const RdrDebugBackpointer& debug)
{
	RdrRenderTarget renderTarget;

	renderTarget.hTexture = CreateTexture2D(width, height, eFormat, RdrResourceAccessFlags::CpuRO_GpuRO_RenderTarget, nullptr, debug);

	if (multisampleLevel > 1)
	{
		renderTarget.hTextureMultisampled = CreateTexture2DMS(width, height, eFormat,
			multisampleLevel, RdrResourceAccessFlags::CpuRO_GpuRO_RenderTarget, debug);
		renderTarget.hRenderTarget = CreateRenderTargetView(renderTarget.hTextureMultisampled, debug);
	}
	else
	{
		renderTarget.hTextureMultisampled = 0;
		renderTarget.hRenderTarget = CreateRenderTargetView(renderTarget.hTexture, debug);
	}

	return renderTarget;
}

void RdrResourceCommandList::ReleaseRenderTarget2d(const RdrRenderTarget& rRenderTarget, const RdrDebugBackpointer& debug)
{
	if (rRenderTarget.hTexture)
		ReleaseResource(rRenderTarget.hTexture, debug);
	if (rRenderTarget.hTextureMultisampled)
		ReleaseResource(rRenderTarget.hTextureMultisampled, debug);
	if (rRenderTarget.hRenderTarget)
		ReleaseRenderTargetView(rRenderTarget.hRenderTarget, debug);
}

void RdrResourceCommandList::ReleaseGeo(const RdrGeoHandle hGeo, const RdrDebugBackpointer& debug)
{
	CmdReleaseGeo& cmd = m_geoReleases.pushSafe();
	cmd.hGeo = hGeo;
	cmd.debug = debug;
}

RdrGeoHandle RdrResourceSystem::CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices,
	RdrTopology eTopology, const Vec3& boundsMin, const Vec3& boundsMax, const RdrDebugBackpointer& debug)
{
	RdrContext* pRdrContext = g_pRenderer->GetContext();
	RdrGeometry* pGeo = s_resourceSystem.geos.allocSafe();
	RdrGeoHandle hGeo = s_resourceSystem.geos.getId(pGeo);

	pGeo->geoInfo.eTopology = eTopology;
	pGeo->geoInfo.vertStride = vertStride;
	pGeo->geoInfo.numVerts = numVerts;
	pGeo->geoInfo.numIndices = numIndices;
	pGeo->geoInfo.boundsMin = boundsMin;
	pGeo->geoInfo.boundsMax = boundsMax;

	if (pVertData)
	{
		RdrBufferInfo vertexBufferInfo;
		vertexBufferInfo.eType = RdrBufferType::Vertex;
		vertexBufferInfo.eFormat = RdrResourceFormat::UNKNOWN;
		vertexBufferInfo.elementSize = vertStride;
		vertexBufferInfo.numElements = numVerts;

		pGeo->pVertexBuffer = s_resourceSystem.resources.allocSafe();
		pGeo->pVertexBuffer->CreateBuffer(*pRdrContext, vertexBufferInfo, RdrResourceAccessFlags::None, debug);

		RdrResourceHandle hResource = s_resourceSystem.resources.getId(pGeo->pVertexBuffer);
		g_pRenderer->GetResourceCommandList().UpdateResource(hResource, pVertData, 0, debug);
	}

	if (pIndexData)
	{
		RdrBufferInfo indexBufferInfo;
		indexBufferInfo.eType = RdrBufferType::Index;
		indexBufferInfo.eFormat = RdrResourceFormat::R16_UINT;
		indexBufferInfo.elementSize = 0;
		indexBufferInfo.numElements = numIndices;

		pGeo->pIndexBuffer = s_resourceSystem.resources.allocSafe();
		pGeo->pIndexBuffer->CreateBuffer(*pRdrContext, indexBufferInfo, RdrResourceAccessFlags::None, debug);

		RdrResourceHandle hResource = s_resourceSystem.resources.getId(pGeo->pIndexBuffer);
		g_pRenderer->GetResourceCommandList().UpdateResource(hResource, pIndexData, 0, debug);
	}

	return hGeo;
}

void RdrResourceCommandList::UpdateGeoVerts(RdrGeoHandle hGeo, const void* pVertData, const RdrDebugBackpointer& debug)
{
	// donotcheckin - all calls to this need to be frame-buffered
	assert(false);

	RdrGeometry* pGeo = s_resourceSystem.geos.get(hGeo);

	RdrResourceHandle hResource = s_resourceSystem.resources.getId(pGeo->pVertexBuffer);
	UpdateResource(hResource, pVertData, 0, debug);
}

const RdrDescriptorTable* RdrResourceSystem::CreateShaderResourceViewTable(const RdrResourceHandle* ahResources, uint count, const RdrDebugBackpointer& debug)
{
	static constexpr uint kMaxDescriptorTableSize = 16;
	assert(count < kMaxDescriptorTableSize);

	RdrContext* pRdrContext = g_pRenderer->GetContext();

	D3D12DescriptorHandles ahDescriptors[kMaxDescriptorTableSize];
	for (uint i = 0; i < count; ++i)
	{
		if (ahResources[i])
		{
			const RdrResource* pResource = s_resourceSystem.resources.get(ahResources[i]);
			ahDescriptors[i] = pResource ? pResource->GetSRV().hCopyableView : pRdrContext->GetNullShaderResourceView().hCopyableView;
		}
		else
		{
			ahDescriptors[i] = pRdrContext->GetNullShaderResourceView().hCopyableView;
		}
	}

	RdrDescriptorTable* pTable = s_resourceSystem.descriptorTables.allocSafe();
	pTable->Create(*pRdrContext, RdrDescriptorType::SRV, ahDescriptors, count, debug);

	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateShaderResourceViewTable(const RdrResource** apResources, uint count, const RdrDebugBackpointer& debug)
{
	static constexpr uint kMaxDescriptorTableSize = 16;
	assert(count < kMaxDescriptorTableSize);

	RdrContext* pRdrContext = g_pRenderer->GetContext();

	D3D12DescriptorHandles ahDescriptors[kMaxDescriptorTableSize];
	for (uint i = 0; i < count; ++i)
	{
		ahDescriptors[i] = apResources[i] ? apResources[i]->GetSRV().hCopyableView : pRdrContext->GetNullShaderResourceView().hCopyableView;
	}

	RdrDescriptorTable* pTable = s_resourceSystem.descriptorTables.allocSafe();
	pTable->Create(*pRdrContext, RdrDescriptorType::SRV, ahDescriptors, count, debug);

	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateTempShaderResourceViewTable(const RdrResource** apResources, uint count, const RdrDebugBackpointer& debug)
{
	const RdrDescriptorTable* pTable = CreateShaderResourceViewTable(apResources, count, debug);
	g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(pTable, debug);
	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateUpdateShaderResourceViewTable(const RdrDescriptorTable* pTable, const RdrResourceHandle* ahResources, uint count, const RdrDebugBackpointer& debug)
{
	if (!pTable)
		return CreateShaderResourceViewTable(ahResources, count, debug);

	g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(pTable, debug);
	return CreateShaderResourceViewTable(ahResources, count, debug);
}

const RdrDescriptorTable* RdrResourceSystem::CreateSamplerTable(const RdrSamplerState* aSamplers, uint count, const RdrDebugBackpointer& debug)
{
	static constexpr uint kMaxDescriptorTableSize = 16;
	assert(count < kMaxDescriptorTableSize);

	RdrContext* pRdrContext = g_pRenderer->GetContext();

	D3D12DescriptorHandles ahDescriptors[kMaxDescriptorTableSize];
	for (uint i = 0; i < count; ++i)
	{
		ahDescriptors[i] = pRdrContext->GetSampler(aSamplers[i]).hCopyable;
	}

	RdrDescriptorTable* pTable = s_resourceSystem.descriptorTables.allocSafe();
	pTable->Create(*pRdrContext, RdrDescriptorType::Sampler, ahDescriptors, count, debug);

	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateTempSamplerTable(const RdrSamplerState* aSamplers, uint count, const RdrDebugBackpointer& debug)
{
	const RdrDescriptorTable* pTable = CreateSamplerTable(aSamplers, count, debug);
	g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(pTable, debug);
	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateUpdateSamplerTable(const RdrDescriptorTable* pTable, const RdrSamplerState* aSamplers, uint count, const RdrDebugBackpointer& debug)
{
	if (!pTable)
		return CreateSamplerTable(aSamplers, count, debug);

	g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(pTable, debug);
	return CreateSamplerTable(aSamplers, count, debug);
}

const RdrDescriptorTable* RdrResourceSystem::CreateConstantBufferTable(const RdrConstantBufferHandle* ahBuffers, uint count, const RdrDebugBackpointer& debug)
{
	static constexpr uint kMaxDescriptorTableSize = 16;
	assert(count < kMaxDescriptorTableSize);

	RdrContext* pRdrContext = g_pRenderer->GetContext();

	D3D12DescriptorHandles ahDescriptors[kMaxDescriptorTableSize];
	for (uint i = 0; i < count; ++i)
	{
		const RdrResource* pResource = s_resourceSystem.constantBuffers.get(ahBuffers[i]);
		ahDescriptors[i] = pResource ? pResource->GetCBV().hCopyableView : pRdrContext->GetNullConstantBufferView().hCopyableView;
	}

	RdrDescriptorTable* pTable = s_resourceSystem.descriptorTables.allocSafe();
	pTable->Create(*pRdrContext, RdrDescriptorType::SRV, ahDescriptors, count, debug);

	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateTempConstantBufferTable(const RdrConstantBufferHandle* ahBuffers, uint count, const RdrDebugBackpointer& debug)
{
	const RdrDescriptorTable* pTable = CreateConstantBufferTable(ahBuffers, count, debug);
	g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(pTable, debug);
	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateConstantBufferTable(const RdrResource** apBuffers, uint count, const RdrDebugBackpointer& debug)
{
	static constexpr uint kMaxDescriptorTableSize = 16;
	assert(count < kMaxDescriptorTableSize);

	RdrContext* pRdrContext = g_pRenderer->GetContext();

	D3D12DescriptorHandles ahDescriptors[kMaxDescriptorTableSize];
	for (uint i = 0; i < count; ++i)
	{
		ahDescriptors[i] = apBuffers[i] ? apBuffers[i]->GetCBV().hCopyableView : pRdrContext->GetNullConstantBufferView().hCopyableView;
	}

	RdrDescriptorTable* pTable = s_resourceSystem.descriptorTables.allocSafe();
	pTable->Create(*pRdrContext, RdrDescriptorType::SRV, ahDescriptors, count, debug);

	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateTempConstantBufferTable(const RdrResource** apBuffers, uint count, const RdrDebugBackpointer& debug)
{
	const RdrDescriptorTable* pTable = CreateConstantBufferTable(apBuffers, count, debug);
	g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(pTable, debug);
	return pTable;
}

const RdrDescriptorTable* RdrResourceSystem::CreateUpdateConstantBufferTable(const RdrDescriptorTable* pTable, const RdrConstantBufferHandle* ahBuffers, uint count, const RdrDebugBackpointer& debug)
{
	if (!pTable)
		return CreateConstantBufferTable(ahBuffers, count, debug);

	g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(pTable, debug);
	return CreateConstantBufferTable(ahBuffers, count, debug);
}

void RdrResourceCommandList::ReleaseDescriptorTable(const RdrDescriptorTable* pTable, const RdrDebugBackpointer& debug)
{
	CmdReleaseDescriptorTable& cmd = m_descTableReleases.pushSafe();
	cmd.pTable = pTable;
	cmd.debug = debug;
}

void RdrResourceCommandList::ProcessCleanupCommands(RdrContext* pRdrContext)
{
	uint numCmds;
	uint64 nLastCompletedFrame = pRdrContext->GetLastCompletedFrame();

	//////////////////////////////////////////////////////////////////////////
	// Free geos
	s_resourceSystem.geos.AcquireLock();
	numCmds = (uint)m_geoReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseGeo& cmd = m_geoReleases[i];
		RdrGeometry* pGeo = s_resourceSystem.geos.get(cmd.hGeo);

		CmdReleaseResource& cmdVertex = m_resourceReleases.pushSafe();
		cmdVertex.hResource = s_resourceSystem.resources.getId(pGeo->pVertexBuffer);
		cmdVertex.debug = cmd.debug;

		if (pGeo->pIndexBuffer)
		{
			CmdReleaseResource& cmdIndex = m_resourceReleases.pushSafe();
			cmdIndex.hResource = s_resourceSystem.resources.getId(pGeo->pIndexBuffer);
			cmdIndex.debug = cmd.debug;
		}
			
		s_resourceSystem.geos.releaseId(cmd.hGeo);
	}
	m_geoReleases.clear();
	s_resourceSystem.geos.ReleaseLock();

	//////////////////////////////////////////////////////////////////////////
	// Free descriptor tables
	s_resourceSystem.descriptorTables.AcquireLock();
	{
		numCmds = (uint)m_descTableReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseDescriptorTable& cmd = m_descTableReleases[i];
			if (cmd.pTable->GetLastUsedFrame() <= nLastCompletedFrame)
			{
				const_cast<RdrDescriptorTable*>(cmd.pTable)->Release(*pRdrContext);
				s_resourceSystem.descriptorTables.release(cmd.pTable);
				m_descTableReleases.eraseFast(i);
				--i;
				--numCmds;
			}
		}
	}
	s_resourceSystem.descriptorTables.ReleaseLock();

	//////////////////////////////////////////////////////////////////////////
	// Free resources
	s_resourceSystem.resources.AcquireLock();
	{
		numCmds = (uint)m_resourceReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseResource& cmd = m_resourceReleases[i];
			RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);
			if (pResource->GetLastUsedFrame() <= nLastCompletedFrame)
			{
				pResource->ReleaseResource(*pRdrContext);
				s_resourceSystem.resources.releaseId(cmd.hResource);
				m_resourceReleases.eraseFast(i);
				--i;
				--numCmds;
			}
		}
	}
	s_resourceSystem.resources.ReleaseLock();

#if 0 //donotcheckin - these should be done before the resource
	// Free render targets
	numCmds = (uint)m_renderTargetReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseRenderTarget& cmd = m_renderTargetReleases[i];
		RdrRenderTargetView* pView = s_resourceSystem.renderTargetViews.get(cmd.hView);
		if (pView->pResource->nLastUsedFrameCode <= nLastCompletedFrame)
		{
			pRdrContext->ReleaseRenderTargetView(*pView);
			s_resourceSystem.renderTargetViews.releaseIdSafe(cmd.hView);
			m_renderTargetReleases.eraseFast(i);
			--i;
			--numCmds;
		}
	}

	// Free depth stencils
	numCmds = (uint)m_depthStencilReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseDepthStencil& cmd = m_depthStencilReleases[i];
		RdrDepthStencilView* pView = s_resourceSystem.depthStencilViews.get(cmd.hView);
		if (pView->pResource->nLastUsedFrameCode <= nLastCompletedFrame)
		{
			pRdrContext->ReleaseDepthStencilView(*pView);
			s_resourceSystem.depthStencilViews.releaseIdSafe(cmd.hView);
			m_depthStencilReleases.eraseFast(i);
			--i;
			--numCmds;
		}
	}

	// Free shader resource views
	numCmds = (uint)m_shaderResourceViewReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseShaderResourceView& cmd = m_shaderResourceViewReleases[i];
		RdrShaderResourceView* pView = s_resourceSystem.shaderResourceViews.get(cmd.hView);
		if (pView->pResource->nLastUsedFrameCode <= nLastCompletedFrame)
		{
			pRdrContext->ReleaseShaderResourceView(*pView);
			s_resourceSystem.shaderResourceViews.releaseIdSafe(cmd.hView);
			m_shaderResourceViewReleases.eraseFast(i);
			--i;
			--numCmds;
		}
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Free constant buffers
	s_resourceSystem.constantBuffers.AcquireLock();
	{
		numCmds = (uint)m_constantBufferReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseConstantBuffer& cmd = m_constantBufferReleases[i];
			RdrResource* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);


			if (pBuffer->GetLastUsedFrame() <= nLastCompletedFrame)
			{
				// Return the buffer to a pool if there is room and the buffer is CPU writable.
				// If it can't be written to from the CPU, then we can't ever update it.
				uint poolIndex = pBuffer->GetSize() / sizeof(Vec4);
				if (poolIndex < kNumConstantBufferPools
					&& s_resourceSystem.constantBufferPools[poolIndex].bufferCount < kMaxConstantBuffersPerPool
					&& IsFlagSet(pBuffer->GetAccessFlags(), RdrResourceAccessFlags::CpuWrite))
				{
					ConstantBufferPool& rPool = s_resourceSystem.constantBufferPools[poolIndex];
					rPool.aBuffers[rPool.bufferCount] = *pBuffer;
					++rPool.bufferCount;
				}
				else
				{
					pBuffer->ReleaseResource(*pRdrContext);
				}

				s_resourceSystem.constantBuffers.releaseId(cmd.hBuffer);
				m_constantBufferReleases.eraseFast(i);
				--i;
				--numCmds;
			}
		}
	}
	s_resourceSystem.constantBuffers.ReleaseLock();
}

void RdrResourceCommandList::ProcessPreFrameCommands(RdrContext* pRdrContext)
{
	// Update resources
	uint numCmds = (uint)m_resourceUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateResource& cmd = m_resourceUpdates[i];
		RdrResource* pResource = s_resourceSystem.resources.get(cmd.hResource);

		pResource->UpdateResource(*pRdrContext, cmd.pData, cmd.dataSize);

		SAFE_DELETE(cmd.pFileData);
	}

	// Update constant buffers
	numCmds = (uint)m_constantBufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		const CmdUpdateConstantBuffer& cmd = m_constantBufferUpdates[i];
		RdrResource* pBuffer = s_resourceSystem.constantBuffers.get(cmd.hBuffer);
		pBuffer->UpdateResource(*pRdrContext, cmd.pData, cmd.dataSize);
	}

	m_resourceUpdates.clear();
	m_constantBufferUpdates.clear();
}
