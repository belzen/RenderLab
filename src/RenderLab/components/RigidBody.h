#pragma once

#include "ObjectComponent.h"
#include "FreeList.h"
#include "PhysicsTypes.h"

class RigidBody;
typedef FreeList<RigidBody, 6 * 1024> RigidBodyFreeList;

class RigidBody : public ObjectComponent
{
public:
	static RigidBody* CreatePlane();
	static RigidBody* CreateBox(const Vec3& halfSize, const float density, const Vec3& offset);
	static RigidBody* CreateSphere(const float radius, const float density, const Vec3& offset);

	void OnAttached(WorldObject* pObject);
	void OnDetached(WorldObject* pObject);

	void Release();

	Vec3 GetPosition() const;
	Quaternion GetOrientation() const;

private:
	friend RigidBodyFreeList;
	RigidBody() {}

private:
	PhysicsActor* m_pActor;
};
