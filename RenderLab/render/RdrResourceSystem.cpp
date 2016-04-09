#include "Precompiled.h"
#include "RdrResourceSystem.h"
#include "RdrContext.h"
#include "RdrTransientHeap.h"
#include "FileLoader.h"

#include <DirectXTex/include/DirectXTex.h>
#include <DirectXTex/include/Dds.h>

#pragma comment (lib, "DirectXTex.lib")

namespace
{
	RdrResourceFormat getFormatFromDXGI(DXGI_FORMAT format)
	{
		switch (format)
		{
		case DXGI_FORMAT_D16_UNORM:
			return kResourceFormat_D16;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
			return kResourceFormat_D24_UNORM_S8_UINT;
		case DXGI_FORMAT_R16_UNORM:
			return kResourceFormat_R16_UNORM;
		case DXGI_FORMAT_R16G16_FLOAT:
			return kResourceFormat_RG_F16;
		case DXGI_FORMAT_R8_UNORM:
			return kResourceFormat_R8_UNORM;
		case DXGI_FORMAT_BC3_UNORM:
			return kResourceFormat_DXT5;
		default:
			assert(false);
			return kResourceFormat_Count;
		}
	}
}

void RdrResourceSystem::Init(RdrContext* pRdrContext)
{
	m_pRdrContext = pRdrContext;

	// Reserve id 1 for the primary render target.
	m_renderTargetViews.allocSafe();
}

RdrResourceHandle RdrResourceSystem::CreateTextureFromFile(const char* filename, RdrTextureInfo* pOutInfo)
{
	// Find texture in cache
	RdrResourceHandleMap::iterator iter = m_textureCache.find(filename);
	if (iter != m_textureCache.end())
		return iter->second;

	// Create new texture info
	RdrResource* pResource = m_resources.allocSafe();
	pResource->texInfo.filename = _strdup(filename); // todo: string cache

	CmdCreateTexture cmd = { 0 };

	char filepath[MAX_PATH];
	sprintf_s(filepath, "data/textures/%s", filename);
	if (!FileLoader::Load(filepath, &cmd.pHeaderData, &cmd.dataSize))
	{
		assert(false);
		return 0;
	}

	char* pPos = (char*)cmd.pHeaderData;
	
	DirectX::TexMetadata metadata;
	DirectX::GetMetadataFromDDSMemory(cmd.pHeaderData, cmd.dataSize, 0, metadata);

	cmd.hResource = m_resources.getId(pResource);
	cmd.eUsage = kRdrResourceUsage_Immutable;
	cmd.texInfo.format = getFormatFromDXGI(metadata.format);
	cmd.texInfo.width = (uint)metadata.width;
	cmd.texInfo.height = (uint)metadata.height;
	cmd.texInfo.mipLevels = (uint)metadata.mipLevels;
	cmd.texInfo.arraySize = (uint)metadata.arraySize;
	cmd.texInfo.sampleCount = 1;
	cmd.texInfo.bCubemap = false;
	cmd.bFreeData = true;

	pPos += sizeof(DWORD); // magic?
	pPos += sizeof(DirectX::DDS_HEADER);

	cmd.pData = pPos;

	m_states[m_queueState].textureCreates.push_back(cmd);

	m_textureCache.insert(std::make_pair(filename, cmd.hResource));

	if (pOutInfo)
	{
		*pOutInfo = cmd.texInfo;
	}
	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat)
{
	return CreateTextureInternal(width, height, 1, 1, eFormat, 1, false);
}

RdrResourceHandle RdrResourceSystem::CreateTexture2DMS(uint width, uint height, RdrResourceFormat eFormat, uint sampleCount)
{
	return CreateTextureInternal(width, height, 1, 1, eFormat, sampleCount, false);
}

RdrResourceHandle RdrResourceSystem::CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat)
{
	return CreateTextureInternal(width, height, 1, arraySize, eFormat, 1, false);
}

RdrResourceHandle RdrResourceSystem::CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat)
{
	return CreateTextureInternal(width, height, 1, 1, eFormat, 1, true);
}

RdrResourceHandle RdrResourceSystem::CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat)
{
	return CreateTextureInternal(width, height, 1, arraySize, eFormat, 1, true);
}

