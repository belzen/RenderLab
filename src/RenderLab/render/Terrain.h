#pragma once

#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "RdrMaterial.h"

class RdrDrawBuckets;
class Camera;

class Terrain
{
public:
	Terrain() {}

	void Init();
	void QueueDraw(RdrDrawBuckets* pDrawBuckets, const Camera& rCamera);

private:
	RdrMaterial m_material;
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	RdrInputLayoutHandle m_hInputLayout;
	RdrGeoHandle m_hGeo;
	RdrResourceHandle m_hInstanceData;
};
