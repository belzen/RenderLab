#pragma once

class Entity;
class IComponentAllocator;

class EntityComponent
{
public:
	virtual void Release() = 0;

	virtual void OnAttached(Entity* pEntity) = 0;
	virtual void OnDetached(Entity* pEntity) = 0;

	Entity* GetEntity();
	const Entity* GetEntity() const;

protected:
	friend IComponentAllocator;
	virtual ~EntityComponent() {}

	// Entity this is attached to.
	Entity* m_pEntity;

	// Allocator this was created from.
	IComponentAllocator* m_pAllocator;
};

//////////////////////////////////////////////////////////////////////////
inline Entity* EntityComponent::GetEntity()
{
	return m_pEntity;
}

inline const Entity* EntityComponent::GetEntity() const
{
	return m_pEntity;
}