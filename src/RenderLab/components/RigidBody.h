#pragma once

#include "EntityComponent.h"
#include "FreeList.h"
#include "PhysicsTypes.h"

class RigidBody;
typedef FreeList<RigidBody, 6 * 1024> RigidBodyFreeList;

class RigidBody : public EntityComponent
{
public:
	static RigidBodyFreeList& GetFreeList();
	static RigidBody* CreatePlane();
	static RigidBody* CreateBox(const Vec3& halfSize, const float density, const Vec3& offset);
	static RigidBody* CreateSphere(const float radius, const float density, const Vec3& offset);

public:
	void OnAttached(Entity* pEntity);
	void OnDetached(Entity* pEntity);

	void Release();

	Vec3 GetPosition() const;
	Quaternion GetOrientation() const;

	// Update after physics simulation has been processed.
	void UpdatePostSimulation();

	// Update called every frame when physics simulation is disabled (editor mode).
	void UpdateNoSimulation();

private:
	friend RigidBodyFreeList;
	RigidBody() {}
	RigidBody(const RigidBody&);
	virtual ~RigidBody() {}

private:
	PhysicsActor* m_pActor;
};
