#include "p_constants.h"
#include "c_constants.h"
#include "p_util.hlsli"
#include "light_types.h"

cbuffer PerAction : register(b0)
{
	PsPerAction cbPerAction;
};

cbuffer MaterialParamsBuffer : register(b1)
{
	MaterialParams cbMaterial;
}

cbuffer GlobalLightsBuffer : register(b2)
{
	GlobalLightData cbGlobalLights;
}

cbuffer AtmosphereParamsBuffer : register(b3)
{
	AtmosphereParams cbAtmosphere;
}


#if DEPTH_ONLY
	#define PsOutput void
#else
	#define WRITE_GBUFFERS !HAS_ALPHA
	struct PsOutput
	{
		float4 color : SV_TARGET0;

		// Alpha doesn't write to the extra g-buffers
		#if WRITE_GBUFFERS
			float4 albedo : SV_TARGET1;
			float4 normal : SV_TARGET2;
		#endif
	};
#endif
