#include "Precompiled.h"
#include "RigidBody.h"
#include "Physics.h"
#include "Entity.h"

namespace
{
	RigidBodyFreeList s_rigidBodyFreeList;
}

RigidBodyFreeList& RigidBody::GetFreeList()
{
	return s_rigidBodyFreeList;
}

RigidBody* RigidBody::CreatePlane()
{
	RigidBody* pRigidBody = s_rigidBodyFreeList.allocSafe();
	pRigidBody->m_pActor = Physics::CreatePlane();
	return pRigidBody;
}

RigidBody* RigidBody::CreateBox(const Vec3& halfSize, const float density, const Vec3& offset)
{
	RigidBody* pRigidBody = s_rigidBodyFreeList.allocSafe();
	pRigidBody->m_pActor = Physics::CreateBox(halfSize, density, offset);
	return pRigidBody;
}

RigidBody* RigidBody::CreateSphere(const float radius, const float density, const Vec3& offset)
{
	RigidBody* pRigidBody = s_rigidBodyFreeList.allocSafe();
	pRigidBody->m_pActor = Physics::CreateSphere(radius, density, offset);
	return pRigidBody;
}

void RigidBody::OnAttached(Entity* pEntity)
{
	m_pEntity = pEntity;
	Physics::SetActorUserData(m_pActor, pEntity);
	Physics::AddToScene(m_pActor, pEntity->GetPosition(), pEntity->GetOrientation());
}

void RigidBody::OnDetached(Entity* pEntity)
{
	m_pEntity = nullptr;
	Physics::SetActorUserData(m_pActor, nullptr);
}

void RigidBody::Release()
{
	Physics::DestroyActor(m_pActor);
	s_rigidBodyFreeList.release(this);
}

Vec3 RigidBody::GetPosition() const
{
	return Physics::GetActorPosition(m_pActor);
}

Quaternion RigidBody::GetOrientation() const
{
	return Physics::GetActorOrientation(m_pActor);
}

void RigidBody::UpdatePostSimulation()
{
	// Physics simulation is active, push the actor's transform to the entity.
	m_pEntity->SetPosition(GetPosition());
	m_pEntity->SetOrientation(GetOrientation());
}

void RigidBody::UpdateNoSimulation()
{
	// Simulation is disabled, pull the transform from the parent entity.
	Physics::SetActorTransform(m_pActor, m_pEntity->GetPosition(), m_pEntity->GetOrientation());
}
