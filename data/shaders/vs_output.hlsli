
struct VSOutput
{
	float4 position : SV_POSITION;
	float4 position_ws : POSITION;

#if !DEPTH_ONLY
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
	float3 tangent : TANGENT;
	float3 bitangent : BINORMAL;
#endif
};
