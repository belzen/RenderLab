#pragma once

namespace physx
{
	class PxRigidActor;
}

typedef physx::PxRigidActor PhysicsActor;

enum class PhysicsGroup
{
	kDefault,
};

enum class PhysicsGroupFlags
{
	kDefault = (1 << (int)PhysicsGroup::kDefault),

	kAll = kDefault
};
ENUM_FLAGS(PhysicsGroupFlags);