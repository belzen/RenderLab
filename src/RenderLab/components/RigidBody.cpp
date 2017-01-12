#include "Precompiled.h"
#include "RigidBody.h"
#include "Physics.h"
#include "WorldObject.h"

namespace
{
	RigidBodyFreeList s_rigidBodies;
}

RigidBody* RigidBody::CreatePlane()
{
	RigidBody* pRigidBody = s_rigidBodies.allocSafe();
	pRigidBody->m_pActor = Physics::CreatePlane();
	return pRigidBody;
}

RigidBody* RigidBody::CreateBox(const Vec3& halfSize, const float density, const Vec3& offset)
{
	RigidBody* pRigidBody = s_rigidBodies.allocSafe();
	pRigidBody->m_pActor = Physics::CreateBox(halfSize, density, offset);
	return pRigidBody;
}

RigidBody* RigidBody::CreateSphere(const float radius, const float density, const Vec3& offset)
{
	RigidBody* pRigidBody = s_rigidBodies.allocSafe();
	pRigidBody->m_pActor = Physics::CreateSphere(radius, density, offset);
	return pRigidBody;
}

void RigidBody::OnAttached(WorldObject* pObject)
{
	m_pParentObject = pObject;
	Physics::SetActorUserData(m_pActor, pObject);
	Physics::AddToScene(m_pActor, pObject->GetPosition(), pObject->GetOrientation());
}

void RigidBody::OnDetached(WorldObject* pObject)
{
	m_pParentObject = nullptr;
	Physics::SetActorUserData(m_pActor, nullptr);
}

void RigidBody::Release()
{
	Physics::DestroyActor(m_pActor);
}

Vec3 RigidBody::GetPosition() const
{
	return Physics::GetActorPosition(m_pActor);
}

Quaternion RigidBody::GetOrientation() const
{
	return Physics::GetActorOrientation(m_pActor);
}