RdrResourceHandle RdrResourceSystem::CreateTextureInternal(uint width, uint height, uint mipLevels, uint arraySize, RdrResourceFormat eFormat, uint sampleCount, bool bCubemap)
{
	RdrResource* pResource = m_resources.allocSafe();

	CmdCreateTexture cmd = { 0 };
	cmd.hResource = m_resources.getId(pResource);
	cmd.eUsage = kRdrResourceUsage_Default;
	cmd.texInfo.format = eFormat;
	cmd.texInfo.width = width;
	cmd.texInfo.height = height;
	cmd.texInfo.mipLevels = mipLevels;
	cmd.texInfo.arraySize = arraySize;
	cmd.texInfo.sampleCount = sampleCount;
	cmd.texInfo.bCubemap = bCubemap;

	m_states[m_queueState].textureCreates.push_back(cmd);

	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, bool bFreeData, RdrResourceUsage eUsage)
{
	RdrResource* pResource = m_resources.allocSafe();

	CmdCreateBuffer cmd = { 0 };
	cmd.hResource = m_resources.getId(pResource);
	cmd.pData = pSrcData;
	cmd.numElements = numElements;
	cmd.elementSize = elementSize;
	cmd.eUsage = eUsage;
	cmd.bFreeData = bFreeData;

	m_states[m_queueState].bufferCreates.push_back(cmd);

	return cmd.hResource;
}

RdrResourceHandle RdrResourceSystem::UpdateStructuredBuffer(const RdrResourceHandle hResource, const void* pSrcData, bool bFreeData)
{
	CmdUpdateBuffer cmd = { 0 };
	cmd.hResource = hResource;
	cmd.pData = pSrcData;
	cmd.bFreeData = bFreeData;

	m_states[m_queueState].bufferUpdates.push_back(cmd);

	return cmd.hResource;
}

RdrConstantBufferHandle RdrResourceSystem::CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	RdrConstantBuffer* pBuffer = m_constantBuffers.allocSafe();

	CmdCreateConstantBuffer cmd = { 0 };
	cmd.hBuffer = m_constantBuffers.getId(pBuffer);
	cmd.pData = pData;
	cmd.cpuAccessFlags = cpuAccessFlags;
	cmd.eUsage = eUsage;
	cmd.size = size;
	cmd.bIsTemp = false;

	m_states[m_queueState].constantBufferCreates.push_back(cmd);

	return cmd.hBuffer;
}

RdrConstantBufferHandle RdrResourceSystem::CreateTempConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage)
{
	// todo: constant buffer pools to avoid tons of create/releases
	RdrConstantBuffer* pBuffer = m_constantBuffers.allocSafe();

	CmdCreateConstantBuffer cmd = { 0 };
	cmd.hBuffer = m_constantBuffers.getId(pBuffer);
	cmd.pData = pData;
	cmd.cpuAccessFlags = cpuAccessFlags;
	cmd.eUsage = eUsage;
	cmd.size = size;
	cmd.bIsTemp = true;

	m_states[m_queueState].constantBufferCreates.push_back(cmd);

	return cmd.hBuffer;
}

void RdrResourceSystem::UpdateConstantBuffer(RdrConstantBufferHandle hBuffer, const void* pData)
{
	CmdUpdateConstantBuffer cmd = { 0 };
	cmd.hBuffer = hBuffer;
	cmd.pData = pData;

	m_states[m_queueState].constantBufferUpdates.push_back(cmd);
}

void RdrResourceSystem::ReleaseConstantBuffer(RdrConstantBufferHandle hBuffer)
{
	CmdReleaseConstantBuffer cmd = { 0 };
	cmd.hBuffer = hBuffer;

	m_states[m_queueState].constantBufferReleases.push_back(cmd);
}

void RdrResourceSystem::ReleaseResource(RdrResourceHandle hRes)
{
	CmdReleaseResource cmd = { 0 };
	cmd.hResource = hRes;

	m_states[m_queueState].resourceReleases.push_back(cmd);
}

const RdrResource* RdrResourceSystem::GetResource(const RdrResourceHandle hRes)
{
	return m_resources.get(hRes);
}

const RdrConstantBuffer* RdrResourceSystem::GetConstantBuffer(const RdrConstantBufferHandle hBuffer)
{
	return m_constantBuffers.get(hBuffer);
}

RdrRenderTargetView RdrResourceSystem::GetRenderTargetView(const RdrRenderTargetViewHandle hView)
{
	return (hView == kPrimaryRenderTargetHandle) ? m_pRdrContext->GetPrimaryRenderTarget() : *m_renderTargetViews.get(hView);
}

RdrRenderTargetViewHandle RdrResourceSystem::CreateRenderTargetView(RdrResourceHandle hResource)
{
	assert(hResource);

	RdrRenderTargetView* pView = m_renderTargetViews.allocSafe();

	CmdCreateRenderTarget cmd = { 0 };
	cmd.hResource = hResource;
	cmd.hView = m_renderTargetViews.getId(pView);
	cmd.arrayStartIndex = -1;

	m_states[m_queueState].renderTargetCreates.push_back(cmd);

	return cmd.hView;
}

