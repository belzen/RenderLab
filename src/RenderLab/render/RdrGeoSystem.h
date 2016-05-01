#pragma once

#include "RdrGeometry.h"

class RdrContext;

namespace RdrGeoSystem
{
	RdrGeoHandle CreateGeoFromFile(const char* filename, RdrGeoInfo* pOutInfo);

	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size);

	const RdrGeometry* GetGeo(const RdrGeoHandle hGeo);

	void ReleaseGeo(const RdrGeoHandle hGeo);

	void FlipState();

	void ProcessCommands(RdrContext* pRdrContext);
}