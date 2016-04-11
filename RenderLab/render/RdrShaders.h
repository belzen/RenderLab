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

enum RdrShaderType
{
	kRdrShaderType_Vertex,
	kRdrShaderType_Pixel,
	kRdrShaderType_Geometry,
	kRdrShaderType_Compute,

	kRdrShaderType_Count
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

typedef uint RdrShaderFlags;
enum RdrShaderFlag // todo: replace all flags with enum type
{
	kRdrShaderFlag_DepthOnly = 0x1,
	kRdrShaderFlag_CubemapCapture = 0x2,

	// Note: Need to switch m_vertexShaders over to a hash table if flag count gets too large.
	kRdrShaderFlag_NumCombos = 0x4
};

enum RdrVertexShaderType
{
	kRdrVertexShader_Model,
	kRdrVertexShader_Text,
	kRdrVertexShader_Sprite,
	kRdrVertexShader_Sky,
	kRdrVertexShader_Screen, // Pass through shader for geo in screen-space

	kRdrVertexShader_Count
};

struct RdrVertexShader
{
	RdrVertexShaderType eType;
	RdrShaderFlags flags;
};

enum RdrGeometryShaderType
{
	kRdrGeometryShader_Model_CubemapCapture,

	kRdrGeometryShader_Count
};

struct RdrGeometryShader
{
	RdrGeometryShaderType eType;
	RdrShaderFlags flags;
};

enum RdrComputeShader
{
	kRdrComputeShader_TiledDepthMinMax,
	kRdrComputeShader_TiledLightCull,
	kRdrComputeShader_LuminanceMeasure_First,
	kRdrComputeShader_LuminanceMeasure_Mid,
	kRdrComputeShader_LuminanceMeasure_Final,

	kRdrComputeShader_Count
};