RdrRenderTargetViewHandle RdrResourceSystem::CreateRenderTargetView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize)
{
	assert(hResource);

	RdrRenderTargetView* pView = m_renderTargetViews.allocSafe();

	CmdCreateRenderTarget cmd = { 0 };
	cmd.hResource = hResource;
	cmd.hView = m_renderTargetViews.getId(pView);
	cmd.arrayStartIndex = arrayStartIndex;
	cmd.arraySize = arraySize;

	m_states[m_queueState].renderTargetCreates.push_back(cmd);

	return cmd.hView;
}

void RdrResourceSystem::ReleaseRenderTargetView(const RdrRenderTargetViewHandle hView)
{
	CmdReleaseRenderTarget cmd = { 0 };
	cmd.hView = hView;

	m_states[m_queueState].renderTargetReleases.push_back(cmd);
}

RdrDepthStencilView RdrResourceSystem::GetDepthStencilView(const RdrDepthStencilViewHandle hView)
{
	return *m_depthStencilViews.get(hView);
}

void RdrResourceSystem::ReleaseDepthStencilView(const RdrRenderTargetViewHandle hView)
{
	CmdReleaseDepthStencil cmd = { 0 };
	cmd.hView = hView;

	m_states[m_queueState].depthStencilReleases.push_back(cmd);
}

RdrDepthStencilViewHandle RdrResourceSystem::CreateDepthStencilView(RdrResourceHandle hResource)
{
	assert(hResource);

	RdrDepthStencilView* pView = m_depthStencilViews.allocSafe();

	CmdCreateDepthStencil cmd = { 0 };
	cmd.hResource = hResource;
	cmd.hView = m_depthStencilViews.getId(pView);
	cmd.arrayStartIndex = -1;

	m_states[m_queueState].depthStencilCreates.push_back(cmd);

	return cmd.hView;
}

RdrDepthStencilViewHandle RdrResourceSystem::CreateDepthStencilView(RdrResourceHandle hResource, uint arrayStartIndex, uint arraySize)
{
	assert(hResource);

	RdrDepthStencilView* pView = m_depthStencilViews.allocSafe();

	CmdCreateDepthStencil cmd = { 0 };
	cmd.hResource = hResource;
	cmd.hView = m_depthStencilViews.getId(pView);
	cmd.arrayStartIndex = arrayStartIndex;
	cmd.arraySize = arraySize;

	m_states[m_queueState].depthStencilCreates.push_back(cmd);

	return cmd.hView;
}

void RdrResourceSystem::FlipState()
{
	// Free temp constant buffers for this frame.
	FrameState& state = m_states[!m_queueState];
	uint numTempBuffers = (uint)state.tempConstantBuffers.size();
	for (uint i = 0; i < numTempBuffers; ++i)
	{
		RdrConstantBufferHandle hBuffer = state.tempConstantBuffers[i];
		RdrConstantBuffer* pBuffer = m_constantBuffers.get(hBuffer);
		m_pRdrContext->ReleaseConstantBuffer(pBuffer->bufferObj);
		m_constantBuffers.releaseId(hBuffer);
	}
	state.tempConstantBuffers.clear();

	// Flip state
	m_queueState = !m_queueState;
}

