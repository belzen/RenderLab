#pragma once

#include "IViewModel.h"
#include "components/Light.h"

class LightViewModel : public IViewModel
{
public:
	void SetTarget(Light* pLight);

	const char* GetTypeName();
	const PropertyDef** GetProperties();

private:
	static int GetLightType(void* pSource);
	static bool SetLightType(const int lightType, void* pSource);

	static Vec3 GetColor(void* pSource);
	static bool SetColor(const Vec3& color, void* pSource);
	static bool SetColorR(float val, void* pSource);
	static bool SetColorG(float val, void* pSource);
	static bool SetColorB(float val, void* pSource);

	static float GetIntensity(void* pSource);
	static bool SetIntensity(float intensity, void* pSource);

	static float GetRadius(void* pSource);
	static bool SetRadius(float radius, void* pSource);

	static float GetPssmLambda(void* pSource);
	static bool SetPssmLambda(float lambda, void* pSource);

	static float GetInnerConeAngle(void* pSource);
	static bool SetInnerConeAngle(float angle, void* pSource);

	static float GetOuterConeAngle(void* pSource);
	static bool SetOuterConeAngle(float angle, void* pSource);

private:
	Light* m_pLight;
};