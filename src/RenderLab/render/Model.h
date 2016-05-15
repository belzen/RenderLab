#pragma once

#include "RdrDrawOp.h"
#include "RdrGeoSystem.h"

class Renderer;
namespace AssetLib
{
	struct Model;
}

class Model;
typedef FreeList<Model, 1024> ModelFreeList;

class Model
{
public:
	static Model* LoadFromFile(const char* modelName);

	struct SubObject
	{
		RdrGeoHandle hGeo;
		const RdrMaterial* pMaterial;
	};

	void Release();

	void QueueDraw(Renderer& rRenderer, const Matrix44& worldMat);

	float GetRadius() const;

	const SubObject& GetSubObject(const uint index) const;

	uint GetNumSubObjects() const;

private:
	friend ModelFreeList;
	Model() {}

	AssetLib::Model* m_pBinData;

	SubObject m_subObjects[16];
	uint m_subObjectCount;

	float m_radius;

	char m_modelName[AssetLib::AssetDef::kMaxNameLen];
};

inline float Model::GetRadius() const 
{
	return m_radius;
}

inline const Model::SubObject& Model::GetSubObject(const uint index) const
{
	return m_subObjects[index];
}

inline uint Model::GetNumSubObjects() const
{
	return m_subObjectCount;
}
