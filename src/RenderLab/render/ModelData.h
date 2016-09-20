#pragma once

#include "RdrGeometry.h"

struct RdrMaterial;
namespace AssetLib
{
	struct Model;
}

class ModelData;
typedef FreeList<ModelData, 1024> ModelDataFreeList;

class ModelData
{
public:
	static const uint kMaxSubObjects = 16;

	static ModelData* LoadFromFile(const char* modelName);

	struct SubObject
	{
		RdrGeoHandle hGeo;
		const RdrMaterial* pMaterial;
	};

	void Release();

	float GetRadius() const;

	const SubObject& GetSubObject(const uint index) const;

	uint GetNumSubObjects() const;

private:
	friend ModelDataFreeList;
	ModelData() {}

	AssetLib::Model* m_pBinData;

	SubObject m_subObjects[kMaxSubObjects];
	uint m_subObjectCount;

	float m_radius;

	char m_modelName[AssetLib::AssetDef::kMaxNameLen];
};

inline float ModelData::GetRadius() const
{
	return m_radius;
}

inline const ModelData::SubObject& ModelData::GetSubObject(const uint index) const
{
	return m_subObjects[index];
}

inline uint ModelData::GetNumSubObjects() const
{
	return m_subObjectCount;
}
