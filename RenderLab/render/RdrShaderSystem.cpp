#include "Precompiled.h"
#include "RdrShaderSystem.h"
#include "RdrContext.h"
#include <d3dcompiler.h>
#include <fstream>
#include <string>

namespace
{
	const char* kShaderRootDir = "data/shaders/";

	const uint kMaxDefines = 16;

	const char* kVertexShaderFilenames[] = {
		"v_model.hlsl",  // kRdrVertexShader_Model
		"v_text.hlsl", // kRdrVertexShader_Text
		"v_sprite.hlsl"    // kRdrVertexShader_Sprite
	};
	static_assert(ARRAYSIZE(kVertexShaderFilenames) == kRdrVertexShader_Count, "Missing vertex shader filename!");

	const char* kGeometryShaderFilenames[] = {
		"g_cubemap.hlsl",  // kRdrGeometryShader_Model_CubemapCapture
	};
	static_assert(ARRAYSIZE(kGeometryShaderFilenames) == kRdrGeometryShader_Count, "Missing geometry shader filename!");

	const char* kComputeShaderFilenames[] = {
		"c_tiled_depth_calc.hlsl", // kRdrComputeShader_TiledDepthMinMax
		"c_tiled_light_cull.hlsl"  // kRdrComputeShader_TiledLightCull
	};
	static_assert(ARRAYSIZE(kComputeShaderFilenames) == kRdrComputeShader_Count, "Missing compute shader filename!");


