#pragma once
#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "RdrMaterial.h"

enum class RdrDrawOpType
{
	Graphics,
	Compute
};

struct RdrDrawOp
{
	static RdrDrawOp* Allocate();
	static void Release(RdrDrawOp* pDrawOp);

	union
	{
		struct
		{
			RdrConstantBufferHandle hVsConstants;

			RdrInputLayoutHandle hInputLayout;
			RdrVertexShader      vertexShader;

			const RdrMaterial* pMaterial;

			RdrGeoHandle hGeo;
			bool bFreeGeo;
		} graphics;

		struct
		{
			RdrComputeShader shader;
			uint threads[3];

			RdrResourceHandle hViews[8];
			uint viewCount;

			RdrResourceHandle hTextures[4];
			uint texCount;

			RdrConstantBufferHandle hCsConstants;
		} compute;
	};

	RdrDrawOpType eType;
};