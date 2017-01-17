#include "Precompiled.h"
#include "SkyVolumeViewModel.h"
#include "PropertyTables.h"

void SkyVolumeViewModel::SetTarget(SkyVolume* pSky)
{
	m_pSky = pSky;
}

const char* SkyVolumeViewModel::GetTypeName()
{
	return "Sky Volume";
}

const PropertyDef** SkyVolumeViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
		new BeginGroupPropertyDef("Volumetric Fog"),
		new BooleanPropertyDef("Enabled", nullptr, GetVolFogEnabled, SetVolFogEnabled),
		new Vector3PropertyDef("Scattering", "", GetVolFogScattering, SetVolFogScattering, SetVolFogScatteringX, SetVolFogScatteringY, SetVolFogScatteringZ),
		new Vector3PropertyDef("Absorption", "", GetVolFogAbsorption, SetVolFogAbsorption, SetVolFogAbsorptionX, SetVolFogAbsorptionY, SetVolFogAbsorptionZ),
		new FloatPropertyDef("Phase G", "", -1.f, 1.f, 0.05f, GetVolFogPhaseG, SetVolFogPhaseG),
		new FloatPropertyDef("Far Depth", "", 1.f, 128.f, 1.f, GetVolFogFarDepth, SetVolFogFarDepth),
		new EndGroupPropertyDef(),
		nullptr
	};
	return s_ppProperties;
}

bool SkyVolumeViewModel::GetVolFogEnabled(void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	return pViewModel->m_pSky->GetSkySettings().volumetricFog.enabled;
}

bool SkyVolumeViewModel::SetVolFogEnabled(const bool enabled, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.enabled = enabled;
	return true;
}

Vec3 SkyVolumeViewModel::GetVolFogScattering(void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	return pViewModel->m_pSky->GetSkySettings().volumetricFog.scatteringCoeff;
}

bool SkyVolumeViewModel::SetVolFogScattering(const Vec3& scattering, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.scatteringCoeff = scattering;
	return true;
}

bool SkyVolumeViewModel::SetVolFogScatteringX(const float value, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.scatteringCoeff.x = value;
	return true;
}

bool SkyVolumeViewModel::SetVolFogScatteringY(const float value, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.scatteringCoeff.y = value;
	return true;
}

bool SkyVolumeViewModel::SetVolFogScatteringZ(const float value, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.scatteringCoeff.z = value;
	return true;
}

Vec3 SkyVolumeViewModel::GetVolFogAbsorption(void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	return pViewModel->m_pSky->GetSkySettings().volumetricFog.absorptionCoeff;
}

bool SkyVolumeViewModel::SetVolFogAbsorption(const Vec3& absorption, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.absorptionCoeff= absorption;
	return true;
}

bool SkyVolumeViewModel::SetVolFogAbsorptionX(const float value, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.absorptionCoeff.x = value;
	return true;
}

bool SkyVolumeViewModel::SetVolFogAbsorptionY(const float value, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.absorptionCoeff.y = value;
	return true;
}

bool SkyVolumeViewModel::SetVolFogAbsorptionZ(const float value, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.absorptionCoeff.z = value;
	return true;
}

float SkyVolumeViewModel::GetVolFogPhaseG(void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	return pViewModel->m_pSky->GetSkySettings().volumetricFog.phaseG;
}

bool SkyVolumeViewModel::SetVolFogPhaseG(const float phaseG, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.phaseG = phaseG;
	return true;
}

float SkyVolumeViewModel::GetVolFogFarDepth(void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	return pViewModel->m_pSky->GetSkySettings().volumetricFog.farDepth;
}

bool SkyVolumeViewModel::SetVolFogFarDepth(const float farDepth, void* pSource)
{
	SkyVolumeViewModel* pViewModel = (SkyVolumeViewModel*)pSource;
	pViewModel->m_pSky->GetSkySettings().volumetricFog.farDepth = farDepth;
	return true;
}
