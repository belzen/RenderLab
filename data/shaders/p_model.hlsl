#include "p_common.hlsli"
#include "light_inc.hlsli"

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

Texture2D texDiffuse : register(t0);
SamplerState sampDiffuse : register(s0);

Texture2D texNormals : register(t1);
SamplerState sampNormals : register(s1);

float4 main(PixelInput input) : SV_TARGET
{
	float4 color = texDiffuse.Sample(sampDiffuse, input.texcoords) + input.color;

	float3 normal = texNormals.Sample(sampNormals, input.texcoords).xyz;
	normal = (normal * 2.f) - 1.f; // Expand normal range

	// Transform normal into world space.
	normal = (normal.x * input.tangent) + (normal.y * input.bitangent) + (normal.z * input.normal);
	normal = normalize(normal);

	float3 cameraViewDir = normalize(cbPerAction.viewPos - input.position_ws.xyz);

	float3 litColor = doLighting(input.position_ws, color.rgb, normal, cameraViewDir, input.position.xy, cbPerAction.viewWidth);
	return float4(litColor, 1);
}