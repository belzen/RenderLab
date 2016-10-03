#pragma once

#include "render/ModelInstance.h"

class WorldObject;
typedef FreeList<WorldObject, 6 * 1024> WorldObjectFreeList;

class WorldObject
{
public:
	static WorldObject* Create(const char* name, ModelInstance* pModel, Vec3 pos, Quaternion orientation, Vec3 scale);
	void Release();

	const char* GetName() const;
	void SetName(const char* name);

	void SetModel(ModelInstance* pModel);
	const ModelInstance* GetModel() const;

	const Vec3& GetPosition() const;
	void SetPosition(Vec3 pos);

	const Quaternion& GetOrientation() const;

	const Vec3& GetScale() const;
	void SetScale(Vec3 scale);

	const Matrix44 GetTransform() const;

	float GetRadius() const;

	void PrepareDraw();

private:
	friend WorldObjectFreeList;

	char m_name[64];

	Vec3 m_position;
	Vec3 m_scale;
	Quaternion m_orientation;
	bool m_transformChanged;

	ModelInstance* m_pModel;
};

inline const Matrix44 WorldObject::GetTransform() const
{
	return Matrix44Transformation(Vec3::kOrigin, Quaternion::kIdentity, m_scale, Vec3::kOrigin, m_orientation, m_position);
}

inline void WorldObject::SetModel(ModelInstance* pModel)
{
	m_pModel = pModel; 
	// todo: destroy old model
}

inline const ModelInstance* WorldObject::GetModel() const
{ 
	return m_pModel; 
}

inline const Vec3& WorldObject::GetPosition() const
{
	return m_position; 
}

inline void WorldObject::SetPosition(Vec3 pos)
{ 
	m_position = pos;
	m_transformChanged = true;
}

inline const Quaternion& WorldObject::GetOrientation() const
{ 
	return m_orientation; 
}

inline const Vec3& WorldObject::GetScale() const
{ 
	return m_scale;
}

inline void WorldObject::SetScale(Vec3 scale)
{ 
	m_scale = scale;
	m_transformChanged = true;
}

inline float WorldObject::GetRadius() const
{
	return m_pModel->GetRadius() * Vec3MaxComponent(m_scale);
}

inline const char* WorldObject::GetName() const
{
	return m_name;
}

inline void WorldObject::SetName(const char* name)
{
	strcpy_s(m_name, name);
}
