#include "c_constants.h"

cbuffer AtmosphereParamsBuffer : register(c0)
{
	AtmosphereParams cbAtmosphere;
};

#include "atmosphere_inc.hlsli"

Texture2D<float4> g_texTransmittance : register(t0);
Texture3D<float4> g_texRadiance : register(t1);
SamplerState g_sampler : register(s0);

RWTexture3D<float4> g_outputTexScatter : register(u0);

// Calculating delta S for multiple scattering.
// Line 9 of algorithm 4.1 from Bruneton08
[numthreads(LUT_THREADS_X, LUT_THREADS_Y, 1)]
void main( uint3 globalId : SV_DispatchThreadID )
{
	uint3 dims;
	g_outputTexScatter.GetDimensions(dims.x, dims.y, dims.z);

	// Extract scatter params from pixel
	float startAltitude, cosViewAngle, cosSunAngle;
	atmGet3dLutParamsAtPixel(globalId, dims, startAltitude, cosViewAngle, cosSunAngle);

	float r = cbAtmosphere.planetRadius + startAltitude;
	float t = atmCalcDistanceToIntersection(r, cosViewAngle);

	float3 result = 0;

	// Cache the transmittance from the starting altitude/angle
	float3 startTransmittance = atmSampleTransmittance(g_texTransmittance, g_sampler, startAltitude, ((cosViewAngle > 0.f) ? cosViewAngle : -cosViewAngle));

	// Step along the view direction and accumulate radiance attenuated by transmittance.
	const uint kNumSamples = 64;
	float dx = t / kNumSamples;
	for (uint i = 0; i < kNumSamples; ++i)
	{
		float d = (i + 0.5f) * dx;

		// Calculate parameters at view point accounting for the curvature of the planet.
		float altitude = atmGetAltitudeAlongViewVector(r, d, cosViewAngle);
		float r2 = cbAtmosphere.planetRadius + altitude;
		float cosViewAngle2 = (r * cosViewAngle + t) / r2;
		float cosSunAngle2 = (t + cosSunAngle * r) / r2;

		// Calculate the transmittance from the start to the current position along the view vector.
		// T(x, y) = T'(x, v) / T'(y, v) where v = (y - x) / |(y - x)|  (Section 4. Precomputations from Bruneton08)
		float3 transmittance = 0;
		if (cosViewAngle > 0.f)
		{
			transmittance = min(startTransmittance / atmSampleTransmittance(g_texTransmittance, g_sampler, altitude, cosViewAngle2), 1.0);
		}
		else 
		{
			// Angle is downward, so we need to flip the ordering of the equation.   This also requires negating the view direction
			transmittance = min(atmSampleTransmittance(g_texTransmittance, g_sampler, altitude, -cosViewAngle2) / startTransmittance, 1.0);
		}

		// Accumulate attenuated radiance.
		float3 irradiance = atmSampleRadiance(g_texRadiance, g_sampler, altitude, cosViewAngle2, cosSunAngle2);
		result += transmittance * irradiance;
	}

	g_outputTexScatter[globalId.xyz] = float4(result, 0);
}