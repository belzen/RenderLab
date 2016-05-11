#include "Precompiled.h"
#include "RdrGeoSystem.h"
#include "RdrContext.h"
#include "RdrScratchMem.h"
#include "AssetLib/BinFile.h"
#include "AssetLib/ModelAsset.h"
#include <fstream>

namespace
{
	struct GeoCmdUpdate
	{
		RdrGeoHandle hGeo;
		const void* pVertData;
		const uint16* pIndexData;
		RdrGeoInfo info;
	};

	struct GeoCmdRelease
	{
		RdrGeoHandle hGeo;
	};

	struct GeoFrameState
	{
		std::vector<GeoCmdUpdate> updates;
		std::vector<GeoCmdRelease> releases;
	};

	struct
	{
		RdrGeoList geos;

		GeoFrameState states[2];
		uint          queueState;
	} s_geoSystem;

	inline GeoFrameState& getQueueState()
	{
		return s_geoSystem.states[s_geoSystem.queueState];
	}
}

const RdrGeometry* RdrGeoSystem::GetGeo(const RdrGeoHandle hGeo)
{
	return s_geoSystem.geos.get(hGeo);
}

void RdrGeoSystem::ReleaseGeo(const RdrGeoHandle hGeo)
{
	GeoCmdRelease cmd;
	cmd.hGeo = hGeo;

	getQueueState().releases.push_back(cmd);
}

RdrGeoHandle RdrGeoSystem::CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& boundsMin, const Vec3& boundsMax)
{
	RdrGeometry* pGeo = s_geoSystem.geos.allocSafe();

	GeoCmdUpdate cmd;
	cmd.hGeo = s_geoSystem.geos.getId(pGeo);
	cmd.pVertData = pVertData;
	cmd.pIndexData = pIndexData;
	cmd.info.vertStride = vertStride;
	cmd.info.numVerts = numVerts;
	cmd.info.numIndices = numIndices;
	cmd.info.boundsMin = boundsMin;
	cmd.info.boundsMax = boundsMax;

	getQueueState().updates.push_back(cmd);

	return cmd.hGeo;
}

void RdrGeoSystem::FlipState()
{
	s_geoSystem.queueState = !s_geoSystem.queueState;
}

void RdrGeoSystem::ProcessCommands(RdrContext* pRdrContext)
{
	GeoFrameState& state = s_geoSystem.states[!s_geoSystem.queueState];
	uint numCmds;

	// Frees
	s_geoSystem.geos.AcquireLock();
	{
		numCmds = (uint)state.releases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			GeoCmdRelease& cmd = state.releases[i];
			RdrGeometry* pGeo = s_geoSystem.geos.get(cmd.hGeo);

			pRdrContext->ReleaseVertexBuffer(pGeo->vertexBuffer);
			pGeo->vertexBuffer.pBuffer = nullptr;

			pRdrContext->ReleaseIndexBuffer(pGeo->indexBuffer);
			pGeo->indexBuffer.pBuffer = nullptr;

			s_geoSystem.geos.releaseId(cmd.hGeo);
		}
	}
	s_geoSystem.geos.ReleaseLock();

	// Updates
	numCmds = (uint)state.updates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		GeoCmdUpdate& cmd = state.updates[i];
		RdrGeometry* pGeo = s_geoSystem.geos.get(cmd.hGeo);
		if (!pGeo->vertexBuffer.pBuffer)
		{
			// Creating
			pGeo->geoInfo = cmd.info;
			pGeo->vertexBuffer = pRdrContext->CreateVertexBuffer(cmd.pVertData, cmd.info.vertStride * cmd.info.numVerts);
			if (cmd.pIndexData)
			{
				pGeo->indexBuffer = pRdrContext->CreateIndexBuffer(cmd.pIndexData, sizeof(uint16) * cmd.info.numIndices);
			}
		}
		else
		{
			// Updating
			assert(false); //todo
		}
	}

	state.updates.clear();
	state.releases.clear();
}
