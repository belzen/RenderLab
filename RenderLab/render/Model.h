#pragma once

#include "RdrDrawOp.h"
#include "RdrGeoSystem.h"

class Renderer;

class Model;
typedef FreeList<Model, 1024> ModelFreeList;

class Model
{
public:
	static Model* Create(RdrGeoHandle hGeo, const RdrMaterial* pMaterial);

	void Release();

	void QueueDraw(Renderer& rRenderer, const Matrix44& worldMat);

	float GetRadius() const;

private:
	friend ModelFreeList;
	Model() {}

	RdrGeoHandle m_hGeo;
	const RdrMaterial* m_pMaterial;
};

inline float Model::GetRadius() const 
{
	return RdrGeoSystem::GetGeo(m_hGeo)->geoInfo.radius;
}