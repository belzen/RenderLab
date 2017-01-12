#pragma once

#include "ObjectComponent.h"
#include "FreeList.h"
#include "MathLib/Vec3.h"
#include "AssetLib/SceneAsset.h"

class Light;
typedef FreeList<Light, 6 * 1024> LightFreeList;

class Light : public ObjectComponent
{
public:
	static LightFreeList& GetFreeList();

	static Light* CreateDirectional(const Vec3& color, const Vec3& direction);
	static Light* CreateSpot(const Vec3& color, const Vec3& direction, float radius, float innerConeAngle, float outerConeAngle);
	static Light* CreatePoint(const Vec3& color, const float radius);
	static Light* CreateEnvironment(bool isGlobal);

public:
	void OnAttached(WorldObject* pObject);
	void OnDetached(WorldObject* pObject);

	void Release();

	LightType GetType() const;

	const Vec3& GetDirection() const;
	const Vec3& GetColor() const;
	float GetRadius() const;

	// Spot light only
	float GetInnerConeAngle() const;
	float GetOuterConeAngle() const;
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

private:
	LightType m_type;
	Vec3 m_direction;
	Vec3 m_color;

	float m_radius;
	float m_innerConeAngle;
	float m_outerConeAngle;
	float m_innerConeAngleCos;
	float m_outerConeAngleCos;

	int m_environmentTextureIndex;
	bool m_isGlobalEnvironmentLight;
};

//////////////////////////////////////////////////////////////////////////

inline LightType Light::GetType() const
{
	return m_type;
}

inline const Vec3& Light::GetDirection() const
{
	return m_direction;
}

inline const Vec3& Light::GetColor() const
{
	return m_color;
}

inline float Light::GetRadius() const
{
	return m_radius;
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
