#pragma once
#include "PhysicsTypes.h"

namespace Physics
{
	void Init();
	void Update();

	//////////////////////////////////////////////////////////////////////////
	// Physics actors
	PhysicsActor* CreatePlane(PhysicsGroup category);
	PhysicsActor* CreateBox(PhysicsGroup category, const Vec3& halfSize, float density, const Vec3& offset);
	PhysicsActor* CreateSphere(PhysicsGroup category, const float radius, float density, const Vec3& offset);

	void AddToScene(PhysicsActor* pActor, const Vec3& position, const Quaternion& orientation);
	void RemoveFromScene(PhysicsActor* pActor);

	void SetActorUserData(PhysicsActor* pActor, void* pUserData);
	void* GetActorUserData(PhysicsActor* pActor);

	Vec3 GetActorPosition(PhysicsActor* pActor);
	Quaternion GetActorOrientation(PhysicsActor* pActor);

	void SetActorTransform(PhysicsActor* pActor, const Vec3& position, const Quaternion& orientation);

	void SetActorVelocities(PhysicsActor* pActor, const Vec3& linearVelocity, const Vec3& angularVelocity);
	void SetActorLinearVelocity(PhysicsActor* pActor, const Vec3& linearVelocity);
	void SetActorAngularVelocity(PhysicsActor* pActor, const Vec3& angularVelocity);

	void DestroyActor(PhysicsActor* pActor);

	//////////////////////////////////////////////////////////////////////////
	// Scene queries
	struct RaycastResult
	{
		PhysicsActor* pActor;
		Vec3 position;
		Vec3 normal;
		float distance;
	};
	bool Raycast(PhysicsGroupFlags groupMask, const Vec3& position, const Vec3& direction, const float distance, RaycastResult* pResult);
}
