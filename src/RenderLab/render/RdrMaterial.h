#pragma once

#include "UtilsLib/Array.h"
#include "UtilsLib/StringCache.h"
#include "RdrShaders.h"
#include "RdrResource.h"

struct RdrMaterial
{
	static const uint kMaxTextures = 12;

	// Retrieve a unique id for the material.
	static uint16 GetMaterialId(const RdrMaterial* pMaterial);
	static RdrMaterial* Create(const CachedString& materialName, const RdrVertexInputElement* pInputLayout, uint nNumInputElements);
	static void ReloadMaterial(const char* materialName);

	CachedString name;
	bool bNeedsLighting;
	bool bAlphaCutout;
	bool bHasAlpha;

	bool IsShaderStageActive(RdrShaderMode eMode, RdrShaderStageFlags eStageFlag) const
	{
		return (activeShaderStages[(uint)eMode] & eStageFlag) != RdrShaderStageFlags::None;
	}

	void CreatePipelineState(
		RdrShaderMode eMode,
		const RdrVertexShader& vertexShader, const RdrShader* pPixelShader,
		const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
		const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
		const RdrBlendMode eBlendMode,
		const RdrRasterState& rasterState,
		const RdrDepthStencilState& depthStencilState);

	RdrShaderStageFlags activeShaderStages[(int)RdrShaderMode::Count];

	RdrPipelineState hPipelineStates[(int)RdrShaderMode::Count];
	Array<RdrResourceHandle, kMaxTextures> ahTextures;
	Array<RdrSamplerState, 4> aSamplers;
	RdrConstantBufferHandle hConstants;

	struct
	{
		RdrConstantBufferHandle hDsConstants;
		Array<RdrResourceHandle, 2> ahResources;
		Array<RdrSamplerState, 2> aSamplers;
	} tessellation;
};