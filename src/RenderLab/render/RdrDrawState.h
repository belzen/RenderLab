#pragma once
#include "RdrShaders.h"
#include "RdrGeometry.h"

// todo: manage changed states
class RdrDrawState
{
public:
	static const uint kMaxVertexBuffers = 3;

	// todo: remove need for reset()
	void Reset()
	{
		memset(this, 0, sizeof(RdrDrawState));
	}

	bool IsShaderStageActive(RdrShaderStageFlags eStageFlag) const
	{
		return (shaderStages & eStageFlag) != RdrShaderStageFlags::None;
	}

	const RdrPipelineState* pPipelineState;
	RdrShaderStageFlags shaderStages;
	RdrTopology eTopology;

	const RdrResource* pIndexBuffer;
	const RdrResource* pVertexBuffers[kMaxVertexBuffers];
	uint vertexBufferCount;
	uint vertexStrides[kMaxVertexBuffers];
	uint vertexOffsets[kMaxVertexBuffers];
	uint vertexCount;
	uint indexCount;

	// Vertex shader
	const RdrDescriptors* pVsGlobalConstantBufferTable;
	const RdrDescriptors* pVsPerObjectConstantBuffer;
	const RdrDescriptors* pVsShaderResourceViewTable;

	// Domain shader
	const RdrDescriptors* pDsGlobalConstantBufferTable;
	const RdrDescriptors* pDsPerObjectConstantBuffer;
	const RdrDescriptors* pDsShaderResourceViewTable;
	const RdrDescriptors* pDsSamplerTable;

	// Pixel shader
	const RdrDescriptors* pPsMaterialConstantBufferTable;
	const RdrDescriptors* pPsMaterialShaderResourceViewTable;
	const RdrDescriptors* pPsMaterialSamplerTable;

	const RdrDescriptors* pPsGlobalConstantBufferTable;
	const RdrDescriptors* pPsGlobalShaderResourceViewTable;
	const RdrDescriptors* pPsScreenShaderResourceViewTable;

	// Compute shader
	const RdrDescriptors* pCsConstantBufferTable;
	const RdrDescriptors* pCsShaderResourceViewTable;
	const RdrDescriptors* pCsSamplerTable;
	const RdrDescriptors* pCsUnorderedAccessViewTable;
};
