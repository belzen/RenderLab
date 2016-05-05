#include "Precompiled.h"
#include "RdrGeoSystem.h"
#include "RdrContext.h"
#include "RdrTransientMem.h"
#include "AssetLib/BinFile.h"
#include "AssetLib/GeoAsset.h"
#include <fstream>

namespace
{
	typedef std::map<std::string, RdrGeoHandle> RdrGeoHandleMap;

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
		RdrGeoHandleMap geoCache;
		RdrGeoList      geos;

		GeoFrameState states[2];
		uint          queueState;
	} s_geoSystem;

	inline GeoFrameState& getQueueState()
	{
		return s_geoSystem.states[s_geoSystem.queueState];
	}
}

RdrGeoHandle RdrGeoSystem::CreateGeoFromFile(const char* assetName, RdrGeoInfo* pOutInfo)
{
	// Find geo in cache
	RdrGeoHandleMap::iterator iter = s_geoSystem.geoCache.find(assetName);
	if (iter != s_geoSystem.geoCache.end())
		return iter->second;

	char fullFilename[FILE_MAX_PATH];
	GeoAsset::Definition.BuildFilename(AssetLoc::Bin, assetName, fullFilename, ARRAY_SIZE(fullFilename));

	char* pFileData;
	uint fileSize;
	FileLoader::Load(fullFilename, &pFileData, &fileSize);

	GeoAsset::BinFile* pGeoBin = GeoAsset::BinFile::FromMem(pFileData);

	// Interleave data
	Vertex* pVerts = (Vertex*)RdrTransientMem::Alloc(sizeof(Vertex) * pGeoBin->vertCount);
	for (int i = pGeoBin->vertCount; i >= 0; --i)
	{
		Vertex& rVert = pVerts[i];
		rVert.position = pGeoBin->positions.ptr[i];
		rVert.texcoord = pGeoBin->texcoords.ptr[i];
		rVert.normal = pGeoBin->normals.ptr[i];
		rVert.color = pGeoBin->colors.ptr[i];
		rVert.tangent = pGeoBin->tangents.ptr[i];
		rVert.bitangent = pGeoBin->bitangents.ptr[i];
	}

	RdrGeometry* pGeo = s_geoSystem.geos.allocSafe();

	GeoCmdUpdate cmd;
	cmd.hGeo = s_geoSystem.geos.getId(pGeo);

	cmd.info.vertStride = sizeof(Vertex);
	cmd.info.numVerts = pGeoBin->vertCount;
	cmd.pVertData = pVerts;

	cmd.info.numIndices = pGeoBin->triCount * 3;
	uint16* pIndexData = (uint16*)RdrTransientMem::Alloc(sizeof(uint16) * cmd.info.numIndices);
	memcpy(pIndexData, pGeoBin->tris.ptr, sizeof(uint16) * cmd.info.numIndices);
	cmd.pIndexData = pIndexData;

	cmd.info.boundsMin = pGeoBin->boundsMin;
	cmd.info.boundsMax = pGeoBin->boundsMax;

	getQueueState().updates.push_back(cmd);

	s_geoSystem.geoCache.insert(std::make_pair(assetName, cmd.hGeo));

	if (pOutInfo)
	{
		*pOutInfo = cmd.info;
	}

	delete pFileData;
	return cmd.hGeo;
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