	class IncludeHandler : public ID3DInclude
	{
		HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
		{
			char filename[MAX_PATH];
			sprintf_s(filename, "%s/%s", kShaderRootDir, pFileName);

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

	ID3D10Blob* preprocessShader(const char* filename, const char** aDefines, uint numDefines)
	{
		HRESULT hr;
		ID3D10Blob* pCompiledData;
		ID3D10Blob* pErrors = nullptr;

		char fullFilename[MAX_PATH];
		sprintf_s(fullFilename, "%s/%s", kShaderRootDir, filename);

		std::ifstream file(fullFilename);
		std::string shaderText;
		std::getline(file, shaderText, (char)EOF);

		assert(numDefines < kMaxDefines);

		D3D_SHADER_MACRO macroDefines[kMaxDefines] = { 0 };
		for (uint i = 0; i < numDefines; ++i)
		{
			macroDefines[i].Name = aDefines[i];
			macroDefines[i].Definition = "1";
		}

		IncludeHandler include;
		hr = D3DPreprocess(shaderText.c_str(), shaderText.size(), nullptr, macroDefines, &include, &pCompiledData, &pErrors);
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

	void createDefaultShader(RdrContext* pRdrContext, const RdrShaderType eType, const char* shaderFilename, const RdrShaderFlags flags, RdrShader& rOutShader)
	{
		void* pCompiledData;
		uint compiledDataSize;

		const char* aDefines[kMaxDefines];
		uint numDefines = 0;

		if (flags & kRdrShaderFlag_DepthOnly)
			aDefines[numDefines++] = "DEPTH_ONLY";
		if (flags & kRdrShaderFlag_CubemapCapture)
			aDefines[numDefines++] = "CUBEMAP_CAPTURE";

		ID3D10Blob* pPreprocData = preprocessShader(shaderFilename, aDefines, numDefines);
		assert(pPreprocData);
	
		const char* pShaderText = (char*)pPreprocData->GetBufferPointer();
		bool res = pRdrContext->CompileShader(eType, pShaderText, (uint)pPreprocData->GetBufferSize(), &pCompiledData, &compiledDataSize);
		assert(res);

		rOutShader.pTypeless = pRdrContext->CreateShader(eType, pCompiledData, compiledDataSize);
		if (eType == kRdrShaderType_Vertex)
		{
			rOutShader.pVertexCompiledData = pCompiledData;
			rOutShader.compiledSize = compiledDataSize;
		}
		else
		{
			delete pCompiledData;
		}

		pPreprocData->Release();
	}
}

void RdrShaderSystem::Init(RdrContext* pRdrContext)
{
	m_pRdrContext = pRdrContext;

	//////////////////////////////////////
	// Load default shaders.
	for (int vs = 0; vs < kRdrVertexShader_Count; ++vs)
	{
		for (uint flags = 0; flags < kRdrShaderFlag_NumCombos; ++flags)
		{
			createDefaultShader(m_pRdrContext, kRdrShaderType_Vertex, 
				kVertexShaderFilenames[vs], (RdrShaderFlags)flags,
				m_vertexShaders[vs * kRdrShaderFlag_NumCombos + flags]);
		}
	}

	/// Geometry shaders
	for (int gs = 0; gs < kRdrGeometryShader_Count; ++gs)
	{
		for (uint flags = 0; flags < kRdrShaderFlag_NumCombos; ++flags)
		{
			createDefaultShader(m_pRdrContext, kRdrShaderType_Geometry,
				kGeometryShaderFilenames[gs], (RdrShaderFlags)flags,
				m_geometryShaders[gs * kRdrShaderFlag_NumCombos + flags]);
		}
	}

	/// Compute shaders
	for (int cs = 0; cs < kRdrComputeShader_Count; ++cs)
	{
		createDefaultShader(m_pRdrContext, kRdrShaderType_Compute,
			kComputeShaderFilenames[cs], (RdrShaderFlags)0,
			m_computeShaders[cs]);
	}
}

RdrShaderHandle RdrShaderSystem::CreatePixelShaderFromFile(const char* filename)
{
	// Find shader in cache
	RdrShaderHandleMap::iterator iter = m_pixelShaderCache.find(filename);
	if (iter != m_pixelShaderCache.end())
		return iter->second;

	ID3D10Blob* pBlob = preprocessShader(filename, nullptr, 0);
	if (pBlob)
	{
		RdrShader* pShader = m_pixelShaders.allocSafe();
		pShader->filename = filename;

		CmdCreatePixelShader cmd;
		cmd.hShader = m_pixelShaders.getId(pShader);
		cmd.textLen = (uint)pBlob->GetBufferSize();
		cmd.pShaderText = new char[cmd.textLen];
		memcpy(cmd.pShaderText, pBlob->GetBufferPointer(), cmd.textLen);

		m_states[m_queueState].pixelShaderCreates.push_back(cmd);
		pBlob->Release();

		m_pixelShaderCache.insert(std::make_pair(filename, cmd.hShader));
		return cmd.hShader;
	}
	else
	{
		return 0;
	}
}

RdrInputLayoutHandle RdrShaderSystem::CreateInputLayout(const RdrVertexShader& vertexShader, const RdrVertexInputElement* aVertexElements, const uint numElements)
{
	RdrInputLayout* pLayout = m_inputLayouts.allocSafe();

	CmdCreateInputLayout cmd;
	cmd.hLayout = m_inputLayouts.getId(pLayout);
	cmd.vertexShader = vertexShader;
	cmd.hLayout = m_inputLayouts.getId(pLayout);
	cmd.numElements = numElements;
	memcpy(cmd.vertexElements, aVertexElements, sizeof(RdrVertexInputElement) * numElements);

	m_states[m_queueState].layoutCreates.push_back(cmd);

	return cmd.hLayout;
}

const RdrShader* RdrShaderSystem::GetVertexShader(const RdrVertexShader shader)
{
	return &m_vertexShaders[shader.eType * kRdrShaderFlag_NumCombos + shader.flags];
}

const RdrShader* RdrShaderSystem::GetGeometryShader(const RdrGeometryShader shader)
{
	return &m_geometryShaders[shader.eType * kRdrShaderFlag_NumCombos + shader.flags];
}

const RdrShader* RdrShaderSystem::GetComputeShader(const RdrComputeShader eShader)
{
	return &m_computeShaders[eShader];
}

const RdrShader* RdrShaderSystem::GetPixelShader(const RdrShaderHandle hShader)
{
	return m_pixelShaders.get(hShader);
}

const RdrInputLayout* RdrShaderSystem::GetInputLayout(const RdrInputLayoutHandle hLayout)
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
	uint numCmds = (uint)state.pixelShaderCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreatePixelShader& cmd = state.pixelShaderCreates[i];
		RdrShader* pShader = m_pixelShaders.get(cmd.hShader);

		void* pCompiledData;
		uint compiledDataSize;

		bool res = m_pRdrContext->CompileShader(kRdrShaderType_Pixel, cmd.pShaderText, cmd.textLen, &pCompiledData, &compiledDataSize);
		assert(res);

		pShader->pTypeless = m_pRdrContext->CreateShader(kRdrShaderType_Pixel, pCompiledData, compiledDataSize);
		
		delete pCompiledData;
	}

	numCmds = (uint)state.layoutCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateInputLayout& cmd = state.layoutCreates[i];
		RdrInputLayout* pLayout = m_inputLayouts.get(cmd.hLayout);
		const RdrShader* pShader = GetVertexShader(cmd.vertexShader);

		*pLayout = m_pRdrContext->CreateInputLayout(pShader->pVertexCompiledData, pShader->compiledSize, cmd.vertexElements, cmd.numElements);
	}

	state.layoutCreates.clear();
	state.pixelShaderCreates.clear();
}
