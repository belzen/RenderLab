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

	static bool GetVolFogEnabled(void* pSource);
	static bool SetVolFogEnabled(const bool enabled, void* pSource);

	static Vec3 GetVolFogScattering(void* pSource);
	static bool SetVolFogScattering(const Vec3& scattering, void* pSource);
	static bool SetVolFogScatteringX(const float value, void* pSource);
	static bool SetVolFogScatteringY(const float value, void* pSource);
	static bool SetVolFogScatteringZ(const float value, void* pSource);

	static Vec3 GetVolFogAbsorption(void* pSource);
	static bool SetVolFogAbsorption(const Vec3& absorption, void* pSource);
	static bool SetVolFogAbsorptionX(const float value, void* pSource);
	static bool SetVolFogAbsorptionY(const float value, void* pSource);
	static bool SetVolFogAbsorptionZ(const float value, void* pSource);

	static float GetVolFogPhaseG(void* pSource);
	static bool SetVolFogPhaseG(const float phaseG, void* pSource);

	static float GetVolFogFarDepth(void* pSource);
	static bool SetVolFogFarDepth(const float farDepth, void* pSource);

private:
	AssetLib::Sky* m_pSky;
};