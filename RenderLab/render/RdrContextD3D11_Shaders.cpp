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

	ID3D10Blob* compileShaderD3D(const char* pShaderText, uint textLen, const char* entrypoint, const char* shadermodel)
	{
		HRESULT hr;
		ID3D10Blob* pCompiledData;
		ID3D10Blob* pErrors = nullptr;

		uint flags = (s_debugShaders ? D3DCOMPILE_DEBUG : 0);

		hr = D3DCompile(pShaderText, textLen, nullptr, nullptr, nullptr, entrypoint, shadermodel, flags, 0, &pCompiledData, &pErrors);
		if (hr != S_OK)
		{
			if (pCompiledData)
				pCompiledData->Release();
		}

		if (pErrors)
		{
			char errorMsg[2048];
			strcpy_s(errorMsg, sizeof(errorMsg), (char*)pErrors->GetBufferPointer());
			OutputDebugStringA(errorMsg);
			pErrors->Release();
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

bool RdrContextD3D11::CompileShader(RdrShaderType eType, const char* pShaderText, uint textLen, void** ppOutCompiledData, uint* pOutDataSize)
{
	static const char* s_d3dShaderModels[] = {
		"vs_5_0", // kRdrShaderType_Vertex
		"ps_5_0", // kRdrShaderType_Pixel
		"gs_5_0", // kRdrShaderType_Geometry
		"cs_5_0"  // kRdrShaderType_Compute
	};
	static_assert(ARRAYSIZE(s_d3dShaderModels) == kRdrShaderType_Count, "Missing D3D shader models!");

	ID3D10Blob* pCompiledData = compileShaderD3D(pShaderText, textLen, "main", s_d3dShaderModels[eType]);
	if (!pCompiledData)
		return false;
	
	*ppOutCompiledData = new char[pCompiledData->GetBufferSize()];
	memcpy(*ppOutCompiledData, pCompiledData->GetBufferPointer(), pCompiledData->GetBufferSize());
	*pOutDataSize = pCompiledData->GetBufferSize();

	pCompiledData->Release();
	return true;
}

void* RdrContextD3D11::CreateShader(RdrShaderType eType, const void* pShaderText, uint textLen)
{
	HRESULT hr;
	
	void* pOutShader = nullptr;

	switch (eType)
	{
	case kRdrShaderType_Vertex:
		hr = m_pDevice->CreateVertexShader(pShaderText, textLen, nullptr, (ID3D11VertexShader**)&pOutShader);
		break;
	case kRdrShaderType_Pixel:
		hr = m_pDevice->CreatePixelShader(pShaderText, textLen, nullptr, (ID3D11PixelShader**)&pOutShader);
		break;
	case kRdrShaderType_Geometry:
		hr = m_pDevice->CreateGeometryShader(pShaderText, textLen, nullptr, (ID3D11GeometryShader**)&pOutShader);
		break;
	case kRdrShaderType_Compute:
		hr = m_pDevice->CreateComputeShader(pShaderText, textLen, nullptr, (ID3D11ComputeShader**)&pOutShader);
		break;
	default:
		assert(false);
	}

	assert(hr == S_OK);
	return pOutShader;
}

RdrInputLayout RdrContextD3D11::CreateInputLayout(const void* pCompiledVertexShader, uint vertexShaderSize, const RdrVertexInputElement* aVertexElements, uint numElements)
{
	RdrInputLayout layout;

	D3D11_INPUT_ELEMENT_DESC d3dVertexDesc[32];
	for (uint i = 0; i < numElements; ++i)
	{
		d3dVertexDesc[i].SemanticName = getD3DSemanticName(aVertexElements[i].semantic);
		d3dVertexDesc[i].SemanticIndex = aVertexElements[i].semanticIndex;
		d3dVertexDesc[i].AlignedByteOffset = aVertexElements[i].byteOffset;
		d3dVertexDesc[i].Format = getD3DVertexInputFormat(aVertexElements[i].format);
		d3dVertexDesc[i].InputSlotClass = getD3DVertexInputClass(aVertexElements[i].inputClass);
		d3dVertexDesc[i].InputSlot = aVertexElements[i].streamIndex;
		d3dVertexDesc[i].InstanceDataStepRate = aVertexElements[i].instanceDataStepRate;
	}

	ID3D11InputLayout* pLayout = nullptr;
	HRESULT hr = m_pDevice->CreateInputLayout(d3dVertexDesc, numElements,
		pCompiledVertexShader, vertexShaderSize,
		&layout.pInputLayout);
	assert(hr == S_OK);

	return layout;
}
