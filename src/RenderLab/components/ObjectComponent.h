#pragma once

class WorldObject;

class ObjectComponent
{
public:
	virtual void OnAttached(WorldObject* pObject) = 0;
	virtual void OnDetached(WorldObject* pObject) = 0;
};