#include "Precompiled.h"
#include "PostProcessEffectsViewModel.h"
#include "PropertyTables.h"

void PostProcessEffectsViewModel::SetTarget(AssetLib::PostProcessEffects* pEffects)
{
	m_pTarget = pEffects;
}

const PropertyDef** PostProcessEffectsViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
			new BeginGroupPropertyDef("Eye Adaptation"),
			new FloatPropertyDef("White Level", "Tone-map white level", 0.1f, 30.f, 0.05f, GetWhiteLevel, SetWhiteLevel),
			new FloatPropertyDef("Middle Grey", "", 0.01f, 1.f, 0.01f, GetMiddleGrey, SetMiddleGrey),
			new FloatPropertyDef("Min Exposure", "", -16.f, 16.f, 0.1f, GetMinExposure, SetMinExposure),
			new FloatPropertyDef("Max Exposure", "", -16.f, 16.f, 0.1f, GetMaxExposure, SetMaxExposure),
			new EndGroupPropertyDef(),
			new BeginGroupPropertyDef("Bloom"),
			new FloatPropertyDef("Threshold", nullptr, 0.1f, 30.f, 0.1f, GetBloomThreshold, SetBloomThreshold),
			new EndGroupPropertyDef(),
			nullptr
	};
	return s_ppProperties;
}

float PostProcessEffectsViewModel::GetWhiteLevel(void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	return pViewModel->m_pTarget->eyeAdaptation.white;
}

bool PostProcessEffectsViewModel::SetWhiteLevel(const float whiteLevel, void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	pViewModel->m_pTarget->eyeAdaptation.white = whiteLevel;
	pViewModel->m_pTarget->timeLastModified = timeGetTime();
	return true;
}

float PostProcessEffectsViewModel::GetMiddleGrey(void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	return pViewModel->m_pTarget->eyeAdaptation.middleGrey;
}

bool PostProcessEffectsViewModel::SetMiddleGrey(const float middleGrey, void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	pViewModel->m_pTarget->eyeAdaptation.middleGrey = middleGrey;
	pViewModel->m_pTarget->timeLastModified = timeGetTime();
	return true;
}

float PostProcessEffectsViewModel::GetMinExposure(void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	return pViewModel->m_pTarget->eyeAdaptation.minExposure;
}

bool PostProcessEffectsViewModel::SetMinExposure(const float minExposure, void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	pViewModel->m_pTarget->eyeAdaptation.minExposure = minExposure;
	pViewModel->m_pTarget->timeLastModified = timeGetTime();
	return true;
}

float PostProcessEffectsViewModel::GetMaxExposure(void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	return pViewModel->m_pTarget->eyeAdaptation.maxExposure;
}

bool PostProcessEffectsViewModel::SetMaxExposure(const float maxExposure, void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	pViewModel->m_pTarget->eyeAdaptation.maxExposure = maxExposure;
	pViewModel->m_pTarget->timeLastModified = timeGetTime();
	return true;
}

float PostProcessEffectsViewModel::GetBloomThreshold(void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	return pViewModel->m_pTarget->bloom.threshold;
}

bool PostProcessEffectsViewModel::SetBloomThreshold(const float bloomThreshold, void* pSource)
{
	PostProcessEffectsViewModel* pViewModel = (PostProcessEffectsViewModel*)pSource;
	pViewModel->m_pTarget->bloom.threshold = bloomThreshold;
	pViewModel->m_pTarget->timeLastModified = timeGetTime();
	return true;
}
