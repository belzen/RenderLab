#include "Precompiled.h"
#include "RdrContextD3D12.h"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <string>

#pragma comment (lib, "d3dcompiler.lib")

namespace
{
	ID3D10Blob* compileShaderD3D(const char* pShaderText, uint textLen, const char* entrypoint, const char* shadermodel)
	{
		HRESULT hr;
		ID3D10Blob* pCompiledData;
		ID3D10Blob* pErrors = nullptr;

		uint flags = (g_userConfig.debugShaders ? D3DCOMPILE_DEBUG : D3DCOMPILE_OPTIMIZATION_LEVEL3);

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
			"POSITION", // RdrShaderSemantic::Position
			"TEXCOORD", // RdrShaderSemantic::Texcoord
			"COLOR",    // RdrShaderSemantic::Color
			"NORMAL",   // RdrShaderSemantic::Normal
			"BINORMAL", // RdrShaderSemantic::Binormal
			"TANGENT"   // RdrShaderSemantic::Tangent
		};
		static_assert(ARRAY_SIZE(s_d3dSemantics) == (int)RdrShaderSemantic::Count, "Missing D3D12 shader semantic!");
		return s_d3dSemantics[(int)eSemantic];
	}

	D3D12_INPUT_CLASSIFICATION getD3DVertexInputClass(RdrVertexInputClass eClass)
	{
		static const D3D12_INPUT_CLASSIFICATION s_d3dClasses[] = {
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,		// RdrVertexInputClass::PerVertex
			D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA	// RdrVertexInputClass::PerInstance
		};
		static_assert(ARRAY_SIZE(s_d3dClasses) == (int)RdrVertexInputClass::Count, "Missing D3D12 vertex input class!");
		return s_d3dClasses[(int)eClass];
	}

	DXGI_FORMAT getD3DVertexInputFormat(RdrVertexInputFormat eFormat)
	{
		static const DXGI_FORMAT s_d3dFormats[] = {
			DXGI_FORMAT_R32_FLOAT,			// RdrVertexInputFormat::R_F32
			DXGI_FORMAT_R32G32_FLOAT,		// RdrVertexInputFormat::RG_F32
			DXGI_FORMAT_R32G32B32_FLOAT,	// RdrVertexInputFormat::RGB_F32
			DXGI_FORMAT_R32G32B32A32_FLOAT	// RdrVertexInputFormat::RGBA_F32
		};
		static_assert(ARRAY_SIZE(s_d3dFormats) == (int)RdrVertexInputFormat::Count, "Missing D3D12 vertex input format!");
		return s_d3dFormats[(int)eFormat];
	}
}

bool RdrContextD3D12::CompileShader(RdrShaderStage eType, const char* pShaderText, uint textLen, void** ppOutCompiledData, uint* pOutDataSize)
{
	static const char* s_d3dShaderModels[] = {
		"vs_5_0", // RdrShaderStage::Vertex
		"ps_5_0", // RdrShaderStage::Pixel
		"gs_5_0", // RdrShaderStage::Geometry
		"hs_5_0", // RdrShaderStage::Hull
		"ds_5_0", // RdrShaderStage::Domain
		"cs_5_0"  // RdrShaderStage::Compute
	};
	static_assert(ARRAY_SIZE(s_d3dShaderModels) == (int)RdrShaderStage::Count, "Missing D3D shader models!");

	ID3D10Blob* pCompiledData = compileShaderD3D(pShaderText, textLen, "main", s_d3dShaderModels[(int)eType]);
	if (!pCompiledData)
		return false;

	*ppOutCompiledData = new char[pCompiledData->GetBufferSize()];
	memcpy(*ppOutCompiledData, pCompiledData->GetBufferPointer(), pCompiledData->GetBufferSize());
	*pOutDataSize = (uint)pCompiledData->GetBufferSize();

	pCompiledData->Release();
	return true;
}

void* RdrContextD3D12::CreateShader(RdrShaderStage eStage, const void* pShaderText, uint textLen)
{
	HRESULT hr;

	void* pOutShader = nullptr;

	switch (eStage)
	{
	case RdrShaderStage::Vertex:
		hr = m_pDevice->CreateVertexShader(pShaderText, textLen, nullptr, (ID3D11VertexShader**)&pOutShader);
		break;
	case RdrShaderStage::Pixel:
		hr = m_pDevice->CreatePixelShader(pShaderText, textLen, nullptr, (ID3D11PixelShader**)&pOutShader);
		break;
	case RdrShaderStage::Geometry:
		hr = m_pDevice->CreateGeometryShader(pShaderText, textLen, nullptr, (ID3D11GeometryShader**)&pOutShader);
		break;
	case RdrShaderStage::Compute:
		hr = m_pDevice->CreateComputeShader(pShaderText, textLen, nullptr, (ID3D11ComputeShader**)&pOutShader);
		break;
	case RdrShaderStage::Hull:
		hr = m_pDevice->CreateHullShader(pShaderText, textLen, nullptr, (ID3D11HullShader**)&pOutShader);
		break;
	case RdrShaderStage::Domain:
		hr = m_pDevice->CreateDomainShader(pShaderText, textLen, nullptr, (ID3D11DomainShader**)&pOutShader);
		break;
	default:
		assert(false);
	}

	assert(hr == S_OK);
	return pOutShader;
}

void RdrContextD3D12::ReleaseShader(RdrShader* pShader)
{
	switch (pShader->eStage)
	{
	case RdrShaderStage::Vertex:
		pShader->pVertex->Release();
		break;
	case RdrShaderStage::Pixel:
		pShader->pPixel->Release();
		break;
	case RdrShaderStage::Geometry:
		pShader->pGeometry->Release();
		break;
	case RdrShaderStage::Compute:
		pShader->pCompute->Release();
		break;
	default:
		assert(false);
	}
}

RdrInputLayout RdrContextD3D12::CreateInputLayout(const void* pCompiledVertexShader, uint vertexShaderSize, const RdrVertexInputElement* aVertexElements, uint numElements)
{
	RdrInputLayout layout;

	D3D12_INPUT_ELEMENT_DESC d3dVertexDesc[32];
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

	ID3D12InputLayout* pLayout = nullptr;
	HRESULT hr = m_pDevice->CreateInputLayout(d3dVertexDesc, numElements,
		pCompiledVertexShader, vertexShaderSize,
		&layout.pInputLayout);
	assert(hr == S_OK);

	return layout;
}
