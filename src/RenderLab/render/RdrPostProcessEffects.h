#pragma once

#include "AssetLib/PostProcessEffectsAsset.h"
#include "RdrResource.h"

class RdrPostProcessEffects
{
public:
	RdrPostProcessEffects();

	void Init(const AssetLib::PostProcessEffects* pEffects);

	void PrepareDraw(float dt);

	RdrConstantBufferHandle GetToneMapInputConstants() const;
	const AssetLib::PostProcessEffects* GetEffectsAsset() const;

private:
	RdrConstantBufferHandle m_hToneMapInputConstants;
	const AssetLib::PostProcessEffects* m_pEffects;
	uint m_updateTime;
};

inline RdrConstantBufferHandle RdrPostProcessEffects::GetToneMapInputConstants() const
{
	return m_hToneMapInputConstants;
}

inline const AssetLib::PostProcessEffects* RdrPostProcessEffects::GetEffectsAsset() const
{
	return m_pEffects;
}