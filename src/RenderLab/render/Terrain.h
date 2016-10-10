#pragma once

#include "AssetLib/SceneAsset.h"
#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "RdrMaterial.h"
#include "RdrTessellationMaterial.h"

class RdrDrawBuckets;
class Camera;

class Terrain
{
public:
	Terrain();

	void Init(const AssetLib::Terrain& rTerrainAsset);
	void QueueDraw(RdrDrawBuckets* pDrawBuckets, const Camera& rCamera);

private:
	AssetLib::Terrain m_srcData;
	UVec2 m_gridSize;
	UVec2 m_heightmapSize;

	RdrMaterial m_material;
	RdrTessellationMaterial m_tessMaterial;

	RdrInputLayoutHandle m_hInputLayout;
	RdrGeoHandle m_hGeo;
	RdrResourceHandle m_hInstanceData;

	bool m_initialized;
};
