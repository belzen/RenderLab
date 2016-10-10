#pragma once
#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "RdrMaterial.h"
#include "RdrTessellationMaterial.h"
#include "RdrInstancedObjectDataBuffer.h"

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
			uint unused1;
			uint unused2;
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
	static void BuildSortKey(const RdrDrawOp* pDrawOp, const float minDepth, RdrDrawOpSortKey& rOutKey);

	RdrConstantBufferHandle hVsConstants;
	RdrInstancedObjectDataId instanceDataId;

	RdrInputLayoutHandle  hInputLayout;
	RdrVertexShader       vertexShader;

	const RdrTessellationMaterial* pTessellationMaterial;
	const RdrMaterial* pMaterial;

	RdrGeoHandle hGeo;

	// Custom instance buffer for special objects like terrain that don't use the normal instancing system.
	RdrResourceHandle hCustomInstanceBuffer;
	uint16 instanceCount;

	uint8 bFreeGeo : 1;
	uint8 bHasAlpha : 1;
	uint8 unused : 6;
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
		rOutKey.opaque.unused1 = 0;
		rOutKey.opaque.unused2 = 0;
	}
}

inline bool operator == (const RdrDrawOpSortKey& rLeft, const RdrDrawOpSortKey& rRight)
{
	return rLeft.compare.val1 == rRight.compare.val1 &&
		rLeft.compare.val2 == rRight.compare.val2;
}