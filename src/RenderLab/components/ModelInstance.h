#pragma once

#include "Renderable.h"
#include "Entity.h"
#include "AssetLib\AssetLibForwardDecl.h"
#include "render/RdrResource.h"
#include "render/RdrShaders.h"
#include "render/ModelData.h"
#include "render/RdrDrawOp.h"

class RdrAction;

class ModelInstance : public Renderable
{
public:
	static ModelInstance* Create(IComponentAllocator* pAllocator, const CachedString& modelAssetName, const AssetLib::MaterialSwap* aMaterialSwaps, uint numMaterialSwaps);

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
	ModelInstance() {}
	ModelInstance(const ModelInstance&);
	virtual ~ModelInstance() {}

	// Whether the model should allow GPU hardware instancing.
	bool CanInstance() const;

private:
	ModelData* m_pModelData;
	const RdrMaterial* m_pMaterials[ModelData::kMaxSubObjects];

	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	int m_lastTransformId;

	RdrInputLayoutHandle m_hInputLayout;
	uint16 m_instancedDataId;
};

inline float ModelInstance::GetRadius() const
{
	return m_pModelData->GetRadius() * Vec3MaxComponent(m_pEntity->GetScale());
}

inline const ModelData* ModelInstance::GetModelData() const
{
	return m_pModelData;
}