#include "p_constants.h"
#include "c_constants.h"

cbuffer PerAction : register(b0)
{
	PsPerAction cbPerAction;
};

cbuffer VolumetricFogBuffer : register(b1)
{
	VolumetricFogParams cbFog;
};

Texture3D<float4> g_texFogLightingLut : register(t0);

RWTexture3D<float4> g_texFinalLut : register(u0);

[numthreads(VOLFOG_LUT_THREADS_X, VOLFOG_LUT_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	float3 accumScattering = 0.f;
	float accumTransmittance = 1.f;

	float depthStep = (cbFog.farDepth - cbPerAction.cameraNearDist) / cbFog.lutSize.z;
	// For each XY pixel in the LUT, step through each depth slice and accumulate scattering.
	for (uint i = 0; i < cbFog.lutSize.z; ++i)
	{
		uint3 lutPixel = uint3(globalId.xy, i);

		float4 scattering_extinction = g_texFogLightingLut[lutPixel];
		float transmittance = exp(-(scattering_extinction.a * depthStep));
		accumScattering += scattering_extinction.rgb * depthStep * accumTransmittance;
		accumTransmittance *= transmittance;
		// TODO: Improve accumulation with Frostbite SIGGRAPH'15 talk.

		// Write final lighting value to the 3D lut
		g_texFinalLut[lutPixel] = float4(accumScattering, accumTransmittance);
	}
}
