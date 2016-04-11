#pragma once

struct ID3D11VertexShader;
struct ID3D11GeometryShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D10Blob;
struct ID3D11Device;
struct ID3D11InputLayout;

typedef uint16 RdrShaderHandle;
typedef uint16 RdrInputLayoutHandle;

struct RdrInputLayout
{
	ID3D11InputLayout* pInputLayout;
};

enum class RdrShaderStage
{
	Vertex,
	Pixel,
	Geometry,
	Compute,

	Count
};

struct RdrShader
{
	union
	{
		ID3D11VertexShader*   pVertex;
		ID3D11GeometryShader* pGeometry;
		ID3D11PixelShader*    pPixel;
		ID3D11ComputeShader*  pCompute;
		void*                 pTypeless;
	};

	const char* filename;

	const void* pVertexCompiledData;
	uint compiledSize;
};

enum class RdrShaderFlags : uint
{
	None = 0x0,
	DepthOnly = 0x1,
	CubemapCapture = 0x2,

	// Note: Need to switch m_vertexShaders over to a hash table if flag count gets too large.
	NumCombos = 0x4
};
ENUM_FLAGS(RdrShaderFlags);

enum class RdrVertexShaderType
{
	Model,
	Text,
	Sprite,
	Sky,
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

enum class RdrComputeShader
{
	TiledDepthMinMax,
	TiledLightCull,
	LuminanceMeasure_First,
	LuminanceMeasure_Mid,
	LuminanceMeasure_Final,

	Count
};

