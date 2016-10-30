#include "Precompiled.h"
#include "SkyViewModel.h"
#include "PropertyTables.h"

void SkyViewModel::SetTarget(AssetLib::Sky* pSky)
{
	m_pSky = pSky;
}

const PropertyDef** SkyViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
		new FloatPropertyDef("Sun Intensity", "", 0.f, 1000.f, 1.f, GetSunIntensity, SetSunIntensity),
		new FloatPropertyDef("Sun Angle X-Axis", "", -90.f, 270.f, 0.5f, GetSunAngleXAxis, SetSunAngleXAxis),
		new FloatPropertyDef("Sun Angle Z-Axis", "", -90.f, 270.f, 0.5f, GetSunAngleZAxis, SetSunAngleZAxis),
		new FloatPropertyDef("PSSM Lambda", "", 0, 1.f, 0.01f, GetPssmLambda, SetPssmLambda),
		new BeginGroupPropertyDef("Volumetric Fog"),
		new Vector3PropertyDef("Scattering", "", GetVolFogScattering, SetVolFogScattering, SetVolFogScatteringX, SetVolFogScatteringY, SetVolFogScatteringZ),
		new Vector3PropertyDef("Absorption", "", GetVolFogAbsorption, SetVolFogAbsorption, SetVolFogAbsorptionX, SetVolFogAbsorptionY, SetVolFogAbsorptionZ),
		new FloatPropertyDef("Phase G", "", -1.f, 1.f, 0.05f, GetVolFogPhaseG, SetVolFogPhaseG),
		new FloatPropertyDef("Far Depth", "", 1.f, 128.f, 1.f, GetVolFogFarDepth, SetVolFogFarDepth),
		new EndGroupPropertyDef(),
		nullptr
	};
	return s_ppProperties;
}

float SkyViewModel::GetSunAngleXAxis(void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	return pViewModel->m_pSky->sun.angleXAxis;
}

bool SkyViewModel::SetSunAngleXAxis(const float angleDegrees, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->sun.angleXAxis = angleDegrees;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

float SkyViewModel::GetSunAngleZAxis(void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	return pViewModel->m_pSky->sun.angleZAxis;
}

bool SkyViewModel::SetSunAngleZAxis(const float angleDegrees, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->sun.angleZAxis = angleDegrees;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

float SkyViewModel::GetSunIntensity(void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	return pViewModel->m_pSky->sun.intensity;
}

bool SkyViewModel::SetSunIntensity(const float intensity, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->sun.intensity = intensity;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

float SkyViewModel::GetPssmLambda(void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	return pViewModel->m_pSky->shadows.pssmLambda;
}

bool SkyViewModel::SetPssmLambda(const float lambda, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->shadows.pssmLambda = lambda;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

Vec3 SkyViewModel::GetVolFogScattering(void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	return pViewModel->m_pSky->volumetricFog.scatteringCoeff;
}

bool SkyViewModel::SetVolFogScattering(const Vec3& scattering, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.scatteringCoeff = scattering;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

bool SkyViewModel::SetVolFogScatteringX(const float value, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.scatteringCoeff.x = value;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

bool SkyViewModel::SetVolFogScatteringY(const float value, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.scatteringCoeff.y = value;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

bool SkyViewModel::SetVolFogScatteringZ(const float value, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.scatteringCoeff.z = value;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

Vec3 SkyViewModel::GetVolFogAbsorption(void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	return pViewModel->m_pSky->volumetricFog.absorptionCoeff;
}

bool SkyViewModel::SetVolFogAbsorption(const Vec3& absorption, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.absorptionCoeff= absorption;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

bool SkyViewModel::SetVolFogAbsorptionX(const float value, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.absorptionCoeff.x = value;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

bool SkyViewModel::SetVolFogAbsorptionY(const float value, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.absorptionCoeff.y = value;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

bool SkyViewModel::SetVolFogAbsorptionZ(const float value, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.absorptionCoeff.z = value;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

float SkyViewModel::GetVolFogPhaseG(void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	return pViewModel->m_pSky->volumetricFog.phaseG;
}

bool SkyViewModel::SetVolFogPhaseG(const float phaseG, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.phaseG = phaseG;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}

float SkyViewModel::GetVolFogFarDepth(void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	return pViewModel->m_pSky->volumetricFog.farDepth;
}

bool SkyViewModel::SetVolFogFarDepth(const float farDepth, void* pSource)
{
	SkyViewModel* pViewModel = (SkyViewModel*)pSource;
	pViewModel->m_pSky->volumetricFog.farDepth = farDepth;
	pViewModel->m_pSky->timeLastModified = timeGetTime();
	return true;
}
