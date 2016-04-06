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

	const RdrResource* pPsResources[20];
	RdrSamplerState psSamplers[16];

	const RdrResource* pCsResources[20];
	const RdrResource* pCsUavs[8];

	RdrConstantBufferDeviceObj vsConstantBuffers[16];
	RdrConstantBufferDeviceObj gsConstantBuffers[4];
	RdrConstantBufferDeviceObj psConstantBuffers[16];
	RdrConstantBufferDeviceObj csConstantBuffers[16];
	uint vsConstantBufferCount;
	uint gsConstantBufferCount;
	uint psConstantBufferCount;
	uint csConstantBufferCount;
};
