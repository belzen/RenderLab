#pragma once
#include "RdrShaders.h"

enum class RdrTopology
{
	TriangleList,

	Count
};

// todo: manage changed states
class RdrDrawState
{
public:
	// todo: remove need for reset()
	void Reset()
	{
		memset(this, 0, sizeof(RdrDrawState));
	}

	const RdrShader* pVertexShader;
	const RdrShader* pGeometryShader;
	const RdrShader* pPixelShader;
	const RdrShader* pComputeShader;

	RdrTopology eTopology;
	RdrInputLayout inputLayout;

	RdrIndexBuffer indexBuffer;
	RdrVertexBuffer vertexBuffers[3];
	uint vertexBufferCount;
	uint vertexStrides[3];
	uint vertexOffsets[3];
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

	RdrConstantBufferDeviceObj vsConstantBuffers[16];
	RdrConstantBufferDeviceObj gsConstantBuffers[4];
	RdrConstantBufferDeviceObj psConstantBuffers[16];
	RdrConstantBufferDeviceObj csConstantBuffers[16];
	uint vsConstantBufferCount;
	uint gsConstantBufferCount;
	uint psConstantBufferCount;
	uint csConstantBufferCount;
};
