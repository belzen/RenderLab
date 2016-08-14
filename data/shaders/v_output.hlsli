
struct VsOutputModel
{
	float4 position : SV_POSITION;
	float4 position_ws : POSITION;

#if !DEPTH_ONLY
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
	float3 tangent : TANGENT;
	float3 bitangent : BINORMAL;
#elif ALPHA_CUTOUT
	float2 texcoords : TEXCOORD0;
#endif
};

struct VsOutputSky
{
	float4 position : SV_POSITION;
	float4 position_ws : POSITION;
	float3 direction : TEXCOORD0;
};

struct VsOutputSprite
{
	float4 position : SV_POSITION;
	float2 texcoords : TEXCOORD0;
	float alpha : TEXCOORD1;
};