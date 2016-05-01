#pragma once

#include "RdrDrawOp.h"
#include "RdrGeoSystem.h"

class Renderer;

class Model;
typedef FreeList<Model, 1024> ModelFreeList;

class Model
{
public:
	static Model* LoadFromFile(const char* modelName);

	void Release();

	void QueueDraw(Renderer& rRenderer, const Matrix44& worldMat);

	float GetRadius() const;

private:
	friend ModelFreeList;
	Model() {}

	struct SubObject
	{
		RdrGeoHandle hGeo;
		const RdrMaterial* pMaterial;
	};

	SubObject m_subObjects[16];
	int m_subObjectCount;

	float m_radius;

	char m_modelName[AssetDef::kMaxNameLen];
};

inline float Model::GetRadius() const 
{
	return m_radius;
}