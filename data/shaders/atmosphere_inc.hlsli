// References:
// Precomputed Atmospheric Scattering, Bruneton08 
//	http://www-ljk.imag.fr/Publications/Basilic/com.lmc.publi.PUBLI_Article@11e7cdda2f7_f64b69/article.pdf
//	http://evasion.inrialpes.fr/~Eric.Bruneton/
// Rendering Parametrizable Planetary Atmospheres with Multiple Scattering in Real-Time, Elek09
//	http://www.cescg.org/CESCG-2009/papers/PragueCUNI-Elek-Oskar09.pdf

#include "c_constants.h"

static const float kPi = 3.14159f;
static const float kDegToRad = kPi / 180.f;

// Minimum cos of the angle between the zenith and the view direction.
// Starts at -0.15 because all transmittance below that is occluded by the planet surface
static const float kCosViewAngleMin = -0.15f;
// Minimum cos of the angle between the zenith and the sun direction.
static const float kCosSunAngleMin = -0.2f;

float atmCalcDensityRayleigh(float altitude)
{
	return exp(-altitude / cbAtmosphere.rayleighAltitudeScale);
}

float atmCalcDensityMie(float altitude)
{
	return exp(-altitude / cbAtmosphere.mieAltitudeScale);
}

float atmCalcPhaseRayleigh(float cosViewAngle)
{
	// Equation 2 from Bruneton08
	return 3.f / (16.f * kPi) * (1.f + cosViewAngle * cosViewAngle);
}

float atmCalcPhaseMie(float cosViewAngle)
{
	// Equation 4 from Bruneton08
	float g = cbAtmosphere.mieG;
	float g2 = g * g;

	float k = 3.f / (8.f * kPi);
	float num = (1 - g2) * (1 + cosViewAngle * cosViewAngle);
	float div = (2 + g2) * pow(1 + g2 - 2 * g * cosViewAngle, 1.5f);
	return k * num / div;
}

// Rebuild single Mie scattering from the red component stored in the combined scattering results.
float3 atmExtractMie(float4 scatter) 
{
	// Extract out the full Mie scattering values by approximation.
	// Section 4 of Bruneton08 - Angular precision (Cm = (C*) * (Cm.r / C*.r) * (Br.r / Bm.r) * (Bm / Br))
	return scatter.rgb * 
		(scatter.w / max(scatter.r, 1e-4)) *
		(cbAtmosphere.rayleighScatteringCoeff.r / cbAtmosphere.mieScatteringCoeff.r) *
		(cbAtmosphere.mieScatteringCoeff / cbAtmosphere.rayleighScatteringCoeff);
}

float atmGetAltitudeAlongViewVector(float r, float viewDist, float cosViewAngle)
{
	// Use the law of cosines - c^2 = sqrt(a^2 + b^2 - 2ab * cos(C)) to 
	// find the altitude along the view direction at a given distance
	// Note that the (2*a*b*cos(C)) value is negated from the law of cosines equation.  This is because the view angle is applied from the target
	//	position which is above the triangle for which we're calculating the edge length.  However, because it's defined as angle away from the zenith
	//	and our triangle's edge (from the altitude vector) is also along the zenith, we can easily get the cos of the opposite angle 
	//	(the one actually part of the triangle) by negating it.
	return sqrt(r * r + viewDist * viewDist + 2 * r * viewDist * cosViewAngle) - cbAtmosphere.planetRadius;
}

// Find the intersection of a view ray with a circle around the planet (i.e. the surface or outer atmosphere)
float atmCalcDistanceToCircle(float r, float cosViewAngle, float radius, bool outsideCircle)
{
	// Get the length of "r" along the view direction.
	float distanceAlongViewAngle = -r * cosViewAngle;
	// Define a triangle at the end point along the view direction (calculated above), 
	// the center of the planet, and the intersection of the view dir with the desired circle
	// Given this, we can use the pythagorean theorem to calculate the length of the edge between
	// the circle intersection point and the end point of the view direction.
	float sinSqr = (1.f - cosViewAngle * cosViewAngle);
	float distIntersectionToEndPoint = sqrt((radius * radius) - (r * r * sinSqr));
	// Finally, depending on whether we're on the inside or outside of the circle,
	// we subtract or add the post-intersection segment from the full length of the view 
	// vector to find the distance to the intersection with the circle.
	return outsideCircle
		? distanceAlongViewAngle - distIntersectionToEndPoint
		: distanceAlongViewAngle + distIntersectionToEndPoint;
}

