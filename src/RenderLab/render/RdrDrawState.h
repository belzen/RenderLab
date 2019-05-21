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

	RdrPipelineState pipelineState;
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
	D3D12DescriptorHandles hVsGlobalConstantBufferTable;
	D3D12DescriptorHandles hVsPerObjectConstantBuffer;
	D3D12DescriptorHandles hVsShaderResourceViewTable;

	// Domain shader
	D3D12DescriptorHandles hDsGlobalConstantBufferTable;
	D3D12DescriptorHandles hDsPerObjectConstantBuffer;
	D3D12DescriptorHandles hDsShaderResourceViewTable;
	D3D12DescriptorHandles hDsSamplerTable;

	// Pixel shader
	D3D12DescriptorHandles hPsMaterialConstantBufferTable;
	D3D12DescriptorHandles hPsMaterialShaderResourceViewTable;
	D3D12DescriptorHandles hPsMaterialSamplerTable;

	D3D12DescriptorHandles hPsGlobalConstantBufferTable;
	D3D12DescriptorHandles hPsGlobalShaderResourceViewTable;
	D3D12DescriptorHandles hPsGlobalSamplerTable;

	// Compute shader
	D3D12DescriptorHandles hCsConstantBufferTable;
	D3D12DescriptorHandles hCsShaderResourceViewTable;
	D3D12DescriptorHandles hCsSamplerTable;
	D3D12DescriptorHandles hCsUnorderedAccessViewTable;
};
