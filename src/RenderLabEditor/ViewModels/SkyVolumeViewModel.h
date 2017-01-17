#pragma once

#include "IViewModel.h"
#include "components/SkyVolume.h"

class SkyVolumeViewModel : public IViewModel
{
public:
	void SetTarget(SkyVolume* pSky);

	const char* GetTypeName();
	const PropertyDef** GetProperties();

private:
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
	SkyVolume* m_pSky;
};