// Find the distance to the planet's surface from an altitude and view angle.
// Note: This function ONLY works if there is an intersection with the ground at the angle.
//			Use atmCalcDistanceToIntersection() if ground intersection is not guaranteed ahead of time.
float atmCalcDistanceToGround(float r, float cosViewAngle)
{
	return atmCalcDistanceToCircle(r, cosViewAngle, cbAtmosphere.planetRadius, true);
}

// Find the distance to the top of the atmosphere from an altitude and view angle.
// Note: Altitude must be in the range [0, atmosphereHeight] or this will fail.
float atmCalcDistanceToAtmosphereTop(float r, float cosViewAngle)
{
	return atmCalcDistanceToCircle(r, cosViewAngle, cbAtmosphere.atmosphereRadius + 1.f, false);
}

float atmCalcDistanceToAtmosphereFromSpace(float r, float cosViewAngle)
{
	return atmCalcDistanceToCircle(r, cosViewAngle, cbAtmosphere.atmosphereRadius, true);
}

// Find the distance to the planet's surface or top of the atmosphere, whichever comes first.
float atmCalcDistanceToIntersection(float r, float cosViewAngle)
{
	// Calc the top atmosphere intersection to fallback on if there's no ground intersection.
	float dist = atmCalcDistanceToAtmosphereTop(r, cosViewAngle);

	// See atmCalcDistanceToGround() for the details on the rest of this function.
	// The individual components are separated out so we can determine if a ground intersection actually occurs.
	float sinSqr = (1.f - cosViewAngle * cosViewAngle);
	float planetIntersectionSqr = (cbAtmosphere.planetRadius * cbAtmosphere.planetRadius) - (r * r * sinSqr);

	if (planetIntersectionSqr >= 0.0) 
	{
		// View vector intersects the ground, find the distance to it and make sure the intersection is in front of the view point.
		float distToGround = -r * cosViewAngle - sqrt(planetIntersectionSqr);
		if (distToGround >= 0.0) 
		{
			// Intersection is in front of the view point, use it as the intersection point.
			dist = min(dist, distToGround);
		}
	}

	return dist;
}

// Calc the cosine of the angle from the zenith where intersection with the ground first occurs given a point above the ground.
float atmCalcGroundIntersectionAngleCos(float altitude)
{
	//	1) sin = opposite / hypotenuse, where the triangle is a right triangle.
	//	2) Therefore, by using "planet radius + altitude" as the hypotenuse and "planet radius" as the opposite edge, we can find
	//		the sin of the angle at which the view direction will intersect the planet.
	//	3) Finally, use pythagorean identity (sin^2 + cos^2 = 1) to flip from sin to cos.
	float distanceFromPlanet = cbAtmosphere.planetRadius + altitude;
	float sinAngle = (cbAtmosphere.planetRadius / distanceFromPlanet);
	return -sqrt(1.f - sinAngle * sinAngle);
}

void atmGetTransmittanceParamsAtPixel(uint2 pixelPos, uint2 dims, out float altitude, out float cosViewAngle)
{
	float2 uv = float2(pixelPos.xy / float2(dims.xy - 1.f));
	altitude = (uv.y * uv.y * cbAtmosphere.atmosphereHeight) + 0.01f;
	cosViewAngle = kCosViewAngleMin + tan(1.5f * uv.x) / tan(1.5f) * (1.f - kCosViewAngleMin);
}

