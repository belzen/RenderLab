#include "Precompiled.h"
#include "Physics.h"
#include "PhysX/PhysXSDK/Include/PxPhysics.h"
#include "PhysX/PhysXSDK/Include/PxPhysicsAPI.h"
#include "components/RigidBody.h"

using namespace physx;

namespace
{
	const PxVec3 kGravity(0.f, -9.81f, 0.f);

	struct
	{
		PxDefaultAllocator allocator;
		PxDefaultErrorCallback errorCallback;
		PxFoundation* pFoundation;
		PxCpuDispatcher* pCpuDispatcher;

		PxPhysics* pPhysics;
		PxScene* pScene;
		PxMaterial* pDefaultMaterial;

		float accumTime;
		bool simulationActive;
	} s_physics;

	inline PxVec3 toPxVec3(const Vec3& v)
	{
		return PxVec3(v.x, v.y, v.z);
	}

	inline Vec3 fromPxVec3(const PxVec3& v)
	{
		return Vec3(v.x, v.y, v.z);
	}

	inline PxQuat toPxQuat(const Quaternion& q)
	{
		return PxQuat(q.x, q.y, q.z, q.w);
	}

	inline PhysicsActor* createPhysicsActor(float density, const PxGeometry& geo, const Vec3& geoOffset)
	{
		PxTransform xform(PxIdentity);
		PxTransform geoXform(geoOffset.x, geoOffset.y, geoOffset.z);
		if (density > 0.f)
		{
			PxRigidDynamic* pActor = PxCreateDynamic(*s_physics.pPhysics, xform, geo, *s_physics.pDefaultMaterial, density, geoXform);
			pActor->setWakeCounter(500);
			pActor->setSleepThreshold(0);
			return pActor;
		}
		else
		{
			return PxCreateStatic(*s_physics.pPhysics, xform, geo, *s_physics.pDefaultMaterial, geoXform);
		}
	}

	void cmdPvdConnect(DebugCommandArg* args, int numArgs)
	{
		PxVisualDebuggerConnectionManager* pDebugger = s_physics.pPhysics->getPvdConnectionManager();
		PxVisualDebuggerExt::createConnection(pDebugger, "localhost", 5425, 5000,
			PxVisualDebuggerConnectionFlag::eDEBUG | PxVisualDebuggerConnectionFlag::ePROFILE | PxVisualDebuggerConnectionFlag::eMEMORY);
	}
}

void Physics::Init()
{
	// Debug commands
	DebugConsole::RegisterCommand("pvdConnect", cmdPvdConnect);

	// Setup physics
	s_physics.pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, s_physics.allocator, s_physics.errorCallback);

	PxTolerancesScale tolerancesScale;
	s_physics.pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *s_physics.pFoundation, tolerancesScale);

	PxSceneDesc sceneDesc(tolerancesScale);
	sceneDesc.gravity = kGravity;
	sceneDesc.cpuDispatcher = s_physics.pCpuDispatcher = PxDefaultCpuDispatcherCreate(2);
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;

	s_physics.pScene = s_physics.pPhysics->createScene(sceneDesc);

	s_physics.pDefaultMaterial = s_physics.pPhysics->createMaterial(0.5f, 0.5f, 0.1f);
}

void Physics::SetSimulationActive(bool active)
{
	s_physics.simulationActive = active;
	s_physics.accumTime = 0.f;
}

void Physics::Update()
{
	if (s_physics.simulationActive)
	{
		const float kStepTime = 1.f / 60.f;
		s_physics.accumTime += Time::FrameTime();

		while (s_physics.accumTime >= kStepTime)
		{
			s_physics.pScene->simulate(kStepTime);
			// Wait for simulation to complete
			s_physics.pScene->fetchResults(true);

			s_physics.accumTime -= kStepTime;
		}

		for (RigidBody& rRigidBody : RigidBody::GetFreeList())
		{
			rRigidBody.UpdatePostSimulation();
		}
	}
	else
	{
		for (RigidBody& rRigidBody : RigidBody::GetFreeList())
		{
			rRigidBody.UpdateNoSimulation();
		}
	}
}

