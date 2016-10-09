#pragma once

#include "RdrResource.h"
#include "RdrShaders.h"
#include "ModelData.h"

class RdrDrawBuckets;
namespace AssetLib
{
	struct Model;
}

class ModelInstance;
typedef FreeList<ModelInstance, 6 * 1024> ModelInstanceFreeList;

class ModelInstance
{
public:
	static ModelInstance* Create(const char* modelAssetName);

	void Release();

	void QueueDraw(RdrDrawBuckets* pDrawBuckets, const Matrix44& mtxWorld, bool transformChanged);

	float GetRadius() const;

	const ModelData* GetModelData() const;

private:
	friend ModelInstanceFreeList;
	ModelInstance() {}

	// Whether the model should allow GPU hardware instancing.
	bool CanInstance() const;

	ModelData* m_pModelData;
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	uint16 m_instancedDataId;
	RdrInputLayoutHandle m_hInputLayout;
};

inline float ModelInstance::GetRadius() const
{
	return m_pModelData->GetRadius();
}

inline const ModelData* ModelInstance::GetModelData() const
{
	return m_pModelData;
}