float3 atmSampleTransmittance(Texture2D<float4> texTransmittance, SamplerState texSampler, float altitude, float cosViewAngle)
{
	cosViewAngle = max(cosViewAngle, kCosViewAngleMin);
	float2 uv = float2(
		atan((cosViewAngle - kCosViewAngleMin) / (1.f - kCosViewAngleMin) * tan(1.5f)) / 1.5f,
		sqrt(altitude / cbAtmosphere.atmosphereHeight));
	return texTransmittance.SampleLevel(texSampler, uv, 0).rgb;
}

// Calculate the transmittance along a segment of a ray.
float3 atmGetTransmittanceSegment(Texture2D<float4> texTransmittance, SamplerState texSampler, float altitude, float cosViewAngle, float viewDist)
{
	float r = cbAtmosphere.planetRadius + altitude;

	// Calculate parameters at view point accounting for the curvature of the planet.
	float altitude2 = atmGetAltitudeAlongViewVector(r, viewDist, cosViewAngle);
	float r1 = cbAtmosphere.planetRadius + altitude2;
	float cosViewAngle2 = (r * cosViewAngle + viewDist) / r1;

	// Calculate the transmittance from the start to the desired position along the view vector.
	// T(x, y) = T'(x, v) / T'(y, v) where v = (y - x) / |(y - x)|  (Section 4. Precomputations from Bruneton08)
	float3 result;
	if (cosViewAngle > 0.0) 
	{
		result = min(atmSampleTransmittance(texTransmittance, texSampler, altitude, cosViewAngle) / atmSampleTransmittance(texTransmittance, texSampler, altitude2, cosViewAngle2), 1.0);
	}
	else 
	{
		// Angle is downward, so we need to flip the ordering of the equation.   This also requires negating the view direction
		result = min(atmSampleTransmittance(texTransmittance, texSampler, altitude2, -cosViewAngle2) / atmSampleTransmittance(texTransmittance, texSampler, altitude, -cosViewAngle), 1.0);
	}
	return result;
}

// Calculate the transmittance along a segment of a ray.
float3 atmGetTransmittanceSegment(Texture2D<float4> texTransmittance, SamplerState texSampler, float altitude, float cosViewAngle, float3 viewDir, float3 endPoint)
{
	float altitude2 = length(endPoint) - cbAtmosphere.planetRadius;
	float cosViewAngle2 = dot(endPoint, viewDir) / (cbAtmosphere.planetRadius + altitude);
	float3 result;
	if (cosViewAngle > 0.0)
	{
		result = min(atmSampleTransmittance(texTransmittance, texSampler, altitude, cosViewAngle) / atmSampleTransmittance(texTransmittance, texSampler, altitude2, cosViewAngle2), 1.0);
	}
	else
	{
		result = min(atmSampleTransmittance(texTransmittance, texSampler, altitude2, -cosViewAngle2) / atmSampleTransmittance(texTransmittance, texSampler, altitude, -cosViewAngle), 1.0);
	}
	return result;
}

float3 atmGetTransmittanceWithShadow(Texture2D<float4> texTransmittanceLut, SamplerState texSampler, float altitude, float cosViewAngle)
{
	return cosViewAngle < atmCalcGroundIntersectionAngleCos(altitude)
		? float3(0, 0, 0)
		: atmSampleTransmittance(texTransmittanceLut, texSampler, altitude, cosViewAngle);
}

float3 atmSampleIrradiance(Texture2D<float4> texIrradiance, SamplerState texSampler, float altitude, float cosSunAngle)
{
	float2 uv = float2(
		(cosSunAngle - kCosSunAngleMin) / (1.f - kCosSunAngleMin),
		(altitude / cbAtmosphere.atmosphereHeight));

	return texIrradiance.SampleLevel(texSampler, uv, 0).rgb;
}

void atmGet3dLutParamsAtPixel(uint3 pixelPos, uint3 dims, out float altitude, out float cosViewAngle, out float cosSunAngle)
{
	float3 uvw = (pixelPos / (dims - 1.f));
	cosSunAngle = kCosSunAngleMin + (1 - kCosSunAngleMin) * uvw.x;
	cosViewAngle = -1.f + 2.f * uvw.y;
	altitude = uvw.z * uvw.z * cbAtmosphere.atmosphereHeight;
	altitude += (pixelPos.z == 0 ? 0.01f : (pixelPos.z == dims.z - 1 ? -0.001f : 0.0f));
}

