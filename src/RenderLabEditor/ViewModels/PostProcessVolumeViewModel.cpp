#include "Precompiled.h"
#include "PostProcessVolumeViewModel.h"
#include "PropertyTables.h"

void PostProcessVolumeViewModel::SetTarget(PostProcessVolume* pVolume)
{
	m_pTarget = pVolume;
}

const char* PostProcessVolumeViewModel::GetTypeName()
{
	return "Post-Process Volume";
}

const PropertyDef** PostProcessVolumeViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
		new BeginGroupPropertyDef("Eye Adaptation"),
		new FloatPropertyDef("White Level", "Tone-map white level", 0.1f, 30.f, 0.05f, GetWhiteLevel, SetWhiteLevel),
		new FloatPropertyDef("Middle Grey", "", 0.01f, 1.f, 0.01f, GetMiddleGrey, SetMiddleGrey),
		new FloatPropertyDef("Min Exposure", "", -16.f, 16.f, 0.1f, GetMinExposure, SetMinExposure),
		new FloatPropertyDef("Max Exposure", "", -16.f, 16.f, 0.1f, GetMaxExposure, SetMaxExposure),
		new FloatPropertyDef("Adaptation Speed", "", 0.1f, 10.f, 0.1f, GetAdaptationSpeed, SetAdaptationSpeed),
		new EndGroupPropertyDef(),
		new BeginGroupPropertyDef("Bloom"),
		new BooleanPropertyDef("Enabled", nullptr, GetBloomEnabled, SetBloomEnabled),
		new FloatPropertyDef("Threshold", nullptr, 0.1f, 30.f, 0.1f, GetBloomThreshold, SetBloomThreshold),
		new EndGroupPropertyDef(),
		new BeginGroupPropertyDef("SSAO"),
		new BooleanPropertyDef("Enabled", nullptr, GetSsaoEnabled, SetSsaoEnabled),
		new FloatPropertyDef("Sample Radius", nullptr, 0.1f, 3.f, 0.01f, GetSsaoSampleRadius, SetSsaoSampleRadius),
		new EndGroupPropertyDef(),
		nullptr
	};
	return s_ppProperties;
}

float PostProcessVolumeViewModel::GetWhiteLevel(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().eyeAdaptation.white;
}

bool PostProcessVolumeViewModel::SetWhiteLevel(const float whiteLevel, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().eyeAdaptation.white = whiteLevel;
	return true;
}

float PostProcessVolumeViewModel::GetMiddleGrey(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().eyeAdaptation.middleGrey;
}

bool PostProcessVolumeViewModel::SetMiddleGrey(const float middleGrey, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().eyeAdaptation.middleGrey = middleGrey;
	return true;
}

float PostProcessVolumeViewModel::GetMinExposure(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().eyeAdaptation.minExposure;
}

bool PostProcessVolumeViewModel::SetMinExposure(const float minExposure, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().eyeAdaptation.minExposure = minExposure;
	return true;
}

float PostProcessVolumeViewModel::GetMaxExposure(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().eyeAdaptation.maxExposure;
}

bool PostProcessVolumeViewModel::SetMaxExposure(const float maxExposure, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().eyeAdaptation.maxExposure = maxExposure;
	return true;
}

float PostProcessVolumeViewModel::GetAdaptationSpeed(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().eyeAdaptation.adaptationSpeed;
}

bool PostProcessVolumeViewModel::SetAdaptationSpeed(const float adaptationSpeed, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().eyeAdaptation.adaptationSpeed = adaptationSpeed;
	return true;
}

float PostProcessVolumeViewModel::GetBloomThreshold(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().bloom.threshold;
}

bool PostProcessVolumeViewModel::SetBloomThreshold(const float bloomThreshold, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().bloom.threshold = bloomThreshold;
	return true;
}

bool PostProcessVolumeViewModel::GetBloomEnabled(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().bloom.enabled;
}

bool PostProcessVolumeViewModel::SetBloomEnabled(const bool enabled, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().bloom.enabled = enabled;
	return true;
}

bool PostProcessVolumeViewModel::GetSsaoEnabled(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().ssao.enabled;
}

bool PostProcessVolumeViewModel::SetSsaoEnabled(const bool enabled, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().ssao.enabled = enabled;
	return true;
}

float PostProcessVolumeViewModel::GetSsaoSampleRadius(void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	return pViewModel->m_pTarget->GetEffects().ssao.sampleRadius;
}

bool PostProcessVolumeViewModel::SetSsaoSampleRadius(const float radius, void* pSource)
{
	PostProcessVolumeViewModel* pViewModel = (PostProcessVolumeViewModel*)pSource;
	pViewModel->m_pTarget->GetEffects().ssao.sampleRadius = radius;
	return true;
}
