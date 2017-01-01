#pragma once
#include "PhysicsTypes.h"

namespace physx
{
	class PxRigidActor;
}

typedef physx::PxRigidActor PhysicsActor;

namespace Physics
{
	typedef uint ActorId;

	void Init();

	void Update();

	PhysicsActor* CreatePlane();
	PhysicsActor* CreateBox(const Vec3& halfSize, float density, const Vec3& offset);
	PhysicsActor* CreateSphere(const float radius, float density, const Vec3& offset);

	void AddToScene(PhysicsActor* pActor, const Vec3& position, const Quaternion& orientation);

	Vec3 GetActorPosition(PhysicsActor* pActor);
	Quaternion GetActorOrientation(PhysicsActor* pActor);

	void DestroyActor(PhysicsActor* pActor);

	struct RaycastResult
	{
		PhysicsActor* pActor;
		Vec3 position;
		Vec3 normal;
		float distance;
	};
	bool Raycast(const Vec3& position, const Vec3& direction, const float distance, RaycastResult* pResult);
}
