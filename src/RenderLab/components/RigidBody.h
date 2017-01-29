#pragma once

#include "EntityComponent.h"
#include "FreeList.h"
#include "PhysicsTypes.h"

class RigidBody : public EntityComponent
{
public:
	static RigidBody* CreatePlane(IComponentAllocator* pAllocator, PhysicsGroup category);
	static RigidBody* CreateBox(IComponentAllocator* pAllocator, PhysicsGroup category, const Vec3& halfSize, const float density, const Vec3& offset);
	static RigidBody* CreateSphere(IComponentAllocator* pAllocator, PhysicsGroup category, const float radius, const float density, const Vec3& offset);

public:
	void OnAttached(Entity* pEntity);
	void OnDetached(Entity* pEntity);

	void Release();

	// Enable/disable collision and simulation
	void SetEnabled(bool enabled);

	Vec3 GetPosition() const;
	Quaternion GetOrientation() const;

	void SetVelocities(const Vec3& linearVelocity, const Vec3& angularVelocity);
	void SetLinearVelocity(const Vec3& linearVelocity);
	void SetAngularVelocity(const Vec3& angularVelocity);

	// Update after physics simulation has been processed.
	void UpdatePostSimulation();

	// Update called every frame when physics simulation is disabled (editor mode).
	void UpdateNoSimulation();

private:
	FRIEND_FREELIST;
	RigidBody() {}
	RigidBody(const RigidBody&);
	virtual ~RigidBody() {}

private:
	PhysicsActor* m_pActor;
	bool m_enabled;
};
