#pragma once

#include "components/ModelInstance.h"

class RdrDrawBuckets;
class ModelInstance;
class RigidBody;
class Light;
class WorldObject;
typedef FreeList<WorldObject, 6 * 1024> WorldObjectFreeList;

class WorldObject
{
public:
	static WorldObject* Create(const char* name, Vec3 pos, Quaternion orientation, Vec3 scale);
	void Release();

	const char* GetName() const;
	void SetName(const char* name);

	void AttachModel(ModelInstance* pModel);
	const ModelInstance* GetModel() const;

	void AttachRigidBody(RigidBody* pRigidBody);
	const RigidBody* GetRigidBody() const;

	void AttachLight(Light* pLight);
	const Light* GetLight() const;

	const Vec3& GetPosition() const;
	void SetPosition(Vec3 pos);

	const Quaternion& GetOrientation() const;

	const Vec3& GetScale() const;
	void SetScale(Vec3 scale);

	const Matrix44 GetTransform() const;

	float GetRadius() const;

	void Update();
	void QueueDraw(RdrDrawBuckets* pDrawBuckets);

private:
	friend WorldObjectFreeList;

	char m_name[64];

	Vec3 m_position;
	Vec3 m_scale;
	Quaternion m_orientation;
	bool m_transformChanged;

	ModelInstance* m_pModel;
	RigidBody* m_pRigidBody;
	Light* m_pLight;
};

//////////////////////////////////////////////////////////////////////////

inline const Matrix44 WorldObject::GetTransform() const
{
	return Matrix44Transformation(Vec3::kOrigin, Quaternion::kIdentity, m_scale, Vec3::kOrigin, m_orientation, m_position);
}

inline const ModelInstance* WorldObject::GetModel() const
{ 
	return m_pModel; 
}

inline const RigidBody* WorldObject::GetRigidBody() const
{
	return m_pRigidBody;
}

inline const Light* WorldObject::GetLight() const
{
	return m_pLight;
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
	if (m_pModel)
	{
		return m_pModel->GetRadius() * Vec3MaxComponent(m_scale);
	}
	else
	{
		// Some non-zero value.
		return 0.00001f;
	}
}

inline const char* WorldObject::GetName() const
{
	return m_name;
}

inline void WorldObject::SetName(const char* name)
{
	strcpy_s(m_name, name);
}
