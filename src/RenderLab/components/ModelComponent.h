#pragma once

#include "Renderable.h"
#include "Entity.h"
#include "AssetLib\AssetLibForwardDecl.h"
#include "render/RdrResource.h"
#include "render/RdrShaders.h"
#include "render/ModelData.h"

class ModelComponent : public Renderable
{
public:
	static ModelComponent* Create(IComponentAllocator* pAllocator, const CachedString& modelAssetName, const AssetLib::MaterialSwap* aMaterialSwaps, uint numMaterialSwaps);

public:
	void Release();

	void OnAttached(Entity* pEntity);
	void OnDetached(Entity* pEntity);

	RdrDrawOpSet BuildDrawOps(RdrAction* pAction);

	float GetRadius() const;

	void SetModelData(const CachedString& modelAssetName, const AssetLib::MaterialSwap* aMaterialSwaps, uint numMaterialSwaps);
	const ModelData* GetModelData() const;

private:
	FRIEND_FREELIST;
	ModelComponent() {}
	ModelComponent(const ModelComponent&);
	virtual ~ModelComponent() {}

	// Whether the model should allow GPU hardware instancing.
	bool CanInstance() const;

private:
	ModelData* m_pModelData;
	const RdrMaterial* m_pMaterials[ModelData::kMaxSubObjects];

	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	int m_lastTransformId;

	uint16 m_instancedDataId;
};

inline float ModelComponent::GetRadius() const
{
	return m_pModelData->GetRadius() * Vec3MaxComponent(m_pEntity->GetScale());
}

inline const ModelData* ModelComponent::GetModelData() const
{
	return m_pModelData;
}