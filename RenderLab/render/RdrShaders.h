#pragma once

struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D10Blob;
struct ID3D11Device;
struct ID3D11InputLayout;

typedef uint RdrShaderHandle;

struct RdrVertexShader
{
	ID3D11InputLayout* pInputLayout;
	ID3D11VertexShader* pShader;
	ID3D10Blob* pCompiledData;
	const char* filename;
};

struct RdrPixelShader
{
	ID3D11PixelShader* pShader;
	ID3D10Blob* pCompiledData;
	const char* filename;
};

struct RdrComputeShader
{
	ID3D11ComputeShader* pShader;
	ID3D10Blob* pCompiledData;
	const char* filename;
};
