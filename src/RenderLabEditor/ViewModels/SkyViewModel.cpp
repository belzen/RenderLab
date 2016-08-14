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
		new FloatPropertyDef("Sun Angle X-Axis", "", -90.f, 270.f, 0.5f, GetSunAngleXAxis, SetSunAngleXAxis),
		new FloatPropertyDef("Sun Angle Z-Axis", "", -90.f, 270.f, 0.5f, GetSunAngleZAxis, SetSunAngleZAxis),
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
