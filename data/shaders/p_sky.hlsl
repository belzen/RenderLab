#include "v_output.hlsli"
#include "p_common.hlsli"
#include "atmosphere_inc.hlsli"

Texture3D<float4> g_texInscatterLut : register(t0);
Texture2D<float4> g_texTransmittanceLut : register(t1);
#if ENABLE_GROUND_REFLECTANCE
	Texture2D<float4> g_texIrradianceLut : register(t2);
	Texture2D<float4> g_texGroundReflectance : register(t3);
#endif

SamplerState g_sampler : register(s0);

/*
// Mie
// DEFAULT
static const float HM = 1.2;
static const float3 betaMSca = float3(4e-3, 4e-3, 4e-3);
static const float3 betaMEx = betaMSca / 0.9;
static const float mieG = 0.9;
// CLEAR SKY
const float HM = 1.2;
const vec3 betaMSca = vec3(20e-3);
const vec3 betaMEx = betaMSca / 0.9;
const float mieG = 0.76;*/
// PARTLY CLOUDY
/*const float HM = 3.0;
const vec3 betaMSca = vec3(3e-3);
const vec3 betaMEx = betaMSca / 0.9;
const float mieG = 0.65;*/

float4 main(VsOutputSky input) : SV_TARGET
{
	// todo2: origin should be somewhere high above planetRadius to avoid negative altitude
	float3 viewPos = cbPerAction.cameraPos * 0.001f + float3(0.f, cbAtmosphere.planetRadius, 0.f);
	float3 viewDir = normalize(input.position_ws.xyz - cbPerAction.cameraPos);

	float r = length(viewPos);
	float altitude = r - cbAtmosphere.planetRadius;
	float cosViewAngle = dot(viewPos, viewDir) / r;
	float distToGround = atmCalcDistanceToGround(r, cosViewAngle);

#if 0
	// Todo: Figure out what this is supposed to be doing.  This is taken from the source code for Bruneton08
	//	It's using 3 cone equations as the coefficients of the quadratic formula, presumably to find
	//	the intersection point of the view direction with the shadow altitude (Xs from the paper).
	//  However, the math doesn't seem to work out that way and there were no comments in the source
	//	explaining what this is supposed to be donig.
	const float kShadowAltitude = 10.f;
	float3 g = viewPos - float3(0.f, cbAtmosphere.planetRadius + kShadowAltitude, 0.f);
	float a = viewDir.x * viewDir.x + viewDir.z * viewDir.z - viewDir.y * viewDir.y;
	float b = 2.f * (g.x * viewDir.x + g.z * viewDir.z - g.y * viewDir.y);
	float c = g.x * g.x + g.z * g.z - g.y * g.y;
	float distToShadow = -(b + sqrt(b * b - 4.f * a * c)) / (2.f * a);
	bool cone = distToShadow > 0.f && abs(viewPos.y + distToShadow * viewDir.y - cbAtmosphere.planetRadius) <= 10.f;

	if (distToGround > 0.f)
	{
		// View direction intersects the planet surface
		if (cone && distToShadow < distToGround)
		{
			// View position is above the shadow altitude and intersects that circle
			// Adjust the view distance.
			distToGround = distToShadow;
		}
	}
	else if (cone) 
	{
		// View direction does not intersect the ground, but does intersect the shadow altitude.
		// Adjust the view distance.
		distToGround = distToShadow;
	}
#endif

	//////
	// Calculate the various components of equation 16 from Bruneton08:
	//  L0 + R|L0| + R|L*| + S|L|x - T(x, x0) * S|L|x0

	// Scattering along the view direction -- S|L|x - T(x, x0) * S|L|x0
	float3 attenuation;
	float3 inscatterColor = atmCalcInscatter(viewPos, viewDir, distToGround, -cbAtmosphere.sunDirection, 
		altitude, r, cosViewAngle, attenuation, g_texInscatterLut, g_texTransmittanceLut, g_sampler);

	float3 sunColor;
	float3 groundColor;
	if (distToGround > 0.f)
	{
		// View intersects the ground, so the sun isn't visible.
		sunColor = 0.f; 

		// Calculate light from ground reflectance -- R|L0| + R|L*|
		// This value is only applicable if viewing the planet from space or a high altitude where the ground
		//	is represented as a textured sphere with no geometry detail.
		#if ENABLE_GROUND_REFLECTANCE
			groundColor = atmCalcGroundColor(viewPos, viewDir, distToGround, -cbAtmosphere.sunDirection, attenuation,
				g_texGroundReflectance, g_texIrradianceLut, g_texTransmittanceLut, g_sampler);
		#else
			groundColor = 0.f;
		#endif
	}
	else
	{
		// View does not intersect the ground, so there's no ground color.
		groundColor = 0.f; 

		// Get the color of the sun disc -- L0
		sunColor = atmCalcSunColor(viewPos, viewDir, -cbAtmosphere.sunDirection, altitude, cosViewAngle,
			g_texTransmittanceLut, g_sampler);
	}
	
	// Combine the components of equation 16 to get the final atmosphere lighting.
	return float4(sunColor + groundColor + inscatterColor, 1.0);
}
