#pragma once

#include "IViewModel.h"

class Entity;

class EntityViewModel : public IViewModel
{
public:
	void SetTarget(Entity* pObject);

	const char* GetTypeName();
	const PropertyDef** GetProperties();

private:
	static std::string GetName(void* pSource);
	static bool SetName(const std::string& name, void* pSource);

	static Vec3 GetPosition(void* pSource);
	static bool SetPosition(const Vec3& position, void* pSource);
	static bool SetPositionX(const float newValue, void* pSource);
	static bool SetPositionY(const float newValue, void* pSource);
	static bool SetPositionZ(const float newValue, void* pSource);

	static Rotation GetRotation(void* pSource);
	static bool SetRotation(const Rotation& rotation, void* pSource);
	static bool SetRotationPitch(const float newValue, void* pSource);
	static bool SetRotationYaw(const float newValue, void* pSource);
	static bool SetRotationRoll(const float newValue, void* pSource);

	static Vec3 GetScale(void* pSource);
	static bool SetScale(const Vec3& position, void* pSource);
	static bool SetScaleX(const float newValue, void* pSource);
	static bool SetScaleY(const float newValue, void* pSource);
	static bool SetScaleZ(const float newValue, void* pSource);

private:
	Entity* m_pEntity;
};