#pragma once

#include "IViewModel.h"

class WorldObject;

class WorldObjectViewModel : public IViewModel
{
public:
	void SetTarget(WorldObject* pObject);

	const PropertyDef** GetProperties();

private:
	static std::string GetName(void* pSource);
	static bool SetName(const std::string& name, void* pSource);

	static std::string GetModel(void* pSource);
	static bool SetModel(const std::string& modelName, void* pSource);

	static Vec3 GetPosition(void* pSource);
	static bool SetPosition(const Vec3& position, void* pSource);
	static bool SetPositionX(const float newValue, void* pSource);
	static bool SetPositionY(const float newValue, void* pSource);
	static bool SetPositionZ(const float newValue, void* pSource);

	static Vec3 GetScale(void* pSource);
	static bool SetScale(const Vec3& position, void* pSource);
	static bool SetScaleX(const float newValue, void* pSource);
	static bool SetScaleY(const float newValue, void* pSource);
	static bool SetScaleZ(const float newValue, void* pSource);

private:
	WorldObject* m_pObject;
};