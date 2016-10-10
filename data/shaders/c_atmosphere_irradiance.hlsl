#include "c_constants.h"

cbuffer AtmosphereParamsBuffer : register(c0)
{
	AtmosphereParams cbAtmosphere;
};

#include "atmosphere_inc.hlsli"

#if IRRADIANCE_INITIAL
Texture2D<float4> g_texTransmittance : register(t0);
#elif COMBINED_SCATTER
Texture3D<float4> g_texScatter : register(t0);
#else
Texture3D<float4> g_texScatterRayleigh : register(t0);
Texture3D<float4> g_texScatterMie : register(t1);
#endif

SamplerState g_sampler : register(s0);

RWTexture2D<float4> g_outputTexIrradiance : register(u0);

[numthreads(ATM_LUT_THREADS_X, ATM_LUT_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	uint2 dims;
	g_outputTexIrradiance.GetDimensions(dims.x, dims.y);

	// Extract params from pixel coord.
	float cosSunAngle = kCosSunAngleMin + (1.f - kCosSunAngleMin) * (globalId.x / (dims.x - 1.f));
	float altitude = cbAtmosphere.atmosphereHeight * (globalId.y / (dims.y - 1.f));

#if IRRADIANCE_INITIAL
	// First delta irradiance.
	// Line 2 of algorithm 4.1 from Bruneton08
	float3 transmittance = atmSampleTransmittance(g_texTransmittance, g_sampler, altitude, cosSunAngle);
	g_outputTexIrradiance[globalId.xy] = float4(transmittance * max(cosSunAngle, 0.f), 0.f);
#else
	// Calc delta irradiance for the current scattering order
	// Line 8 of algorithm 4.1 from Bruneton08
	float3 result = 0;
	float3 sunDir = float3(sqrt(1.f - cosSunAngle * cosSunAngle), cosSunAngle, 0.f);

	// Iterate over the sphere and accumulate irradiance from scattering in every direction.
	//http://mathworld.wolfram.com/SphericalCoordinates.html
	const uint kNumSamples = 32;
	float angleDelta = kPi / kNumSamples;
	for (uint iTheta = 0; iTheta < 2 * kNumSamples; ++iTheta)
	{
		float theta = (iTheta + 0.5f) * angleDelta;

		for (uint iPhi = 0; iPhi < kNumSamples / 2; ++iPhi)
		{
			float phi = (iPhi + 0.5f) * angleDelta;

			// Calc direction along the current spherical coordinates.
			float3 dir = float3(cos(theta) * sin(phi), cos(phi), sin(theta) * sin(phi));
			
			#if COMBINED_SCATTER
				float3 scatter = atmSampleScatter(g_texScatter, g_sampler, altitude, dir.y, cosSunAngle).rgb;
			#else
				float3 scatterRay = atmSampleScatter(g_texScatterRayleigh, g_sampler, altitude, dir.y, cosSunAngle).rgb;
				float3 scatterMie = atmSampleScatter(g_texScatterMie, g_sampler, altitude, dir.y, cosSunAngle).rgb;
				// Single scattering results don't include the phase functions, so we need to apply them prior to accumulating irradiance.
				float cosViewAngle = dot(sunDir, dir);
				float3 scatter = atmCalcPhaseRayleigh(cosViewAngle) * scatterRay + atmCalcPhaseMie(cosViewAngle) * scatterMie;
			#endif

			// Accumulate irradiance (equation 15 from Bruneton08)
			float dw = angleDelta * angleDelta * sin(phi);
			result += scatter * dir.y * dw;
		}
	}

	g_outputTexIrradiance[globalId.xy] = float4(result, 0.f);
#endif
}