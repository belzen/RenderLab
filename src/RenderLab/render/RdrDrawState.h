#pragma once
#include "RdrShaders.h"


// todo: manage changed states
class RdrDrawState
{
public:
	static const uint kMaxVertexBuffers = 3;

	// todo: remove need for reset()
	void Reset()
	{
		pipelineState.pPipelineState = nullptr; //donotcheckin
		memset(this, 0, sizeof(RdrDrawState));
	}

	bool IsShaderStageActive(RdrShaderStage eStage)
	{
		return (shaderStages & (1 << (uint)eStage)) != 0;
	}

	RdrPipelineState pipelineState;
	uint shaderStages; // Bit-field of RdrShaderStage
	RdrTopology eTopology;

	const RdrResource* pIndexBuffer;
	const RdrResource* pVertexBuffers[kMaxVertexBuffers];
	uint vertexBufferCount;
	uint vertexStrides[kMaxVertexBuffers];
	uint vertexOffsets[kMaxVertexBuffers];
	uint vertexCount;
	uint indexCount;

	// Vertex shader
	RdrShaderResourceView vsConstantBuffers[4];
	RdrShaderResourceView vsResources[3];
	uint vsConstantBufferCount;
	uint vsResourceCount;

	// Geometry shader
	RdrShaderResourceView gsConstantBuffers[4];
	uint gsConstantBufferCount;

	// Domain shader
	RdrShaderResourceView dsConstantBuffers[4];
	RdrShaderResourceView dsResources[4];
	RdrSamplerState dsSamplers[4];
	uint dsConstantBufferCount;
	uint dsResourceCount;
	uint dsSamplerCount;

	// Pixel shader
	RdrShaderResourceView psConstantBuffers[4];
	RdrShaderResourceView psResources[20];
	RdrSamplerState psSamplers[16];
	uint psConstantBufferCount;
	uint psResourceCount;
	uint psSamplerCount;

	// Compute shader
	RdrShaderResourceView csConstantBuffers[4];
	RdrShaderResourceView csResources[8];
	RdrSamplerState csSamplers[4];
	RdrUnorderedAccessView csUavs[4];
	uint csConstantBufferCount;
};
