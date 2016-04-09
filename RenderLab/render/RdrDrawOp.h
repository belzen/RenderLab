#pragma once
#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"

#define MAX_TEXTURES_PER_DRAW 16

enum RdrDrawOpType
{
	kRdrDrawOpType_Graphics,
	kRdrDrawOpType_Compute
};

struct RdrDrawOp
{
	static RdrDrawOp* Allocate();
	static void Release(RdrDrawOp* pDrawOp);

	RdrSamplerState samplers[MAX_TEXTURES_PER_DRAW];
	RdrResourceHandle hTextures[MAX_TEXTURES_PER_DRAW];
	uint texCount;

	union
	{
		struct
		{
			RdrConstantBufferHandle hVsConstants;

			RdrInputLayoutHandle hInputLayout;
			RdrVertexShader      vertexShader;
			RdrShaderHandle      hPixelShaders[kRdrShaderMode_Count];

			RdrGeoHandle hGeo;
			bool bFreeGeo;

			bool bNeedsLighting;
		} graphics;

		struct
		{
			RdrComputeShader shader;
			uint threads[3];

			RdrResourceHandle hViews[8];
			uint viewCount;

			RdrConstantBufferHandle hCsConstants;
		} compute;
	};

	RdrDrawOpType eType;
};