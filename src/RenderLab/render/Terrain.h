#pragma once

#include "AssetLib/SceneAsset.h"
#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "RdrMaterial.h"
#include "RdrDrawOp.h"

class RdrAction;
class Camera;

class Terrain
{
public:
	Terrain();

	void Init(const AssetLib::Terrain& rTerrainAsset);
	RdrDrawOpSet BuildDrawOps(RdrAction* pAction);

private:
	AssetLib::Terrain m_srcData;
	UVec2 m_gridSize;
	UVec2 m_heightmapSize;

	RdrMaterial m_material;

	RdrGeoHandle m_hGeo;
	RdrResourceHandle m_hInstanceData;
	RdrConstantBufferHandle m_hDomainConstants;

	bool m_initialized;
};
