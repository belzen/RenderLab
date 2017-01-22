#include "Precompiled.h"
#include "Game.h"
#include "Physics.h"
#include "Entity.h"
#include "components/RigidBody.h"

namespace
{
	// Entity state that can be cached and restored during Play() and Stop().
	// Probably going to need cache/serialize methods on EntityComponents in the future,
	// but position/rotation is all that's needed for now.
	struct EntityCachedState
	{
		Vec3 position;
		Rotation rotation;
		uint16 playId; // Which playId this state is valid for.
	};

	struct  
	{
		EntityCachedState entityCachedData[EntityFreeList::kMaxEntries];
		uint16 playId; // Unique ID for current run of the game.  Used to validate EntityCachedState when restoring state.
		bool isActive;
	} s_gameData;
}

void Game::Play()
{
	if (s_gameData.isActive)
	{
		// Reset time scale in case the game was paused.
		Time::SetTimeScale(1.f);
		return;
	}

	s_gameData.playId++;
	s_gameData.isActive = true;
	Time::SetTimeScale(1.f);

	// Cache entity state to restore on Stop().
	EntityFreeList& rEntityList = Entity::GetFreeList();
	for (const Entity& rEntity : rEntityList)
	{
		uint16 id = rEntityList.getId(&rEntity);
		EntityCachedState& rState = s_gameData.entityCachedData[id];

		rState.position = rEntity.GetPosition();
		rState.rotation = rEntity.GetRotation();
		rState.playId = s_gameData.playId;
	}
}

void Game::Pause()
{
	if (!s_gameData.isActive)
		return;

	Time::SetTimeScale(0.f);
}

void Game::Stop()
{
	if (!s_gameData.isActive)
		return;

	s_gameData.isActive = false;
	Time::SetTimeScale(1.f);

	// Restore entity state.
	EntityFreeList& rEntityList = Entity::GetFreeList();
	for (Entity& rEntity : rEntityList)
	{
		uint16 id = rEntityList.getId(&rEntity);
		EntityCachedState& rState = s_gameData.entityCachedData[id];

		if (rState.playId == s_gameData.playId)
		{
			rEntity.SetPosition(rState.position);
			rEntity.SetRotation(rState.rotation);
		}

		RigidBody* pRigidBody = rEntity.GetRigidBody();
		if (pRigidBody)
		{
			pRigidBody->SetVelocities(Vec3::kZero, Vec3::kZero);
		}
	}
}

bool Game::IsActive()
{
	return s_gameData.isActive;
}
