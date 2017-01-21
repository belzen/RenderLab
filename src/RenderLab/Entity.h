#pragma once

#include "FreeList.h"

class RdrDrawBuckets;
class Renderable;
class RigidBody;
class Light;
class VolumeComponent;

class Entity;
typedef FreeList<Entity, 6 * 1024> EntityFreeList;

class Entity
{
public:
	static EntityFreeList& GetFreeList();
	static Entity* Create(const char* name, Vec3 pos, Rotation rotation, Vec3 scale);

public:
	void Release();

	const char* GetName() const;
	void SetName(const char* name);

	void AttachRenderable(Renderable* pRenderable);
	const Renderable* GetRenderable() const;
	Renderable* GetRenderable();

	void AttachRigidBody(RigidBody* pRigidBody);
	const RigidBody* GetRigidBody() const;
	RigidBody* GetRigidBody();

	void AttachLight(Light* pLight);
	const Light* GetLight() const;
	Light* GetLight();

	void AttachVolume(VolumeComponent* pVolume);
	const VolumeComponent* GetVolume() const;
	VolumeComponent* GetVolume();

	const Vec3& GetPosition() const;
	void SetPosition(const Vec3& pos);

	const Quaternion& GetOrientation() const;
	void SetOrientation(const Quaternion& q);

	const Rotation& GetRotation() const;
	void SetRotation(const Rotation& rotation);

	const Vec3& GetScale() const;
	void SetScale(const Vec3& scale);

	const Matrix44 GetTransform() const;
	int GetTransformId() const;

private:
	friend EntityFreeList;
	Entity() {}
	Entity(const Entity&);

private:
	char m_name[64];

	Vec3 m_position;
	Vec3 m_scale;
	Quaternion m_orientation;
	Rotation m_rotation;
	int m_transformId;

	Renderable* m_pRenderable;
	RigidBody* m_pRigidBody;
	Light* m_pLight;
	VolumeComponent* m_pVolume;
};

//////////////////////////////////////////////////////////////////////////

inline const Matrix44 Entity::GetTransform() const
{
	return Matrix44Transformation(Vec3::kOrigin, Quaternion::kIdentity, m_scale, Vec3::kOrigin, m_orientation, m_position);
}

inline int Entity::GetTransformId() const
{
	return m_transformId;
}

inline Renderable* Entity::GetRenderable()
{
	return m_pRenderable;
}

inline const Renderable* Entity::GetRenderable() const
{ 
	return m_pRenderable; 
}

inline const RigidBody* Entity::GetRigidBody() const
{
	return m_pRigidBody;
}

inline RigidBody* Entity::GetRigidBody()
{
	return m_pRigidBody;
}

inline const Light* Entity::GetLight() const
{
	return m_pLight;
}

inline Light* Entity::GetLight()
{
	return m_pLight;
}

inline const VolumeComponent* Entity::GetVolume() const
{
	return m_pVolume;
}

inline VolumeComponent* Entity::GetVolume()
{
	return m_pVolume;
}

inline const Vec3& Entity::GetPosition() const
{
	return m_position; 
}

inline void Entity::SetPosition(const Vec3& pos)
{ 
	m_position = pos;
	++m_transformId;
}

inline const Quaternion& Entity::GetOrientation() const
{ 
	return m_orientation; 
}

inline void Entity::SetOrientation(const Quaternion& q)
{
	m_orientation = q;
	// TODO: Extract rotation from quaternion
	++m_transformId;
}

inline const Rotation& Entity::GetRotation() const
{
	return m_rotation;
}

inline void Entity::SetRotation(const Rotation& r)
{
	m_rotation = r;
	m_orientation = Quaternion::FromRotation(r);
	++m_transformId;
}

inline const Vec3& Entity::GetScale() const
{ 
	return m_scale;
}

inline void Entity::SetScale(const Vec3& scale)
{ 
	m_scale = scale;
	++m_transformId;
}

inline const char* Entity::GetName() const
{
	return m_name;
}

inline void Entity::SetName(const char* name)
{
	strcpy_s(m_name, name);
}
