#pragma once

#include "EntityComponent.h"
#include "FreeList.h"
#include "MathLib/Vec3.h"
#include "AssetLib/SceneAsset.h"

class Light;
typedef FreeList<Light, 6 * 1024> LightFreeList;

class Light : public EntityComponent
{
public:
	static LightFreeList& GetFreeList();
	static Light* CreateDirectional(const Vec3& color, float intensity, float pssmLambda);
	static Light* CreateSpot(const Vec3& color, float intensity, float radius, float innerConeAngle, float outerConeAngle);
	static Light* CreatePoint(const Vec3& color, float intensity, const float radius);
	static Light* CreateEnvironment(bool isGlobal);

public:
	void OnAttached(Entity* pEntity);
	void OnDetached(Entity* pEntity);

	void Release();

	LightType GetType() const;
	void SetType(LightType lightType);

	// Calculate light direction based on parent entity rotation.
	Vec3 GetDirection() const;

	// Get the light's true color (ColorRgb * Intensity)
	const Vec3& GetColor() const;

	// Get/set the light's clamped RGB color.
	const Vec3& GetColorRgb() const;
	void SetColorRgb(const Vec3& color);

	float GetIntensity() const;
	void SetIntensity(float intensity);

	float GetRadius() const;
	void SetRadius(float radius);

	// Directional light only
	float GetPssmLambda() const;
	void SetPssmLambda(float lambda);

	// Spot light only
	float GetInnerConeAngle() const;
	void SetInnerConeAngle(float angle);

	float GetOuterConeAngle() const;
	void SetOuterConeAngle(float angle);

	float GetInnerConeAngleCos() const;
	float GetOuterConeAngleCos() const;

	// Environment light only
	bool IsGlobalEnvironmentLight() const;

	void SetEnvironmentTextureIndex(int index);
	int GetEnvironmentTextureIndex() const;

private:
	friend LightFreeList;
	Light() {}
	Light(Light&);
	virtual ~Light() {}

private:
	LightType m_type;

	// These need to be kept separate for editor usage until I get around to making an HSV color picker.
	// If the color and intensity are not kept separate, the original RGB value can't be determined.
	Vec3 m_color; // ColorRgb * Intensity
	Vec3 m_colorRgb;
	float m_intensity;

	float m_radius;
	float m_innerConeAngle;
	float m_outerConeAngle;
	float m_innerConeAngleCos;
	float m_outerConeAngleCos;
	float m_pssmLambda;

	int m_environmentTextureIndex;
	bool m_isGlobalEnvironmentLight;
};

//////////////////////////////////////////////////////////////////////////

inline LightType Light::GetType() const
{
	return m_type;
}

inline const Vec3& Light::GetColor() const
{
	return m_color;
}

inline const Vec3& Light::GetColorRgb() const
{
	return m_colorRgb;
}

inline float Light::GetIntensity() const
{
	return m_intensity;
}

inline float Light::GetRadius() const
{
	return m_radius;
}

inline float Light::GetPssmLambda() const
{
	return m_pssmLambda;
}

inline float Light::GetInnerConeAngle() const
{
	return m_innerConeAngle;
}

inline float Light::GetOuterConeAngle() const
{
	return m_outerConeAngle;
}

inline float Light::GetInnerConeAngleCos() const
{
	return m_innerConeAngleCos;
}

inline float Light::GetOuterConeAngleCos() const
{
	return m_outerConeAngleCos;
}

inline bool Light::IsGlobalEnvironmentLight() const
{
	return m_isGlobalEnvironmentLight;
}

inline int Light::GetEnvironmentTextureIndex() const
{
	return m_environmentTextureIndex;
}
