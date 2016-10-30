#pragma once

struct RdrTessellationMaterial
{
	RdrTessellationShader shader;

	RdrConstantBufferHandle hDsConstants;

	Array<RdrResourceHandle, 2> ahResources;
	Array<RdrSamplerState, 2> aSamplers;
};
