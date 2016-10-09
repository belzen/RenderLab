#include "c_constants.h"

cbuffer AtmosphereParamsBuffer : register(c0)
{
	AtmosphereParams cbAtmosphere;
};

#include "atmosphere_inc.hlsli"

Texture2D<float4> g_texTransmittanceLut : register(t0);
SamplerState g_sampler : register(s0);

RWTexture3D<float4> g_outputTexRayleighScatteringLut : register(u0);
RWTexture3D<float4> g_outputTexMieScatteringLut : register(u1);
RWTexture3D<float4> g_outputTexScatteringLut : register(u2);


/* todo : non-linear scattering.  From Bruneton08:
static const float RES_MU = 128.f;
static const float RES_MU_S = 32.f * 8.f;
static const float RES_NU = 8.f;

void getMuMuSNu(float x, float y, float r, out float mu, out float muS, out float nu) {

float dmin = Rt - r;
float dmax = sqrt(r * r - Rg * Rg) + sqrt(Rt * Rt - Rg * Rg);
float dminp = r - Rg;
float dmaxp = sqrt(r * r - Rg * Rg);
float4 dhdH = float4(dmin, dmax, dminp, dmaxp);

x = x - 0.5;
y = y - 0.5;
#if INSCATTER_NON_LINEAR
	if (y < float(RES_MU) / 2.0) {
		float d = 1.0 - y / (float(RES_MU) / 2.0 - 1.0);
		d = min(max(dhdH.z, d * dhdH.w), dhdH.w * 0.999);
		mu = (Rg * Rg - r * r - d * d) / (2.0 * r * d);
		mu = min(mu, -sqrt(1.0 - (Rg / r) * (Rg / r)) - 0.001);
	}
	else {
		float d = (y - float(RES_MU) / 2.0) / (float(RES_MU) / 2.0 - 1.0);
		d = min(max(dhdH.x, d * dhdH.y), dhdH.y * 0.999);
		mu = (Rt * Rt - r * r - d * d) / (2.0 * r * d);
	}
	muS = (x % float(RES_MU_S)) / (float(RES_MU_S) - 1.0);
	// paper formula
	//muS = -(0.6 + log(1.0 - muS * (1.0 -  exp(-3.6)))) / 3.0;
	// better formula
	muS = tan((2.0 * muS - 1.0 + 0.26) * 1.1) / tan(1.26 * 1.1);
	nu = 0;// -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;
#else

	mu = -1.0 + 2.0 * y / (float(RES_MU) - 1.0);
	muS = (x % float(RES_MU_S)) / (float(RES_MU_S) - 1.0);
	muS = -0.2 + muS * 1.2;
	nu = -1.0 + floor(x / float(RES_MU_S)) / (float(RES_NU) - 1.0) * 2.0;
#endif
}
*/

[numthreads(LUT_THREADS_X, LUT_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	uint3 scatteringLutDimensions;
	g_outputTexRayleighScatteringLut.GetDimensions(scatteringLutDimensions.x, scatteringLutDimensions.y, scatteringLutDimensions.z);

	float3 rayleigh = 0.f;
	float3 mie = 0.f;

	float cosViewToSunAngle = 1.f; // Elek09 - Constant view-sun angle.  Doesn't have a strong impact and simplifies to a 3D lut.
	float origAltitude, cosViewAngle, cosSunAngle;
	atmGet3dLutParamsAtPixel(globalId, scatteringLutDimensions, origAltitude, cosViewAngle, cosSunAngle);

	float r = cbAtmosphere.planetRadius + origAltitude;

	// Transmittance at the original point.
	float3 origTransmittance = atmSampleTransmittance(g_texTransmittanceLut, g_sampler, origAltitude, ((cosViewAngle > 0.f) ? cosViewAngle : -cosViewAngle));

	// Step along the view vector and accumulate scattering.
	const uint kNumSteps = 64;
	float dx = atmCalcDistanceToIntersection(r, cosViewAngle) / kNumSteps; // Distance along the view vector to step at each iteration
	for (uint i = 0; i < kNumSteps; ++i)
	{
		float viewDist = (i + 0.5f) * dx; // Distance along the view vector

		// Calculate params for the current step position.
		float altitude = atmGetAltitudeAlongViewVector(r, viewDist, cosViewAngle);
		float r1 = cbAtmosphere.planetRadius + altitude;
		float cosSunAngle1 = (r * cosSunAngle + viewDist * cosViewToSunAngle) / r1;

		r1 = max(cbAtmosphere.planetRadius, r1);
		altitude = r1 - cbAtmosphere.planetRadius;

		// Check if the sun is visible.
		float cosGroundAngle = atmCalcGroundIntersectionAngleCos(altitude);
		if (cosSunAngle1 >= cosGroundAngle)
		{
			// Sun is not occluded by the planet at this angle, accumulate scattering.
			float cosViewAngle1 = (r * cosViewAngle + viewDist) / r1;

			// Calculate the transmittance from the start to the desired position along the view vector.
			// T(x, y) = T'(x, v) / T'(y, v) where v = (y - x) / |(y - x)|  (Section 4. Precomputations from Bruneton08)
			float3 transmittance = 0.f;
			if (cosViewAngle > 0.0)
			{
				transmittance = min(origTransmittance / atmSampleTransmittance(g_texTransmittanceLut, g_sampler, altitude, cosViewAngle1), 1.0);
			}
			else
			{
				// Angle is downward, so we need to flip the ordering of the equation.   This also requires negating the view direction
				transmittance = min(atmSampleTransmittance(g_texTransmittanceLut, g_sampler, altitude, -cosViewAngle1) / origTransmittance, 1.0);
			}

			// Scattering equation from Eksel09:
			//		S = Density * exp(-T(x, sunAngle) - T(x, viewAngle)
			// The transmittance lut already contains the exp(), so we can factor the exponent out into:
			//		S = Density * T'(x, sunAngle) * T'(x, viewAngle)
			transmittance = transmittance * atmSampleTransmittance(g_texTransmittanceLut, g_sampler, altitude, cosSunAngle1);
			rayleigh += atmCalcDensityRayleigh(altitude) * transmittance * dx;
			mie += atmCalcDensityMie(altitude) * transmittance * dx;
		}
	}
	rayleigh *= cbAtmosphere.rayleighScatteringCoeff;
	mie *= cbAtmosphere.mieScatteringCoeff;

	// Independent scattering results to use in the first pass of irradiance and radiance.
	g_outputTexRayleighScatteringLut[globalId.xyz] = float4(rayleigh, 0.f);
	g_outputTexMieScatteringLut[globalId.xyz] = float4(mie, 0.f);
	// Merged scattering results.  Line 5 of algorithm 4.1
	// Note that only the red component of Mie is being stored.  
	g_outputTexScatteringLut[globalId.xyz] = float4(rayleigh.rgb, mie.r);
}