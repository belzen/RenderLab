#pragma once

struct ID3D11VertexShader;
struct ID3D11GeometryShader;
struct ID3D11HullShader;
struct ID3D11DomainShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D10Blob;
struct ID3D11Device;
struct ID3D11InputLayout;

struct RdrInputLayout
{
	ID3D11InputLayout* pInputLayout;
};

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

struct RdrShader
{
	union
	{
		ID3D11VertexShader*		pVertex;
		ID3D11GeometryShader*	pGeometry;
		ID3D11HullShader*		pHull;
		ID3D11DomainShader*		pDomain;
		ID3D11PixelShader*		pPixel;
		ID3D11ComputeShader*	pCompute;
		void*					pTypeless;
	};

	const char* filename;

	const void* pVertexCompiledData;
	uint compiledSize;

	RdrShaderStage eStage;
};

enum class RdrShaderFlags : uint
{
	None			= 0,
	DepthOnly		= 1 << 1,
	CubemapCapture	= 1 << 2,
	AlphaCutout		= 1 << 3,
	IsInstanced		= 1 << 4,

	// Note: Need to switch m_vertexShaders over to a hash table if flag count gets too large.
	NumCombos		= 1 << 5
};
ENUM_FLAGS(RdrShaderFlags);

enum class RdrVertexShaderType
{
	Model,
	Text,
	Sprite,
	Sky,
	Terrain,
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
typedef RdrShaderList::Handle RdrShaderHandle;

typedef FreeList<RdrInputLayout, 1024> RdrInputLayoutList;
typedef RdrInputLayoutList::Handle RdrInputLayoutHandle;
