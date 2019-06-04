#pragma once

#include <set>

struct ID3D10Blob;
struct RdrPipelineState;

enum class RdrShaderStage
{
	Vertex,
	Pixel,
	Geometry,
	Hull,
	Domain,
	Compute,

	Count
};

enum class RdrShaderStageFlags
{
	None		= 0,
	Vertex		= 1 << (int)RdrShaderStage::Vertex,
	Pixel		= 1 << (int)RdrShaderStage::Pixel,
	Geometry	= 1 << (int)RdrShaderStage::Geometry,
	Hull		= 1 << (int)RdrShaderStage::Hull,
	Domain		= 1 << (int)RdrShaderStage::Domain,
	Compute		= 1 << (int)RdrShaderStage::Compute,
};
ENUM_FLAGS(RdrShaderStageFlags);

struct RdrShader
{
	const char* filename;

	void* pCompiledData;
	uint compiledSize;

	RdrShaderStage eStage;

	std::vector<const char*> defines;
	std::set<const RdrPipelineState*> refPipelineStates;
};

enum class RdrShaderFlags : uint
{
	None			= 0,
	DepthOnly		= 1 << 1,
	AlphaCutout		= 1 << 2,
	IsInstanced		= 1 << 3,

	// Note: Need to switch m_vertexShaders over to a hash table if flag count gets too large.
	NumCombos		= 1 << 5
};
ENUM_FLAGS(RdrShaderFlags);

enum class RdrVertexShaderType
{
	Model,
	Decal,
	Text,
	Sprite,
	Sky,
	Terrain,
	Ocean,
	Screen, // Pass through shader for geo in screen-space

	Count
};

struct RdrVertexShader
{
	RdrVertexShaderType eType;
	RdrShaderFlags flags;
};

enum class RdrGeometryShaderType
{
	Model_CubemapCapture,

	Count
};

struct RdrGeometryShader
{
	RdrGeometryShaderType eType;
	RdrShaderFlags flags;
};

enum class RdrTessellationShaderType
{
	None,
	Terrain,
	Count
};

struct RdrTessellationShader
{
	RdrTessellationShaderType eType;
	RdrShaderFlags flags;
};

enum class RdrComputeShader
{
	TiledDepthMinMax,
	TiledLightCull,

	ClusteredLightCull,

	LuminanceMeasure_First,
	LuminanceMeasure_Mid,
	LuminanceMeasure_Final,

	LuminanceHistogram_Tile,
	LuminanceHistogram_Merge,
	LuminanceHistogram_ResponseCurve,

	BloomShrink,
	Shrink,
	Blend2d,
	Add2d,
	Add3d,
	BlurHorizontal,
	BlurVertical,
	Clear2d,
	Clear3d,
	Copy2d,
	Copy3d,

	AtmosphereTransmittanceLut,
	AtmosphereScatterLut_Single,
	AtmosphereScatterLut_N,
	AtmosphereIrradianceLut_Initial,
	AtmosphereIrradianceLut_N,
	AtmosphereIrradianceLut_N_CombinedScatter,
	AtmosphereRadianceLut,
	AtmosphereRadianceLut_CombinedScatter,

	VolumetricFog_Light,
	VolumetricFog_Accum,

	Count
};


//////////////////////////////////////////////////////////////////////////

typedef FreeList<RdrShader, 1024> RdrShaderList;
