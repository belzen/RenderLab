
#define FLT_MAX 3.402823466e+38F

cbuffer CalcParams : register(b0)
{
	float4x4 g_mtxInvProj;
};

Texture2DMS<float2> g_depthTex : register(t0);
RWTexture2D<float2> g_zMinMaxResults : register(u0);

groupshared float grp_zMin[64];
groupshared float grp_zMax[64];

#define GroupSizeX 8
#define GroupSizeY 8
#define ThreadCount GroupSizeX * GroupSizeY

// Each tile is 16x16 pixels, but each thread samples 4 pixels
[numthreads(GroupSizeX, GroupSizeY, 1)]
void main( uint3 dispatchId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex )
{
	uint2 samplePos = dispatchId.xy * 2;

	float z00 = g_depthTex.Load( uint2(samplePos.x, samplePos.y), 0 ).x;
	float z10 = g_depthTex.Load( uint2(samplePos.x + 1, samplePos.y), 0).x;
	float z01 = g_depthTex.Load( uint2(samplePos.x, samplePos.y + 1), 0).x;
	float z11 = g_depthTex.Load( uint2(samplePos.x + 1, samplePos.y + 1), 0).x;

	float zMin = min(z00, min(z10, min(z01, z11)));
	float zMax = max(z00, max(z10, max(z01, z11)));

	float4 vDepthMin = mul(float4(0.f, 0.f, zMin, 1.f), g_mtxInvProj);
	grp_zMin[localIdx] = vDepthMin.z / vDepthMin.w;

	float4 vDepthMax = mul(float4(0.f, 0.f, zMax, 1.f), g_mtxInvProj);
	grp_zMax[localIdx] = vDepthMin.z / vDepthMin.w;

	GroupMemoryBarrierWithGroupSync();

	[unroll(ThreadCount)]
	for (uint i = ThreadCount / 2; i > 0; i >>= 1)
	{
		if (localIdx < i)
		{
			grp_zMin[localIdx] = min(grp_zMin[localIdx], grp_zMin[localIdx + i]);
			grp_zMax[localIdx] = max(grp_zMax[localIdx], grp_zMax[localIdx + i]);
		}

		GroupMemoryBarrierWithGroupSync();
	}

	if (localIdx == 0)
	{
		g_zMinMaxResults[groupId.xy] = float2(grp_zMin[0], grp_zMax[0] + 1.f);
	}
}