PhysicsActor* Physics::CreatePlane()
{
	PhysicsActor* pActor = createPhysicsActor(0.f, PxPlaneGeometry(), Vec3::kZero);
	return pActor;
}

PhysicsActor* Physics::CreateBox(const Vec3& halfSize, float density, const Vec3& geoOffset)
{
	PhysicsActor* pActor = createPhysicsActor(density, PxBoxGeometry(halfSize.x, halfSize.y, halfSize.z), geoOffset);
	return pActor;
}

PhysicsActor* Physics::CreateSphere(const float radius, float density, const Vec3& geoOffset)
{
	PhysicsActor* pActor = createPhysicsActor(density, PxSphereGeometry(radius), geoOffset);
	return pActor;
}

void Physics::AddToScene(PhysicsActor* pActor, const Vec3& position, const Quaternion& orientation)
{
	PxTransform xform(position.x, position.y, position.z, toPxQuat(orientation));
	pActor->setGlobalPose(xform);
	s_physics.pScene->addActor(*pActor);
}

void Physics::SetActorUserData(PhysicsActor* pActor, void* pUserData)
{
	pActor->userData = pUserData;
}

void* Physics::GetActorUserData(PhysicsActor* pActor)
{
	return pActor->userData;
}

Vec3 Physics::GetActorPosition(PhysicsActor* pActor)
{
	const PxVec3& p = pActor->getGlobalPose().p;
	return Vec3(p.x, p.y, p.z);
}

Quaternion Physics::GetActorOrientation(PhysicsActor* pActor)
{
	const PxQuat& q = pActor->getGlobalPose().q;
	return Quaternion(q.x, q.y, q.z, q.w);
}

void Physics::SetActorTransform(PhysicsActor* pActor, const Vec3& position, const Quaternion& orientation)
{
	PxTransform xform(position.x, position.y, position.z, toPxQuat(orientation));
	pActor->setGlobalPose(xform);
}

void Physics::SetActorVelocities(PhysicsActor* pActor, const Vec3& linearVelocity, const Vec3& angularVelocity)
{
	PxRigidDynamic* pDynamicActor = pActor->isRigidDynamic();
	if (pDynamicActor)
	{
		pDynamicActor->setLinearVelocity(toPxVec3(linearVelocity));
		pDynamicActor->setAngularVelocity(toPxVec3(angularVelocity));
	}
}

void Physics::SetActorLinearVelocity(PhysicsActor* pActor, const Vec3& linearVelocity)
{
	PxRigidDynamic* pDynamicActor = pActor->isRigidDynamic();
	if (pDynamicActor)
	{
		pDynamicActor->setLinearVelocity(toPxVec3(linearVelocity));
	}
}

void Physics::SetActorAngularVelocity(PhysicsActor* pActor, const Vec3& angularVelocity)
{
	PxRigidDynamic* pDynamicActor = pActor->isRigidDynamic();
	if (pDynamicActor)
	{
		pDynamicActor->setAngularVelocity(toPxVec3(angularVelocity));
	}
}

void Physics::DestroyActor(PhysicsActor* pActor)
{
	s_physics.pScene->removeActor(*pActor);
	pActor->release();
}

bool Physics::Raycast(const Vec3& position, const Vec3& direction, const float distance, RaycastResult* pResult)
{
	Physics::RaycastResult results;
	const PxHitFlags hitFlags = PxHitFlag::ePOSITION | PxHitFlag::eNORMAL | PxHitFlag::eDISTANCE;
	PxRaycastBuffer hitBuffer;

	bool hit = s_physics.pScene->raycast(toPxVec3(position), toPxVec3(direction), distance, hitBuffer, hitFlags);
	if (hit && pResult)
	{
		pResult->pActor = hitBuffer.block.actor;
		pResult->position = fromPxVec3(hitBuffer.block.position);
		pResult->normal = fromPxVec3(hitBuffer.block.normal);
		pResult->distance = distance;
	}

	return hit;
}