float3 atmGet3dLutUv(float altitude, float cosViewAngle, float cosSunAngle)
{
	float normalizedViewAngle = (cosViewAngle + 1.f) * 0.5f;
	float normalizedSunAngle = (cosSunAngle - kCosSunAngleMin) / (1.f - kCosSunAngleMin);

	return float3(
		normalizedSunAngle,
		normalizedViewAngle,
		(altitude / cbAtmosphere.atmosphereHeight));
}

float3 atmSampleRadiance(Texture3D<float4> texRadiance, SamplerState texSampler, float altitude, float cosViewAngle, float cosSunAngle)
{
	float3 uvw = atmGet3dLutUv(altitude, cosViewAngle, cosSunAngle);
	return texRadiance.SampleLevel(texSampler, uvw, 0).rgb;
}

float4 atmSampleScatter(Texture3D<float4> texScatter, SamplerState texSampler, float altitude, float cosViewAngle, float cosSunAngle)
{
	float3 uvw = atmGet3dLutUv(altitude, cosViewAngle, cosSunAngle);
	return texScatter.SampleLevel(texSampler, uvw, 0);
}

// Scattering component of equation 16 from Bruneton08
float3 atmCalcInscatter(float3 viewPos, float3 viewDir, float distToGround, float3 sunDir, 
	float altitude, float r, float cosViewAngle, out float3 attenuation,
	Texture3D<float4> texScatterLut, Texture2D<float4> texTransmittanceLut, 
	SamplerState texSampler)
{
	// If the view point is in space, we want to find the first intersection along the view dir with
	// the atmosphere.  We can then use that as the starting point and calculate the scattering within the atmosphere.
	float distToAtmosphereFromSpace = atmCalcDistanceToAtmosphereFromSpace(r, cosViewAngle);
	if (distToAtmosphereFromSpace > 0.f)
	{
		// In space and view intersects the atmosphere, adjust the view pos
		viewPos += viewDir * distToAtmosphereFromSpace;
		distToGround -= distToAtmosphereFromSpace;
		cosViewAngle = (r * cosViewAngle + distToGround) / cbAtmosphere.atmosphereRadius;
		r = cbAtmosphere.atmosphereRadius;
	}

	float3 result = 0;
	attenuation = 0;

	if (r <= cbAtmosphere.atmosphereRadius)
	{
		// View position is within the planet's atmosphere.
		float cosViewToSunAngle = dot(viewDir, sunDir);
		float cosSunAngle = dot(viewPos, sunDir) / r;

		// S|L| component of Equation 16 from Bruneton08
		float4 scatter = atmSampleScatter(texScatterLut, texSampler, r - cbAtmosphere.planetRadius, cosViewAngle, cosSunAngle);
		if (distToGround > 0.f)
		{
			float3 viewPos0 = viewPos + viewDir * distToGround;
			float r0 = length(viewPos0);
			float altitude0 = r0 - cbAtmosphere.planetRadius;
			float cosViewAngle0 = dot(viewPos0, viewDir) / r0;
			float cosSunAngle0 = dot(viewPos0, sunDir) / r0;

			// S|L| - T(x,x0) * S|L|x0 component of Equation 16 from Bruneton08
			attenuation = atmGetTransmittanceSegment(texTransmittanceLut, texSampler, altitude, cosViewAngle, viewDir, viewPos0);
			if (r0 > cbAtmosphere.planetRadius + 0.01f)
			{
				scatter -= attenuation.rgbr * atmSampleScatter(texScatterLut, texSampler, altitude0, cosViewAngle0, cosSunAngle0);
				scatter = max(scatter, 0);
			}
		}

		result = scatter.rgb * atmCalcPhaseRayleigh(cosViewToSunAngle) + atmExtractMie(scatter) * atmCalcPhaseMie(cosViewToSunAngle);
		result = max(result, 0);
	}
	else
	{
		// View pos is in space and the view direction does not intersect the planet
		result = float3(0, 0, 0);
	}

	return result * cbAtmosphere.sunColor;
}

