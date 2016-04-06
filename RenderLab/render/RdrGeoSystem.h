#pragma once

#include <map>
#include <vector>
#include "RdrGeometry.h"
#include "FreeList.h"

class RdrContext;

typedef std::map<std::string, RdrGeoHandle> RdrGeoHandleMap;
typedef FreeList<RdrGeometry, 1024> RdrGeoList;
typedef FreeList<RdrVertexBuffer, 1024> RdrVertexBufferList;
typedef FreeList<RdrIndexBuffer, 1024> RdrIndexBufferList;

class RdrGeoSystem
{
public:
	void Init(RdrContext* pRdrContext);

	RdrGeoHandle CreateGeoFromFile(const char* filename, RdrGeoInfo* pOutInfo);

	RdrGeoHandle CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size);

	const RdrGeometry* GetGeo(const RdrGeoHandle hGeo);

	const RdrVertexBuffer* GetVertexBuffer(const RdrVertexBufferHandle hBuffer);
	const RdrIndexBuffer* GetIndexBuffer(const RdrIndexBufferHandle hBuffer);

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
	RdrVertexBufferList m_vertexBuffers;
	RdrIndexBufferList  m_indexBuffers;

	FrameState m_states[2];
	uint       m_queueState;
};