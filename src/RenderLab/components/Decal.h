#pragma once

#include "Renderable.h"
#include "Entity.h"
#include "AssetLib\AssetLibForwardDecl.h"
#include "render/RdrResource.h"
#include "render/RdrShaders.h"
#include "render/ModelData.h"

class Decal : public Renderable
{
public:
	static Decal* Create(IComponentAllocator* pAllocator, const CachedString& textureName);

public:
	void Release();

	void OnAttached(Entity* pEntity);
	void OnDetached(Entity* pEntity);

	RdrDrawOpSet BuildDrawOps(RdrAction* pAction);

	float GetRadius() const;

	void SetTexture(const CachedString& textureName);
	const CachedString& GetTexture() const;

private:
	FRIEND_FREELIST;
	Decal() {}
	Decal(const Decal&);
	virtual ~Decal() {}

private:
	RdrMaterial m_material;

	CachedString m_textureName;
	RdrResourceHandle m_hTexture;

	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	RdrConstantBufferHandle m_hPsMaterialBuffer;
	int m_lastTransformId;

	RdrInputLayoutHandle m_hInputLayout;
};

inline float Decal::GetRadius() const
{
	return Vec3MaxComponent(m_pEntity->GetScale());
}
