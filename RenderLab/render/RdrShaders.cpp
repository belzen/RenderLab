#include "Precompiled.h"
#include "RdrShaders.h"
#include <assert.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "FileWatcher.h"
#include <fstream>
#include <string>

#pragma comment (lib, "d3dcompiler.lib")

namespace
{
	int s_debugShaders = 1;
	const char* s_shaderRootDir = "data/shaders/";
}

class IncludeHandler : public ID3DInclude
{
	HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
	{
		char filename[MAX_PATH];
		sprintf_s(filename, "%s/%s", s_shaderRootDir, pFileName);

		std::ifstream file(filename);
		std::string text;
		std::getline(file, text, (char)EOF);

		int size = text.length();
		char* data = new char[size+1];
		strcpy_s(data, size+1, text.c_str());

		*ppData = data;
		*pBytes = size;

		return S_OK;
	}

	HRESULT __stdcall Close(LPCVOID pData)
	{
		delete pData;
		return S_OK;
	}
};

static void shaderFileChangedHandler(const char* filename)
{

}

void InitShaderSystem(const char* path)
{
	FileWatcher::AddListener("*.hlsl", shaderFileChangedHandler);
}

static ID3D10Blob* CompileShader(const char* filename, const char* entrypoint, const char* shadermodel)
{
	HRESULT hr;
	ID3D10Blob* pCompiledData;
	ID3D10Blob* pErrors = nullptr;

	char fullFilename[MAX_PATH];
	sprintf_s(fullFilename, "%s/%s", s_shaderRootDir, filename);

	std::ifstream file(fullFilename);
	std::string shaderText;
	std::getline(file, shaderText, (char)EOF);

	uint flags = (s_debugShaders ? D3DCOMPILE_DEBUG : 0);

	IncludeHandler include;
	hr = D3DCompile(shaderText.c_str(), shaderText.size(), nullptr, nullptr, &include, entrypoint, shadermodel, flags, 0, &pCompiledData, &pErrors);
	if (hr != S_OK)
	{
		if (pErrors)
		{
			char errorMsg[2048];
			strcpy_s(errorMsg, sizeof(errorMsg), (char*)pErrors->GetBufferPointer());
			OutputDebugStringA(errorMsg);
			pErrors->Release();
		}

		if (pCompiledData)
			pCompiledData->Release();
	}

	return pCompiledData;
}

VertexShader VertexShader::Create(ID3D11Device* pDevice, const char* filename)
{
	VertexShader vs;
	vs.filename = _strdup(filename);
	vs.pCompiledData = CompileShader(filename, "main", "vs_5_0");

	HRESULT hr = pDevice->CreateVertexShader(vs.pCompiledData->GetBufferPointer(), vs.pCompiledData->GetBufferSize(), nullptr, &vs.pShader);
	assert(hr == S_OK);

	return vs;
}

PixelShader PixelShader::Create(ID3D11Device* pDevice, const char* filename)
{
	PixelShader ps;
	ps.filename = _strdup(filename);
	ps.pCompiledData = CompileShader(filename, "main", "ps_5_0");

	HRESULT hr = pDevice->CreatePixelShader(ps.pCompiledData->GetBufferPointer(), ps.pCompiledData->GetBufferSize(), nullptr, &ps.pShader);
	assert(hr == S_OK);

	return ps;
}

ComputeShader ComputeShader::Create(ID3D11Device* pDevice, const char* filename)
{
	ComputeShader cs;
	cs.filename = _strdup(filename);
	cs.pCompiledData = CompileShader(filename, "main", "cs_5_0");

	HRESULT hr = pDevice->CreateComputeShader(cs.pCompiledData->GetBufferPointer(), cs.pCompiledData->GetBufferSize(), nullptr, &cs.pShader);
	assert(hr == S_OK);

	return cs;
}
