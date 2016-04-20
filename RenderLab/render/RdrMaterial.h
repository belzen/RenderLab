#pragma once

#include "RdrShaders.h"
#include "RdrResource.h"

struct RdrMaterial
{
	static const uint kMaxTextures = 12;

	static const RdrMaterial* LoadFromFile(const char* materialName);
	static void ReloadMaterial(const char* materialName);

	RdrShaderHandle hPixelShaders[(int)RdrShaderMode::Count];
	RdrResourceHandle hTextures[kMaxTextures];
	RdrSamplerState samplers[kMaxTextures];
	uint texCount;
	bool bNeedsLighting;
};