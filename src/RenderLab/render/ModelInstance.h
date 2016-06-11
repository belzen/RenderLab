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
typedef FreeList<ModelInstance, 1024> ModelInstanceFreeList;

class ModelInstance
{
public:
	static ModelInstance* Create(const char* modelAssetName);

	void Release();

	void PrepareDraw(const Matrix44& mtxWorld, bool transformChanged);

	const RdrDrawOp** GetDrawOps() const;
	uint GetNumDrawOps() const;

	float GetRadius() const;

private:
	friend ModelInstanceFreeList;
	ModelInstance() {}

	ModelData* m_pModelData;
	RdrDrawOp* m_pSubObjectDrawOps[ModelData::kMaxSubObjects];
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	RdrInputLayoutHandle m_hInputLayout;
};

inline const RdrDrawOp** ModelInstance::GetDrawOps() const
{
	return (const RdrDrawOp**)m_pSubObjectDrawOps;
}

inline uint ModelInstance::GetNumDrawOps() const
{
	return ModelData::kMaxSubObjects;
}

inline float ModelInstance::GetRadius() const
{
	return m_pModelData->GetRadius();
}