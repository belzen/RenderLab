#pragma once

struct RdrTessellationMaterial
{
	RdrTessellationShader shader;

	RdrConstantBufferHandle hDsConstants;

	RdrResourceHandle hResources[2];
	RdrSamplerState samplers[2];
	uint resourceCount;
};
