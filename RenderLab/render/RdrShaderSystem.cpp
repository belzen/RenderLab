#include "Precompiled.h"
#include "RdrShaderSystem.h"
#include "RdrContext.h"
#include <d3dcompiler.h>
#include <fstream>
#include <string>

namespace
{
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

			int size = (int)text.length();
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

	ID3D10Blob* preprocessShader(const char* filename)
	{
		HRESULT hr;
		ID3D10Blob* pCompiledData;
		ID3D10Blob* pErrors = nullptr;

		char fullFilename[MAX_PATH];
		sprintf_s(fullFilename, "%s/%s", s_shaderRootDir, filename);

		std::ifstream file(fullFilename);
		std::string shaderText;
		std::getline(file, shaderText, (char)EOF);

		IncludeHandler include;
		hr = D3DPreprocess(shaderText.c_str(), shaderText.size(), nullptr, nullptr, &include, &pCompiledData, &pErrors);
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
}

void RdrShaderSystem::Init(RdrContext* pRdrContext)
{
	m_pRdrContext = pRdrContext;
}

RdrShaderHandle RdrShaderSystem::CreateShaderFromFile(RdrShaderType eType, const char* filename)
{
	RdrShaderList& shaders = m_shaders[eType];
	RdrShaderHandleMap& shaderCache = m_shaderCaches[eType];

	// Find shader in cache
	RdrShaderHandleMap::iterator iter = shaderCache.find(filename);
	if (iter != shaderCache.end())
		return iter->second;

	ID3D10Blob* pBlob = preprocessShader(filename);
	if (pBlob)
	{
		RdrShader* pShader = shaders.alloc();
		pShader->filename = filename;

		CmdCreateShader cmd;
		cmd.hShader = shaders.getId(pShader);
		cmd.textLen = (uint)pBlob->GetBufferSize();
		cmd.pShaderText = new char[cmd.textLen];
		memcpy(cmd.pShaderText, pBlob->GetBufferPointer(), cmd.textLen);
		cmd.eType = eType;

		m_states[m_queueState].shaderCreates.push_back(cmd);
		pBlob->Release();

		shaderCache.insert(std::make_pair(filename, cmd.hShader));
		return cmd.hShader;
	}
	else
	{
		return 0;
	}
}

RdrInputLayoutHandle RdrShaderSystem::CreateInputLayout(RdrShaderHandle hVertexShader, RdrVertexInputElement* aVertexElements, uint numElements)
{
	RdrInputLayout* pLayout = m_inputLayouts.alloc();

	CmdCreateInputLayout cmd;
	cmd.hLayout = m_inputLayouts.getId(pLayout);
	cmd.hVertexShader = hVertexShader;
	cmd.hLayout = m_inputLayouts.getId(pLayout);
	cmd.numElements = numElements;
	memcpy(cmd.vertexElements, aVertexElements, sizeof(RdrVertexInputElement) * numElements);

	m_states[m_queueState].layoutCreates.push_back(cmd);

	return cmd.hLayout;
}

const RdrShader* RdrShaderSystem::GetShader(RdrShaderType eType, RdrShaderHandle hShader)
{
	return m_shaders[eType].get(hShader);
}

const RdrInputLayout* RdrShaderSystem::GetInputLayout(RdrInputLayoutHandle hLayout)
{
	return m_inputLayouts.get(hLayout);
}

void RdrShaderSystem::FlipState()
{
	m_queueState = !m_queueState;
}

void RdrShaderSystem::ProcessCommands()
{
	FrameState& state = m_states[!m_queueState];

	// Shader creates
	uint numCmds = (uint)state.shaderCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateShader& cmd = state.shaderCreates[i];
		RdrShader* pShader = m_shaders[cmd.eType].get(cmd.hShader);

		void* pCompiledData;
		uint compiledDataSize;

		bool res = m_pRdrContext->CompileShader(cmd.eType, cmd.pShaderText, cmd.textLen, &pCompiledData, &compiledDataSize);
		assert(res);

		pShader->pTypeless = m_pRdrContext->CreateShader(cmd.eType, pCompiledData, compiledDataSize);
		if (cmd.eType == kRdrShaderType_Vertex)
		{
			pShader->pVertexCompiledData = pCompiledData;
			pShader->compiledSize = compiledDataSize;
		}
		else
		{
			delete pCompiledData;
		}
	}

	numCmds = (uint)state.layoutCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateInputLayout& cmd = state.layoutCreates[i];
		RdrInputLayout* pLayout = m_inputLayouts.get(cmd.hLayout);
		RdrShader* pShader = m_shaders[kRdrShaderType_Vertex].get(cmd.hVertexShader);

		*pLayout = m_pRdrContext->CreateInputLayout(pShader->pVertexCompiledData, pShader->compiledSize, cmd.vertexElements, cmd.numElements);
	}

	state.layoutCreates.clear();
	state.shaderCreates.clear();
}
