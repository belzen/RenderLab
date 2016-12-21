#pragma once

#include "FreeList.h"
#include "PhysicsTypes.h"

class RigidBody;
typedef FreeList<RigidBody, 6 * 1024> RigidBodyFreeList;

class RigidBody
{
public:
	static RigidBody* CreatePlane();
	static RigidBody* CreateBox(const Vec3& halfSize, const float density, const Vec3& offset);
	static RigidBody* CreateSphere(const float radius, const float density, const Vec3& offset);

	void AddToScene(const Vec3& position, const Quaternion& orientation);

	void Release();

	Vec3 GetPosition() const;
	Quaternion GetOrientation() const;

private:
	friend RigidBodyFreeList;
	RigidBody() {}

private:
	PhysicsActor* m_pActor;
};
