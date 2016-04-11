#pragma once
#include "RdrShaders.h"

enum RdrTopology
{
	kRdrTopology_TriangleList,

	kRdrTopology_Count
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
	const RdrInputLayout* pInputLayout;

	RdrVertexBuffer vertexBuffers[3];
	RdrIndexBuffer indexBuffer;
	uint vertexStride;
	uint vertexOffset;
	uint vertexCount;
	uint indexCount;

	RdrShaderResourceView psResources[20];
	RdrSamplerState psSamplers[16];

	RdrShaderResourceView csResources[20];
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
