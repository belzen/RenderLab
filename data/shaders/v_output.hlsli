
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

struct VsOutputDecal
{
	float4 position : SV_POSITION;
	float4 position_vs : POSITION_VIEW;
	float4 position_cs : POSITION_CLIP;
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

struct VsOutputTerrain
{
	float4 position_ws : POSITION;
	float lod : TEXCOORD0;
};

struct VsOutputOcean
{
	float4 position : SV_POSITION;
	float4 position_ws : POSITION;
	float3 normal : NORMAL;
	float color_mod : color_mod;
};

struct VsOutputWater
{
	float4 position : SV_POSITION;
	float4 position_ws : POSITION;
	float2 texcoord : TEXCOORD0;
};

// Hull/Domain outputs
struct HsOutputTerrain
{
	float4 position_ws : POSITION;
	float lod : TEXCOORD0;
};

struct HsPatchConstants
{
	float edgeTessFactor[4] : SV_TessFactor;
	float insideTessFactors[2] : SV_InsideTessFactor;
};

struct DsOutputTerrain
{
	float4 position : SV_POSITION;
	float4 position_ws : POSITION;

#if !DEPTH_ONLY
	float3 normal : NORMAL;
	float2 texcoords : TEXCOORD0;
#endif
};