// Ground reflectance component of equation 16 from Bruneton08
// distToGround MUST be >= 0.
// Note: This is only relevant if we're high above the planet's surface and representing it as a textured sphere with no detail geometry.
float3 atmCalcGroundColor(float3 viewPos, float3 viewDir, float distToGround, float3 sunDir, float3 attenuation,
	Texture2D<float4> texGroundReflectance, Texture2D<float4> texIrradianceLut, Texture2D<float4> texTransmittanceLut,
	SamplerState texSampler)
{
	// TODO: Clean this up and enable for internal usage.
	//		Right now it's basically a direct copy from the source code for Bruneton08.
	//		The reflectance texture is just a color texture for the planet with the a/w component being some sort of specular value.
	float3 result;
	// if ray hits ground surface
	// ground reflectance at end of ray, x0
	float3 x0 = viewPos + viewDir * distToGround;
	float r0 = length(x0);
	float3 n = x0 / r0;// viewPos0 / r0;
	float2 coords = float2(atan2(n.y, n.x), acos(n.z)) * float2(0.5, 1.0) / kPi + float2(0.5, 0.0);
	float4 reflectance = texGroundReflectance.Sample(texSampler, coords) * float4(0.2, 0.2, 0.2, 1.0);
	if (r0 > cbAtmosphere.planetRadius + 0.01f) 
	{
		reflectance = float4(0.4f, 0.4f, 0.4f, 0.f);
	}

	// direct sun light (radiance) reaching x0
	float cosSunAngle = dot(n, sunDir);
	float3 sunLight = atmGetTransmittanceWithShadow(texTransmittanceLut, texSampler, r0 - cbAtmosphere.planetRadius, cosSunAngle);

	// precomputed sky light (irradiance) (=E[L*]) at x0
	float3 groundSkyLight = atmSampleIrradiance(texIrradianceLut, texSampler, r0, cosSunAngle);

	// light reflected at x0 (=(R[L0]+R[L*])/T(x,x0))
	float3 groundColor = reflectance.rgb * (max(cosSunAngle, 0.0) * sunLight + groundSkyLight) * cbAtmosphere.sunColor / kPi;

	// water specular color due to sunLight
	if (reflectance.w > 0.0) 
	{
		float3 h = normalize(sunDir - viewDir);
		float fresnel = 0.02 + 0.98 * pow(1.0 - dot(-viewDir, h), 5.0);
		float waterBrdf = fresnel * pow(max(dot(h, n), 0.0), 150.0);
		groundColor += reflectance.w * max(waterBrdf, 0.0) * sunLight * cbAtmosphere.sunColor;
	}

	return attenuation * groundColor;
}

// Calculate the lighting for the sun disc.
// Equation 9 from Bruneton08
float3 atmCalcSunColor(float3 viewPos, float3 viewDir, float3 sunDir, float altitude, float cosViewAngle,
	Texture2D<float4> texTransmittanceLut, SamplerState texSampler)
{
	// Size the sun disc to cover 4 degrees in each direction of the planet's sphere.
	float sunRadiusAngle = 4.f * kDegToRad;
	float3 sunLight = step(cos(sunRadiusAngle), dot(viewDir, sunDir)) * cbAtmosphere.sunColor;

	// Attenuate the sun light by transmittance
	float3 transmittance = (altitude <= cbAtmosphere.atmosphereHeight)
		? atmGetTransmittanceWithShadow(texTransmittanceLut, texSampler, altitude, cosViewAngle)
		: float3(1, 1, 1);
	return transmittance * sunLight;
}

// Convert a standard world units view position into planet-relative kilometers.
float3 atmViewPosToPlanetPos(float3 viewPos)
{
	// todo: origin should be somewhere high above planetRadius to avoid negative altitude
	return viewPos * 0.001f + float3(0.f, cbAtmosphere.planetRadius, 0.f);
}