
struct TerrainLod
{
	// Min world pos and size of each LOD level.
	float2 minPos;
	float2 size;
};

struct DsTerrain
{
	TerrainLod lods[4];

	float2 heightmapTexelSize;
	float heightScale;
	float unused;
};
