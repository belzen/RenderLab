#pragma once

#include "AssetLib/PostProcessEffectsAsset.h"
#include "RdrResource.h"

class Renderer;
class RdrContext;

class RdrPostProcessEffects
{
public:
	RdrPostProcessEffects();

	void Init(const AssetLib::PostProcessEffects* pEffects);

	void PrepareForDraw(Renderer& rRenderer);

	RdrConstantBufferHandle GetToneMapInputConstants() const;

private:
	RdrConstantBufferHandle m_hToneMapInputConstants;
	AssetLib::PostProcessEffects m_effects;
};

inline RdrConstantBufferHandle RdrPostProcessEffects::GetToneMapInputConstants() const
{
	return m_hToneMapInputConstants;
}