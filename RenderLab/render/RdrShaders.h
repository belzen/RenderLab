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

