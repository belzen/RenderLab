#pragma once

#include "UtilsLib/Array.h"
#include "RdrShaders.h"
#include "RdrResource.h"

struct RdrMaterial
{
	static const uint kMaxTextures = 12;

	static const RdrMaterial* LoadFromFile(const char* materialName);
	static void ReloadMaterial(const char* materialName);
	// Retrieve a unique id for the material.
	static uint16 GetMaterialId(const RdrMaterial* pMaterial);

	RdrShaderHandle hPixelShaders[(int)RdrShaderMode::Count];
	Array<RdrResourceHandle, kMaxTextures> ahTextures;
	Array<RdrSamplerState, 4> aSamplers;
	bool bNeedsLighting;
	bool bAlphaCutout;
};