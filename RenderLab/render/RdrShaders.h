#pragma once

struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11ComputeShader;
struct ID3D10Blob;
struct ID3D11Device;
struct ID3D11InputLayout;

struct VertexShader
{
	static VertexShader Create(ID3D11Device* pDevice, const char* filename);

	ID3D11InputLayout* pInputLayout;
	ID3D11VertexShader* pShader;
	ID3D10Blob* pCompiledData;
	const char* filename;
};

struct PixelShader
{
	static PixelShader Create(ID3D11Device* pDevice, const char* filename);

	ID3D11PixelShader* pShader;
	ID3D10Blob* pCompiledData;
	const char* filename;
};

struct ComputeShader
{
	static ComputeShader Create(ID3D11Device* pDevice, const char* filename);

	ID3D11ComputeShader* pShader;
	ID3D10Blob* pCompiledData;
	const char* filename;
};
