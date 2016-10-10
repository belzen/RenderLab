#pragma once

#include "AssetLib\AssetLibForwardDecl.h"
#include "AssetLib\AssetDef.h"
#include "RdrGeometry.h"

struct RdrMaterial;

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

	const char* GetName() const;

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

inline const char* ModelData::GetName() const
{
	return m_modelName;
}
