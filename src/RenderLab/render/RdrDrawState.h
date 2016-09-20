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

	RdrShaderResourceView vsResources[3];
	uint vsResourceCount;

	RdrShaderResourceView psResources[20];
	uint psResourceCount;

	RdrSamplerState psSamplers[16];
	uint psSamplerCount;

	RdrShaderResourceView csResources[20];
	RdrSamplerState csSamplers[16];
	RdrUnorderedAccessView csUavs[8];

	RdrConstantBufferDeviceObj vsConstantBuffers[4];
	RdrConstantBufferDeviceObj dsConstantBuffers[4];
	RdrConstantBufferDeviceObj gsConstantBuffers[4];
	RdrConstantBufferDeviceObj psConstantBuffers[4];
	RdrConstantBufferDeviceObj csConstantBuffers[4];
	uint vsConstantBufferCount;
	uint dsConstantBufferCount;
	uint gsConstantBufferCount;
	uint psConstantBufferCount;
	uint csConstantBufferCount;
};
