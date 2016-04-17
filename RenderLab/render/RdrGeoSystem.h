#pragma once

#include <map>
#include <vector>
#include "RdrGeometry.h"
#include "FreeList.h"

class RdrContext;

typedef std::map<std::string, RdrGeoHandle> RdrGeoHandleMap;

class RdrGeoSystem
{
public:
	void Init(RdrContext* pRdrContext);

	RdrGeoHandle CreateGeoFromFile(const char* filename, RdrGeoInfo* pOutInfo);

	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size);

	const RdrGeometry* GetGeo(const RdrGeoHandle hGeo);

	void ReleaseGeo(const RdrGeoHandle hGeo);

	void FlipState();
	void ProcessCommands();

private:
	struct CmdUpdate
	{
		RdrGeoHandle hGeo;
		const void* pVertData;
		const uint16* pIndexData;
		RdrGeoInfo info;
	};

	struct CmdRelease
	{
		RdrGeoHandle hGeo;
	};

	struct FrameState
	{
		std::vector<CmdUpdate> updates;
		std::vector<CmdRelease> releases;
	};

private:
	RdrContext* m_pRdrContext;

	RdrGeoHandleMap     m_geoCache;
	RdrGeoList          m_geos;

	FrameState m_states[2];
	uint       m_queueState;
};