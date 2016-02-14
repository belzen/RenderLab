#include "light_inc.hlsli"

cbuffer PerFrame
{
	float4x4 invProjMat;
	float3 cameraPosition;
	uint screenWidth;
};

static float4 ambient_color = float4(1.f, 1.f, 1.f, 1.f);
static float ambient_intensity = 0.1f;

struct PixelInput
{
	float4 position : SV_POSITION;
	float3 position_ws : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
	float3 tangent : TANGENT;
	float3 bitangent : BINORMAL;
};

Texture2D diffuseTex : register(t0);
SamplerState diffuseTexSampler;

Texture2D normalsTex : register(t1);
SamplerState normalsTexSampler;

float4 main(PixelInput input) : SV_TARGET
{
	float4 color = diffuseTex.Sample(diffuseTexSampler, input.texcoords) + input.color;
	float4 ambient = color * ambient_color * ambient_intensity;

	float3 normal = normalsTex.Sample(normalsTexSampler, input.texcoords).xyz;
	normal = (normal * 2.f) - 1.f; // Expand normal range

	// Transform normal into world space.
	normal = (normal.x * input.tangent) + (normal.y * input.bitangent) + (normal.z * input.normal);
	normal = normalize(normal);

	float3 cameraViewDir = normalize(cameraPosition - input.position_ws.xyz);

	float4 litColor = doLighting(input.position_ws, color, normal, cameraViewDir, input.position.xy, screenWidth);
	return saturate(ambient + litColor);
}