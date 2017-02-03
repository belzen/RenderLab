#pragma once

#include "IViewModel.h"
#include "components/PostProcessVolume.h"

class PostProcessVolumeViewModel : public IViewModel
{
public:
	void SetTarget(PostProcessVolume* pVolume);

	const char* GetTypeName();
	const PropertyDef** GetProperties();

private:
	// Property def callbacks
	static float GetWhiteLevel(void* pSource);
	static bool SetWhiteLevel(const float whiteLevel, void* pSource);

	static float GetMiddleGrey(void* pSource);
	static bool SetMiddleGrey(const float middleGrey, void* pSource);

	static float GetMinExposure(void* pSource);
	static bool SetMinExposure(const float minExposure, void* pSource);

	static float GetMaxExposure(void* pSource);
	static bool SetMaxExposure(const float maxExposure, void* pSource);

	static float GetAdaptationSpeed(void* pSource);
	static bool SetAdaptationSpeed(const float adaptationSpeed, void* pSource);

	static float GetBloomThreshold(void* pSource);
	static bool SetBloomThreshold(const float bloomThreshold, void* pSource);

	static bool GetBloomEnabled(void* pSource);
	static bool SetBloomEnabled(const bool enabled, void* pSource);

	static bool GetSsaoEnabled(void* pSource);
	static bool SetSsaoEnabled(const bool enabled, void* pSource);

	static float GetSsaoSampleRadius(void* pSource);
	static bool SetSsaoSampleRadius(const float radius, void* pSource);

private:
	PostProcessVolume* m_pTarget;
};