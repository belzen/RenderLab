#pragma once

#include "AssetLib\AssetLibForwardDecl.h"
#include "RdrResource.h"
#include "RdrShaders.h"
#include "ModelData.h"

class RdrDrawBuckets;

class ModelInstance;
typedef FreeList<ModelInstance, 6 * 1024> ModelInstanceFreeList;

class ModelInstance
{
public:
	static ModelInstance* Create(const CachedString& modelAssetName, const AssetLib::MaterialSwap* aMaterialSwaps, uint numMaterialSwaps);

	void Release();

	void QueueDraw(RdrDrawBuckets* pDrawBuckets, const Matrix44& mtxWorld, bool transformChanged);

	float GetRadius() const;

	const ModelData* GetModelData() const;

private:
	friend ModelInstanceFreeList;
	ModelInstance() {}

	// Whether the model should allow GPU hardware instancing.
	bool CanInstance() const;

private:
	ModelData* m_pModelData;
	const RdrMaterial* m_pMaterials[ModelData::kMaxSubObjects];

	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;

	RdrInputLayoutHandle m_hInputLayout;
	uint16 m_instancedDataId;
};

inline float ModelInstance::GetRadius() const
{
	return m_pModelData->GetRadius();
}

inline const ModelData* ModelInstance::GetModelData() const
{
	return m_pModelData;
}