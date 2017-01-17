#include "Precompiled.h"
#include "LightViewModel.h"
#include "PropertyTables.h"

void LightViewModel::SetTarget(Light* pLight)
{
	m_pLight = pLight;
}

const char* LightViewModel::GetTypeName()
{
	return "Light";
}

const PropertyDef** LightViewModel::GetProperties()
{
	static const NameValuePair kLightTypeChoices[] = {
		{ L"Directional", (int)LightType::Directional },
		{ L"Point", (int)LightType::Point },
		{ L"Spot", (int)LightType::Spot },
		{ L"Environment", (int)LightType::Environment }
	};

	static const PropertyDef* s_ppProperties[] = {
		new IntChoicePropertyDef("Type", nullptr, kLightTypeChoices, ARRAY_SIZE(kLightTypeChoices), GetLightType, SetLightType),
		new Vector3PropertyDef("Color", nullptr, GetColor, SetColor, SetColorR, SetColorG, SetColorB),
		new FloatPropertyDef("Intensity", nullptr, 0.1f, 1000.f, 0.1f, GetIntensity, SetIntensity),
		new FloatPropertyDef("Radius", nullptr, 0.1f, 500.f, 0.1f, GetRadius, SetRadius),
		new FloatPropertyDef("Inner Cone Angle", nullptr, 0.1f, 90.f, 0.1f, GetInnerConeAngle, SetInnerConeAngle),
		new FloatPropertyDef("Outer Cone Angle", nullptr, 0.1f, 90.f, 0.1f, GetOuterConeAngle, SetOuterConeAngle),
		new FloatPropertyDef("PSSM Lambda", "Blend value between log and linear shadow distance for cascaded shadows", 0.f, 1.f, 0.001f, GetPssmLambda, SetPssmLambda),
		nullptr
	};
	return s_ppProperties;
}

int LightViewModel::GetLightType(void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	return (int)pViewModel->m_pLight->GetType();
}

bool LightViewModel::SetLightType(const int lightType, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	pViewModel->m_pLight->SetType((LightType)lightType);
	return true;
}

Vec3 LightViewModel::GetColor(void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	return pViewModel->m_pLight->GetColorRgb();
}

bool LightViewModel::SetColor(const Vec3& color, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	pViewModel->m_pLight->SetColorRgb(color);
	return true;
}

bool LightViewModel::SetColorR(float val, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	Vec3 color = pViewModel->m_pLight->GetColorRgb();
	color.x = val;
	pViewModel->m_pLight->SetColorRgb(color);
	return true;
}

bool LightViewModel::SetColorG(float val, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	Vec3 color = pViewModel->m_pLight->GetColorRgb();
	color.y = val;
	pViewModel->m_pLight->SetColorRgb(color);
	return true;
}

bool LightViewModel::SetColorB(float val, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	Vec3 color = pViewModel->m_pLight->GetColorRgb();
	color.z = val;
	pViewModel->m_pLight->SetColorRgb(color);
	return true;
}

float LightViewModel::GetIntensity(void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	return pViewModel->m_pLight->GetIntensity();
}

bool LightViewModel::SetIntensity(float intensity, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	pViewModel->m_pLight->SetIntensity(intensity);
	return true;
}

float LightViewModel::GetRadius(void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	return pViewModel->m_pLight->GetRadius();
}

bool LightViewModel::SetRadius(float radius, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	pViewModel->m_pLight->SetRadius(radius);
	return true;
}

float LightViewModel::GetPssmLambda(void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	return pViewModel->m_pLight->GetPssmLambda();
}

bool LightViewModel::SetPssmLambda(float lambda, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	pViewModel->m_pLight->SetPssmLambda(lambda);
	return true;
}

float LightViewModel::GetInnerConeAngle(void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	return Maths::RadToDeg(pViewModel->m_pLight->GetInnerConeAngle());
}

bool LightViewModel::SetInnerConeAngle(float angle, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	pViewModel->m_pLight->SetInnerConeAngle(Maths::DegToRad(angle));
	return true;
}

float LightViewModel::GetOuterConeAngle(void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	return Maths::RadToDeg(pViewModel->m_pLight->GetOuterConeAngle());
}

bool LightViewModel::SetOuterConeAngle(float angle, void* pSource)
{
	LightViewModel* pViewModel = (LightViewModel*)pSource;
	pViewModel->m_pLight->SetOuterConeAngle(Maths::DegToRad(angle));
	return true;
}
