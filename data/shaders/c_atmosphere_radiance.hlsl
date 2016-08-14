#define ATMOSPHERE_LUT_CALC 1
#include "c_constants.h"

cbuffer AtmosphereParamsBuffer : register(c0)
{
	AtmosphereParams cbAtmosphere;
};

#include "atmosphere_inc.hlsli"

SamplerState g_sampler : register(s0);

Texture2D<float4> g_texTransmittance : register(t0);
Texture2D<float4> g_texIrradiance : register(t1);

#if COMBINED_SCATTER
Texture3D<float4> g_texScatter : register(t2);
#else
Texture3D<float4> g_texScatterRayleigh : register(t2);
Texture3D<float4> g_texScatterMie : register(t3);
#endif

RWTexture3D<float4> g_outputTexRadiance : register(u0);

// Equation 7 from Bruneton08 (and line 7 from algorithm 4.1).  Calculating the delta J/radiance
[numthreads(LUT_THREADS_X, LUT_THREADS_Y, 1)]
void main( uint3 globalId : SV_DispatchThreadID )
{
	uint3 dims;
	g_outputTexRadiance.GetDimensions(dims.x, dims.y, dims.z);

	// Extract radiance params from pixel coord
	float altitude;
	float cosViewAngle;
	float cosSunAngle;
	atmGet3dLutParamsAtPixel(globalId, dims, altitude, cosViewAngle, cosSunAngle);

	float3 viewDir = float3(sqrt(1.f - cosViewAngle * cosViewAngle), cosViewAngle, 0.f);
	float3 sunDir = float3(sqrt(1.f - cosSunAngle * cosSunAngle), cosSunAngle, 0.f);

	float3 accumScatter = 0;

	float r = cbAtmosphere.planetRadius + altitude;
	// Calculate the cosine of the angle where ground intersection occurs.  Radiance will be affected by ground reflectance below this angle.
	float phiCosGroundIntersection = atmCalcGroundIntersectionAngleCos(altitude);

	// Iterate over the sphere and accumulate radiance from scattering in every direction.
	//http://mathworld.wolfram.com/SphericalCoordinates.html
	const uint kNumSamples = 32;
	float angleDelta = (kPi / kNumSamples);
	for (uint iPhi = 0; iPhi < kNumSamples; ++iPhi)
	{
		float phi = (iPhi + 0.5f) * angleDelta;
		float phiCos = cos(phi);
		float phiSin = sin(phi);

		float groundReflectance = 0.f;
		float distanceToGround = 0.f;
		float3 groundTransmittance = 0.f;
		if (phiCos < phiCosGroundIntersection)
		{
			// Calculate the transmittance of light that hit the ground and reflects along the current spherical direction.
			groundReflectance = cbAtmosphere.averageGroundReflectance / kPi;
			distanceToGround = atmCalcDistanceToGround(r, phiCos);
			// Get the cosine of the reflection angle.  Similar to 
			float cosReflectionAngle = (r * -phiCos - distanceToGround) / cbAtmosphere.planetRadius;
			groundTransmittance = atmGetTransmittanceSegment(g_texTransmittance, g_sampler, 0.01f, cosReflectionAngle, distanceToGround);
		}

		for (uint iTheta = 0; iTheta < 2 * kNumSamples; ++iTheta)
		{
			float theta = (iTheta + 0.5f) * angleDelta;

			// Calc direction along the current spherical coordinates.
			float3 dir = float3(cos(theta) * phiSin, phiCos, sin(theta) * phiSin);

			// Calculate the light reflecting off the ground.
			float3 groundNormal = (float3(0.f, r, 0.f) + distanceToGround * dir) / cbAtmosphere.planetRadius;
			float3 groundIrradiance = atmSampleIrradiance(g_texIrradiance, g_sampler, 0.f, dot(groundNormal, sunDir));
			float3 scatter = groundReflectance * groundIrradiance * groundTransmittance;

			#if COMBINED_SCATTER
				scatter += atmSampleScatter(g_texScatter, g_sampler, altitude, phiCos, cosSunAngle).rgb;
			#else
				float3 scatterRay = atmSampleScatter(g_texScatterRayleigh, g_sampler, altitude, phiCos, cosSunAngle).rgb;
				float3 scatterMie = atmSampleScatter(g_texScatterMie, g_sampler, altitude, phiCos, cosSunAngle).rgb;
				// Single scattering results don't include the phase functions, so we need to apply them prior to accumulating radiance.
				float nu1 = dot(sunDir, dir);
				scatter += scatterRay * atmCalcPhaseRayleigh(nu1) + scatterMie * atmCalcPhaseMie(nu1);
			#endif

			// Accumulate radiance from the current direction
			float cosViewSphereAngle = dot(viewDir, dir); // Angle between view direction and current spherical direction.
			float dw = angleDelta * angleDelta * phiSin;
			accumScatter += scatter * dw * 
				(cbAtmosphere.rayleighScatteringCoeff * atmCalcDensityRayleigh(altitude) * atmCalcPhaseRayleigh(cosViewSphereAngle) +
				cbAtmosphere.mieScatteringCoeff * atmCalcDensityMie(altitude) * atmCalcPhaseMie(cosViewSphereAngle));
		}
	}

	g_outputTexRadiance[globalId] = float4(accumScatter, 0.f);
}