void RdrResourceSystem::ProcessCommands()
{
	FrameState& state = m_states[!m_queueState];
	uint numCmds;

	// Free resources
	m_resources.AcquireLock();
	{
		numCmds = (uint)state.resourceReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseResource& cmd = state.resourceReleases[i];
			RdrResource* pResource = m_resources.get(cmd.hResource);
			m_pRdrContext->ReleaseResource(*pResource);
			m_resources.releaseId(cmd.hResource);
		}
	}
	m_resources.ReleaseLock();

	// Free render targets
	numCmds = (uint)state.renderTargetReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseRenderTarget& cmd = state.renderTargetReleases[i];
		RdrRenderTargetView* pView = m_renderTargetViews.get(cmd.hView);
		m_pRdrContext->ReleaseRenderTargetView(*pView);
		m_renderTargetViews.releaseIdSafe(cmd.hView);
	}

	// Free depth stencils
	numCmds = (uint)state.depthStencilReleases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdReleaseDepthStencil& cmd = state.depthStencilReleases[i];
		RdrDepthStencilView* pView = m_depthStencilViews.get(cmd.hView);
		m_pRdrContext->ReleaseDepthStencilView(*pView);
		m_depthStencilViews.releaseIdSafe(cmd.hView);
	}

	// Free constant buffers
	m_constantBuffers.AcquireLock();
	{
		numCmds = (uint)state.constantBufferReleases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReleaseConstantBuffer& cmd = state.constantBufferReleases[i];
			RdrConstantBuffer* pBuffer = m_constantBuffers.get(cmd.hBuffer);
			m_pRdrContext->ReleaseConstantBuffer(pBuffer->bufferObj);
			m_constantBuffers.releaseId(cmd.hBuffer);
		}
	}
	m_constantBuffers.ReleaseLock();

	// Create textures
	numCmds = (uint)state.textureCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateTexture& cmd = state.textureCreates[i];
		RdrResource* pResource = m_resources.get(cmd.hResource);
		m_pRdrContext->CreateTexture(cmd.pData, cmd.texInfo, cmd.eUsage, *pResource);
		pResource->texInfo = cmd.texInfo;

		if (cmd.bFreeData)
		{
			if (cmd.pHeaderData)
			{
				delete cmd.pHeaderData;
			}
			else if (cmd.pData)
			{
				RdrTransientHeap::Free(cmd.pData);
			}
		}
	}

	// Update buffers
	numCmds = (uint)state.bufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateBuffer& cmd = state.bufferUpdates[i];
		RdrResource* pResource = m_resources.get(cmd.hResource);
		m_pRdrContext->UpdateStructuredBuffer(cmd.pData, *pResource);

		if (cmd.bFreeData)
		{
			RdrTransientHeap::Free(cmd.pData);
		}
	}

	// Create buffers
	numCmds = (uint)state.bufferCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateBuffer& cmd = state.bufferCreates[i];
		RdrResource* pResource = m_resources.get(cmd.hResource);
		pResource->bufferInfo.elementSize = cmd.elementSize;
		pResource->bufferInfo.numElements = cmd.numElements;
		m_pRdrContext->CreateStructuredBuffer(cmd.pData, cmd.numElements, cmd.elementSize, cmd.eUsage, *pResource);
		
		if (cmd.bFreeData)
		{
			RdrTransientHeap::Free(cmd.pData);
		}
	}

	// Create render targets
	numCmds = (uint)state.renderTargetCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateRenderTarget& cmd = state.renderTargetCreates[i];
		RdrRenderTargetView* pView = m_renderTargetViews.get(cmd.hView);
		RdrResource* pResource = m_resources.get(cmd.hResource);
		if (cmd.arrayStartIndex >= 0)
		{
			*pView = m_pRdrContext->CreateRenderTargetView(*pResource, cmd.arrayStartIndex, cmd.arraySize);
		}
		else
		{
			*pView = m_pRdrContext->CreateRenderTargetView(*pResource);
		}
	}

	// Create depth stencils
	numCmds = (uint)state.depthStencilCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateDepthStencil& cmd = state.depthStencilCreates[i];
		RdrDepthStencilView* pView = m_depthStencilViews.get(cmd.hView);
		RdrResource* pResource = m_resources.get(cmd.hResource);
		if (cmd.arrayStartIndex >= 0)
		{
			*pView = m_pRdrContext->CreateDepthStencilView(*pResource, cmd.arrayStartIndex, cmd.arraySize);
		}
		else
		{
			*pView = m_pRdrContext->CreateDepthStencilView(*pResource);
		}
	}

	// Create constant buffers
	numCmds = (uint)state.constantBufferCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateConstantBuffer& cmd = state.constantBufferCreates[i];
		RdrConstantBuffer* pBuffer = m_constantBuffers.get(cmd.hBuffer);
		pBuffer->bufferObj = m_pRdrContext->CreateConstantBuffer(cmd.pData, cmd.size, cmd.cpuAccessFlags, cmd.eUsage);
		pBuffer->size = cmd.size;

		if (cmd.bIsTemp)
		{
			state.tempConstantBuffers.push_back(cmd.hBuffer);
		}
		RdrTransientHeap::Free(cmd.pData);
	}

	// Update constant buffers
	numCmds = (uint)state.constantBufferUpdates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdateConstantBuffer& cmd = state.constantBufferUpdates[i];
		RdrConstantBuffer* pBuffer = m_constantBuffers.get(cmd.hBuffer);
		m_pRdrContext->UpdateConstantBuffer(*pBuffer, cmd.pData);

		RdrTransientHeap::Free(cmd.pData);
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
