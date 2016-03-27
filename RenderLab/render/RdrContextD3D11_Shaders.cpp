#include "Precompiled.h"
#include "RdrContextD3D11.h"
#include <assert.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <fstream>
#include <string>

#pragma comment (lib, "d3dcompiler.lib")

namespace
{
	int s_debugShaders = 1;
	const char* s_shaderRootDir = "data/shaders/";

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
			char* data = new char[size + 1];
			strcpy_s(data, size + 1, text.c_str());

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

	ID3D10Blob* CompileShader(const char* filename, const char* entrypoint, const char* shadermodel)
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

	const char* getD3DSemanticName(RdrShaderSemantic eSemantic)
	{
		static const char* s_d3dSemantics[] = {
			"POSITION", // kRdrShaderSemantic_Position
			"TEXCOORD", // kRdrShaderSemantic_Texcoord
			"COLOR",    // kRdrShaderSemantic_Color
			"NORMAL",   // kRdrShaderSemantic_Normal
			"BINORMAL", // kRdrShaderSemantic_Binormal
			"TANGENT"   // kRdrShaderSemantic_Tangent
		};
		static_assert(ARRAYSIZE(s_d3dSemantics) == kRdrShaderSemantic_Count, "Missing D3D11 shader semantic!");
		return s_d3dSemantics[eSemantic];
	}

	D3D11_INPUT_CLASSIFICATION getD3DVertexInputClass(RdrVertexInputClass eClass)
	{
		static const D3D11_INPUT_CLASSIFICATION s_d3dClasses[] = {
			D3D11_INPUT_PER_VERTEX_DATA, // kRdrVertexInputClass_PerVertex
			D3D11_INPUT_PER_INSTANCE_DATA// kRdrVertexInputClass_PerInstance
		};
		static_assert(ARRAYSIZE(s_d3dClasses) == kRdrVertexInputClass_Count, "Missing D3D11 vertex input class!");
		return s_d3dClasses[eClass];
	}

	DXGI_FORMAT getD3DVertexInputFormat(RdrVertexInputFormat eFormat)
	{
		static const DXGI_FORMAT s_d3dFormats[] = {
			DXGI_FORMAT_R32G32_FLOAT,      // kRdrVertexInputFormat_RG_F32
			DXGI_FORMAT_R32G32B32_FLOAT,   // kRdrVertexInputFormat_RGB_F32
			DXGI_FORMAT_R32G32B32A32_FLOAT // kRdrVertexInputFormat_RGBA_F32
		};
		static_assert(ARRAYSIZE(s_d3dFormats) == kRdrVertexInputFormat_Count, "Missing D3D11 vertex input format!");
		return s_d3dFormats[eFormat];
	}
}

RdrShaderHandle RdrContextD3D11::LoadVertexShader(const char* filename, RdrVertexInputElement* inputDesc, uint numElements)
{
	// todo: key vertex shader off of hash of (filename + input desc)
	RdrShaderMap::iterator iter = m_vertexShaderCache.find(filename);
	if (iter != m_vertexShaderCache.end())
		return iter->second;

	RdrVertexShader* pShader = m_vertexShaders.alloc();

	pShader->filename = _strdup(filename);
	pShader->pCompiledData = CompileShader(filename, "main", "vs_5_0");

	HRESULT hr = m_pDevice->CreateVertexShader(pShader->pCompiledData->GetBufferPointer(), pShader->pCompiledData->GetBufferSize(), nullptr, &pShader->pShader);
	assert(hr == S_OK);

	{
		D3D11_INPUT_ELEMENT_DESC d3dVertexDesc[32];
		for (uint i = 0; i < numElements; ++i)
		{
			d3dVertexDesc[i].SemanticName = getD3DSemanticName(inputDesc[i].semantic);
			d3dVertexDesc[i].SemanticIndex = inputDesc[i].semanticIndex;
			d3dVertexDesc[i].AlignedByteOffset = inputDesc[i].byteOffset;
			d3dVertexDesc[i].Format = getD3DVertexInputFormat(inputDesc[i].format);
			d3dVertexDesc[i].InputSlotClass = getD3DVertexInputClass(inputDesc[i].inputClass);
			d3dVertexDesc[i].InputSlot = inputDesc[i].streamIndex;
			d3dVertexDesc[i].InstanceDataStepRate = inputDesc[i].instanceDataStepRate;
		}

		hr = m_pDevice->CreateInputLayout(d3dVertexDesc, numElements,
			pShader->pCompiledData->GetBufferPointer(), pShader->pCompiledData->GetBufferSize(),
			&pShader->pInputLayout);
		assert(hr == S_OK);
	}

	RdrShaderHandle hShader = m_vertexShaders.getId(pShader);
	m_vertexShaderCache.insert(std::make_pair(filename, hShader));
	return hShader;
}

RdrShaderHandle RdrContextD3D11::LoadPixelShader(const char* filename)
{
	RdrShaderMap::iterator iter = m_pixelShaderCache.find(filename);
	if (iter != m_pixelShaderCache.end())
		return iter->second;

	RdrPixelShader* pShader = m_pixelShaders.alloc();
	pShader->filename = _strdup(filename);
	pShader->pCompiledData = CompileShader(filename, "main", "ps_5_0");

	HRESULT hr = m_pDevice->CreatePixelShader(pShader->pCompiledData->GetBufferPointer(), pShader->pCompiledData->GetBufferSize(), nullptr, &pShader->pShader);
	assert(hr == S_OK);

	RdrShaderHandle hShader = m_pixelShaders.getId(pShader);
	m_pixelShaderCache.insert(std::make_pair(filename, hShader));
	return hShader;
}

RdrShaderHandle RdrContextD3D11::LoadComputeShader(const char* filename)
{
	RdrShaderMap::iterator iter = m_computeShaderCache.find(filename);
	if (iter != m_computeShaderCache.end())
		return iter->second;

	RdrComputeShader* pShader = m_computeShaders.alloc();
	pShader->filename = _strdup(filename);
	pShader->pCompiledData = CompileShader(filename, "main", "cs_5_0");

	HRESULT hr = m_pDevice->CreateComputeShader(pShader->pCompiledData->GetBufferPointer(), pShader->pCompiledData->GetBufferSize(), nullptr, &pShader->pShader);
	assert(hr == S_OK);

	RdrShaderHandle hShader = m_computeShaders.getId(pShader);
	m_computeShaderCache.insert(std::make_pair(filename, hShader));
	return hShader;
}
