#pragma once

#include "IViewModel.h"
#include "AssetLib/SkyAsset.h"

class SkyViewModel : public IViewModel
{
public:
	void SetTarget(AssetLib::Sky* pSky);

	const PropertyDef** GetProperties();

private:
	static float GetSunAngleXAxis(void* pSource);
	static bool SetSunAngleXAxis(const float angleDegrees, void* pSource);

	static float GetSunAngleZAxis(void* pSource);
	static bool SetSunAngleZAxis(const float angleDegrees, void* pSource);

	static float GetSunIntensity(void* pSource);
	static bool SetSunIntensity(const float intensity, void* pSource);

	static float GetPssmLambda(void* pSource);
	static bool SetPssmLambda(const float lambda, void* pSource);

private:
	AssetLib::Sky* m_pSky;
};