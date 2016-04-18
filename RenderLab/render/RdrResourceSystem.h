#pragma once

#include <map>
#include <vector>
#include "RdrResource.h"
#include "FreeList.h"

class RdrContext;

namespace RdrResourceSystem
{
	static const uint kPrimaryRenderTargetHandle = 1;

	void Init();

	RdrResourceHandle CreateTextureFromFile(const char* filename, RdrTextureInfo* pOutInfo);

	RdrResourceHandle CreateTexture2D(uint width, uint height, RdrResourceFormat eFormat, RdrResourceUsage eUsage);
	RdrResourceHandle CreateTexture2DMS(uint width, uint height, RdrResourceFormat format, uint sampleCount);
	RdrResourceHandle CreateTexture2DArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat);

	RdrResourceHandle CreateTextureCube(uint width, uint height, RdrResourceFormat eFormat);
	RdrResourceHandle CreateTextureCubeArray(uint width, uint height, uint arraySize, RdrResourceFormat eFormat);

	RdrResourceHandle CreateStructuredBuffer(const void* pSrcData, int numElements, int elementSize, RdrResourceUsage eUsage);
	RdrResourceHandle UpdateStructuredBuffer(const RdrResourceHandle hResource, const void* pSrcData);

	RdrConstantBufferHandle CreateConstantBuffer(const void* pData, uint size, RdrCpuAccessFlags cpuAccessFlags, RdrResourceUsage eUsage);
	RdrConstantBufferHandle CreateTempConstantBuffer(const void* pData, uint size);
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

	void FlipState(RdrContext* pRdrContext);
	void ProcessCommands(RdrContext* pRdrContext);
}