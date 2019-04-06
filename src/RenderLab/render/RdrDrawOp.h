#pragma once
#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "RdrMaterial.h"
#include "RdrInstancedObjectDataBuffer.h"
#include "RdrDebugBackpointer.h"

struct RdrDrawOpSortKey
{
	union
	{
		// Note: Each uint64 must be in reverse order due to the byte ordering
		//	when using the unioned comparison values.
		struct
		{
			uint depth;
			uint16 material;
			uint16 geo;
		} alpha;

		struct 
		{
			// First uint64
			uint16 material;
			uint16 geo;
			uint unused;
		} opaque;

		struct  
		{
			uint64 val;
		} compare;
	};
};

static_assert(sizeof(RdrDrawOpSortKey) == sizeof(uint64), "RdrDrawOpSortKey size has changed.  Update compare vals.");

struct RdrDrawOp
{
	static void BuildSortKey(const RdrDrawOp* pDrawOp, const float minDepth, RdrDrawOpSortKey& rOutKey);

	RdrConstantBufferHandle hVsConstants;
	RdrInstancedObjectDataId instanceDataId;

	const RdrMaterial* pMaterial;

	RdrGeoHandle hGeo;

	// Custom instance buffer for special objects like terrain that don't use the normal instancing system.
	RdrResourceHandle hCustomInstanceBuffer;
	uint16 instanceCount;

	uint8 bHasAlpha : 1;
	uint8 unused : 7;

	RdrDebugBackpointer debug;
};

struct RdrDrawOpSet
{
	RdrDrawOpSet()
		: aDrawOps(nullptr), numDrawOps(0) {}
	RdrDrawOpSet(RdrDrawOp* aDrawOps, uint16 numDrawOps)
		: aDrawOps(aDrawOps), numDrawOps(numDrawOps) {}

	RdrDrawOp* aDrawOps;
	uint16 numDrawOps;
};

//////////////////////////////////////////////////////////////////////////

inline void RdrDrawOp::BuildSortKey(const RdrDrawOp* pDrawOp, const float minDepth, RdrDrawOpSortKey& rOutKey)
{
	if (pDrawOp->bHasAlpha)
	{
		rOutKey.alpha.geo = pDrawOp->hGeo;
		rOutKey.alpha.material = RdrMaterial::GetMaterialId(pDrawOp->pMaterial);
		rOutKey.alpha.depth = (uint)minDepth;
	}
	else
	{
		rOutKey.opaque.geo = pDrawOp->hGeo;
		rOutKey.opaque.material = RdrMaterial::GetMaterialId(pDrawOp->pMaterial);
		rOutKey.opaque.unused = 0;
	}
}

inline bool operator == (const RdrDrawOpSortKey& rLeft, const RdrDrawOpSortKey& rRight)
{
	return rLeft.compare.val == rRight.compare.val;
}