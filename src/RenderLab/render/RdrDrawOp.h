#pragma once
#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "RdrMaterial.h"
#include "RdrInstancedObjectDataBuffer.h"

struct RdrDispatchOp
{
	static RdrDispatchOp* Allocate();
	static void QueueRelease(const RdrDispatchOp* pDrawOp);
	static void ProcessReleases();

	RdrComputeShader shader;
	uint threads[3];

	RdrResourceHandle hViews[8];
	uint viewCount;

	RdrResourceHandle hTextures[4];
	uint texCount;

	RdrConstantBufferHandle hCsConstants;
};

struct RdrDrawOpSortKey
{
	union
	{
		struct
		{
			uint depth;
			uint16 vertexShaderType;
			uint16 vertexShaderFlags;
			uint16 geo;
			uint16 material;
			uint unused;
		} alpha;

		struct 
		{
			uint16 vertexShaderType;
			uint16 vertexShaderFlags;
			uint16 geo;
			uint16 material;
			uint bCanInstance : 1;
			uint unusedBits : 31;
			uint unused;
		} opaque;

		struct  
		{
			uint64 val1;
			uint64 val2;
		} compare;
	};
};

static_assert(sizeof(RdrDrawOpSortKey) == sizeof(uint64) * 2, "RdrDrawOpSortKey size has changed.  Update compare vals.");

struct RdrDrawOp
{
	static RdrDrawOp* Allocate();
	static void QueueRelease(const RdrDrawOp* pDrawOp);
	static void ProcessReleases();
	static void BuildSortKey(const RdrDrawOp* pDrawOp, const float minDepth, RdrDrawOpSortKey& rOutKey);

	RdrConstantBufferHandle hVsConstants;
	RdrInstancedObjectDataId instanceDataId;

	RdrInputLayoutHandle hInputLayout;
	RdrVertexShader      vertexShader;

	const RdrMaterial* pMaterial;

	RdrGeoHandle hGeo;
	uint8 bFreeGeo : 1;
	uint8 bHasAlpha : 1;
	uint8 bIsSky : 1;
	uint8 bTempDrawOp : 1;
	uint8 unused : 4;
};

inline void RdrDrawOp::BuildSortKey(const RdrDrawOp* pDrawOp, const float minDepth, RdrDrawOpSortKey& rOutKey)
{
	if (pDrawOp->bHasAlpha)
	{
		rOutKey.alpha.vertexShaderType = (uint16)pDrawOp->vertexShader.eType;
		rOutKey.alpha.vertexShaderFlags = (uint16)pDrawOp->vertexShader.flags;
		rOutKey.alpha.geo = pDrawOp->hGeo;
		rOutKey.alpha.material = RdrMaterial::GetMaterialId(pDrawOp->pMaterial);
		rOutKey.alpha.depth = (uint)minDepth;
		rOutKey.alpha.unused = 0;
	}
	else
	{
		rOutKey.opaque.vertexShaderType = (uint16)pDrawOp->vertexShader.eType;
		rOutKey.opaque.vertexShaderFlags = (uint16)pDrawOp->vertexShader.flags;
		rOutKey.opaque.geo = pDrawOp->hGeo;
		rOutKey.opaque.material = RdrMaterial::GetMaterialId(pDrawOp->pMaterial);
		rOutKey.opaque.bCanInstance = !!pDrawOp->instanceDataId;
		rOutKey.opaque.unusedBits = 0;
		rOutKey.opaque.unused = 0;
	}
}

inline bool operator == (const RdrDrawOpSortKey& rLeft, const RdrDrawOpSortKey& rRight)
{
	return rLeft.compare.val1 == rRight.compare.val1 &&
		rLeft.compare.val2 == rRight.compare.val2;
}