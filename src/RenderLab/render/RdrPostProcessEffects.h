#pragma once

#include "AssetLib/PostProcessEffectsAsset.h"
#include "RdrResource.h"

class RdrPostProcessEffects
{
public:
	RdrPostProcessEffects();

	void Init(const AssetLib::PostProcessEffects* pEffects);

	void PrepareDraw();

	RdrConstantBufferHandle GetToneMapInputConstants() const;

private:
	RdrConstantBufferHandle m_hToneMapInputConstants;
	AssetLib::PostProcessEffects m_effects;
};

inline RdrConstantBufferHandle RdrPostProcessEffects::GetToneMapInputConstants() const
{
	return m_hToneMapInputConstants;
}