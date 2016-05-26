
struct Partitions
{
	float zNear;
	float zFar;
};

Texture2D<float2> g_zMinMaxTiles : register(t0);

RWStructuredBuffer<Partitions> g_partitions : register(u0);

#define GroupSizeX 16
#define GroupSizeY 16
#define ThreadCount (GroupSizeX * GroupSizeY)

groupshared float2 grp_zMinMax[GroupSizeX * GroupSizeY];

[numthreads(GroupSizeX, GroupSizeY, 1)]
void main(uint3 globalId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex)
{
	uint2 tileDimensions;
	g_zMinMaxTiles.GetDimensions(tileDimensions.x, tileDimensions.y);

	float2 zMinMax = float2(1000000.f, -1.f);

	for (uint x = globalId.x; x < tileDimensions.x; x += GroupSizeX)
	{
		for (uint y = globalId.y; y < tileDimensions.y; y += GroupSizeY)
		{
			float2 tileDepth = g_zMinMaxTiles[uint2(x, y)];
			zMinMax.x = min(zMinMax.x, tileDepth.x);
			zMinMax.y = max(zMinMax.y, tileDepth.y);
		}
	}

	grp_zMinMax[localIdx] = zMinMax;
	GroupMemoryBarrierWithGroupSync();

	for (uint i = ThreadCount / 2; i > 0; i >>= 2)
	{
		if (localIdx < i)
		{
			grp_zMinMax[localIdx].x = min(grp_zMinMax[localIdx].x, grp_zMinMax[localIdx + i].x);
			grp_zMinMax[localIdx].y = max(grp_zMinMax[localIdx].y, grp_zMinMax[localIdx + i].y);
		}

		GroupMemoryBarrierWithGroupSync();
	}

	if (localIdx == 0)
	{
		uint numPartitions;
		uint stride;
		g_partitions.GetDimensions(numPartitions, stride);

		float zDiff = zMinMax.y - zMinMax.x;


		g_partitions[0].zNear = zMinMax.x;

		//float log = 


		g_partitions[numPartitions - 1].zFar = zMinMax.y;
	}
}