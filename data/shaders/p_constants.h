
struct PsPerAction
{
	float4x4 mtxInvProj;
	float3 viewPos;
	uint viewWidth;
};

struct ToneMapParams
{
	float linearExposure;
	float white;
	float2 unused;
};