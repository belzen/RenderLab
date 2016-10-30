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
		memset(this, 0, sizeof(RdrDrawState));
	}

	const RdrShader* pVertexShader;
	const RdrShader* pGeometryShader;
	const RdrShader* pHullShader;
	const RdrShader* pDomainShader;
	const RdrShader* pPixelShader;
	const RdrShader* pComputeShader;

	RdrTopology eTopology;
	RdrInputLayout inputLayout;

	RdrBuffer indexBuffer;
	RdrBuffer vertexBuffers[kMaxVertexBuffers];
	uint vertexBufferCount;
	uint vertexStrides[kMaxVertexBuffers];
	uint vertexOffsets[kMaxVertexBuffers];
	uint vertexCount;
	uint indexCount;

	// Vertex shader
	RdrConstantBufferDeviceObj vsConstantBuffers[4];
	RdrShaderResourceView vsResources[3];
	uint vsConstantBufferCount;
	uint vsResourceCount;

	// Geometry shader
	RdrConstantBufferDeviceObj gsConstantBuffers[4];
	uint gsConstantBufferCount;

	// Domain shader
	RdrConstantBufferDeviceObj dsConstantBuffers[4];
	RdrShaderResourceView dsResources[4];
	RdrSamplerState dsSamplers[4];
	uint dsConstantBufferCount;
	uint dsResourceCount;
	uint dsSamplerCount;

	// Pixel shader
	RdrConstantBufferDeviceObj psConstantBuffers[4];
	RdrShaderResourceView psResources[20];
	RdrSamplerState psSamplers[16];
	uint psConstantBufferCount;
	uint psResourceCount;
	uint psSamplerCount;

	// Compute shader
	RdrConstantBufferDeviceObj csConstantBuffers[4];
	RdrShaderResourceView csResources[8];
	RdrSamplerState csSamplers[4];
	RdrUnorderedAccessView csUavs[4];
	uint csConstantBufferCount;
};
