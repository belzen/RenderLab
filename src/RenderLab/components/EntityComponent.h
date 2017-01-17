#pragma once

class Entity;

class EntityComponent
{
public:
	virtual void Release() = 0;

	virtual void OnAttached(Entity* pEntity) = 0;
	virtual void OnDetached(Entity* pEntity) = 0;

	Entity* GetEntity();
	const Entity* GetEntity() const;

protected:
	virtual ~EntityComponent() {}

	Entity* m_pEntity;
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