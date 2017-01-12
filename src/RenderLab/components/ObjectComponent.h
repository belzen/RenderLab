#pragma once

class WorldObject;

class ObjectComponent
{
public:
	virtual void OnAttached(WorldObject* pObject) = 0;
	virtual void OnDetached(WorldObject* pObject) = 0;

	WorldObject* GetParent();
	const WorldObject* GetParent() const;

protected:
	WorldObject* m_pParentObject;
};

//////////////////////////////////////////////////////////////////////////
inline WorldObject* ObjectComponent::GetParent()
{
	return m_pParentObject;
}

inline const WorldObject* ObjectComponent::GetParent() const
{
	return m_pParentObject;
}