#include "c_constants.h"

cbuffer AtmosphereParamsBuffer : register(c0)
{
	AtmosphereParams cbAtmosphere;
};

#include "atmosphere_inc.hlsli"

RWTexture2D<float4> g_outputTexTransmittanceLut : register(u0);

// distanceToEndPoint - Distance along the view vector to the end point (either the ground or the top of the atmosphere)
// cosViewAngle - Cosine of the view angle against the zenith.  The view angle at the zenith (straight up) would be 0 degrees
// altitudeScale - Effectively the density of the air.  Modifies the rate at which transmittance is reduced.
float calcOpticalDepth(float startAltitude, float distanceToEndPoint, float cosViewAngle, float altitudeScale)
{
	const float kNumSteps = 64;
	float result = 0.f;
	float dx = distanceToEndPoint / kNumSteps; // Distance along the view vector to step at each iteration

	// Note that we first need to convert the altitude to a radius from the center of the planet.
	float r = (startAltitude + cbAtmosphere.planetRadius);

	// Riemann middle sum of the transmittance integral (see the Bruneton08 paper).
	for (int i = 0; i < kNumSteps; ++i)
	{
		float viewDist = (i + 0.5f) * dx; // Distance along the view vector
		float altitude = atmGetAltitudeAlongViewVector(r, viewDist, cosViewAngle);
		result += exp(-altitude / altitudeScale);
	}

	// Finish the riemann sum.
	return result * dx;
}

[numthreads(LUT_THREADS_X, LUT_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	uint2 lutDimensions;
	g_outputTexTransmittanceLut.GetDimensions(lutDimensions.x, lutDimensions.y);

	// Extract transmittance params
	float altitude;
	float cosViewAngle;
	atmGetTransmittanceParamsAtPixel(globalId.xy, lutDimensions.xy, altitude, cosViewAngle);

	float3 transmittance = 0;

	// Determine if the view direction intersects the ground of the planet.
	if (cosViewAngle > atmCalcGroundIntersectionAngleCos(altitude))
	{
		// View angle is above the point where it would intersect with the ground.  Determine transmittance.
		float r = cbAtmosphere.planetRadius + altitude;
		float distanceToEndPoint = atmCalcDistanceToIntersection(r, cosViewAngle);
		float rayleighDepth = calcOpticalDepth(altitude, distanceToEndPoint, cosViewAngle, cbAtmosphere.rayleighAltitudeScale);
		float mieDepth = calcOpticalDepth(altitude, distanceToEndPoint, cosViewAngle, cbAtmosphere.mieAltitudeScale);

		// TODO: Add ozone transmittance (Frostbite SIGGRAPH 2016)
		float3 rayleighExtinction = cbAtmosphere.rayleighScatteringCoeff;
		float3 mieExtinction = cbAtmosphere.mieScatteringCoeff / 0.9f; // Bruneton08 Figure 6: Mie scattering / extinction = 0.9
		transmittance = exp(-(rayleighExtinction * rayleighDepth + mieExtinction * mieDepth));
	}

	g_outputTexTransmittanceLut[globalId.xy] = float4(transmittance, 0.f);
}
