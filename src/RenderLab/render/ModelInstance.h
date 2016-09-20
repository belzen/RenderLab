#pragma once

#include "RdrResource.h"
#include "RdrShaders.h"
#include "ModelData.h"

struct RdrDrawOp;
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

	void PrepareDraw(const Matrix44& mtxWorld, bool transformChanged);

	const RdrDrawOp* const* GetDrawOps() const;
	uint GetNumDrawOps() const;

	float GetRadius() const;

private:
	friend ModelInstanceFreeList;
	ModelInstance() {}

	// Whether the model should allow GPU hardware instancing.
	bool CanInstance() const;

	ModelData* m_pModelData;
	RdrDrawOp* m_pSubObjectDrawOps[ModelData::kMaxSubObjects];
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	uint16 m_instancedDataId;
	RdrInputLayoutHandle m_hInputLayout;
};

inline const RdrDrawOp* const* ModelInstance::GetDrawOps() const
{
	return m_pSubObjectDrawOps;
}

inline uint ModelInstance::GetNumDrawOps() const
{
	return ModelData::kMaxSubObjects;
}

inline float ModelInstance::GetRadius() const
{
	return m_pModelData->GetRadius();
}