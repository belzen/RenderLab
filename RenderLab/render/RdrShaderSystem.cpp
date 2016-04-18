#include "Precompiled.h"
#include "RdrShaderSystem.h"
#include "RdrContext.h"
#include <d3dcompiler.h>
#include <fstream>
#include <string>
#include "FileWatcher.h"

namespace
{
	const char* kShaderFolder = "shaders/";
	const char* kShaderDataDir = "data/shaders/";

	const uint kMaxDefines = 16;

	struct RdrShaderDef
	{
		const char* filename;
		const char* aDefines[kMaxDefines];
	};

	const RdrShaderDef kVertexShaderDefs[] = {
		{ "v_model.hlsl", 0 },  // RdrVertexShaderType::Model
		{ "v_text.hlsl", 0},    // RdrVertexShaderType::Text
		{ "v_sprite.hlsl", 0 }, // RdrVertexShaderType::Sprite
		{ "v_sky.hlsl", 0 },    // RdrVertexShaderType::Sky
		{ "v_screen.hlsl", 0 }, // RdrVertexShaderType::Screen
	};
	static_assert(ARRAYSIZE(kVertexShaderDefs) == (int)RdrVertexShaderType::Count, "Missing vertex shader defs!");

	const RdrShaderDef kGeometryShaderDefs[] = {
		{ "g_cubemap.hlsl", 0 },  // RdrGeometryShaderType::Model_CubemapCapture
	};
	static_assert(ARRAYSIZE(kGeometryShaderDefs) == (int)RdrGeometryShaderType::Count, "Missing geometry shader defs!");

	const RdrShaderDef kComputeShaderDefs[] = {
		{"c_tiled_depth_calc.hlsl", { 0 } },          // RdrComputeShader::TiledDepthMinMax
		{"c_tiled_light_cull.hlsl", { 0 } },          // RdrComputeShader::TiledLightCull
		{"c_luminance_measure.hlsl",{ "STEP_ONE", 0 } }, // RdrComputeShader::LuminanceMeasure_First,
		{"c_luminance_measure.hlsl", { "STEP_MID", 0 } },   // RdrComputeShader::LuminanceMeasure_Mid,
		{"c_luminance_measure.hlsl", { "STEP_FINAL", 0 } },  // RdrComputeShader::LuminanceMeasure_Final,
	};
	static_assert(ARRAYSIZE(kComputeShaderDefs) == (int)RdrComputeShader::Count, "Missing compute shader defs!");


	typedef std::map<std::string, RdrShaderHandle> RdrShaderHandleMap; // todo: better map key for this and other caches

	struct CmdCreatePixelShader
	{
		RdrShaderHandle hShader;
		char* pShaderText;
		uint textLen;
	};

	struct CmdCreateInputLayout
	{
		RdrInputLayoutHandle hLayout;
		RdrVertexShader vertexShader;
		RdrVertexInputElement vertexElements[16];
		uint numElements;
	};

	struct CmdReloadShader
	{
		union
		{
			RdrShaderHandle hPixelShader;
			RdrVertexShader vertexShader;
			RdrGeometryShader geomShader;
			RdrComputeShader computeShader;
		};
		RdrShaderStage eStage;
	};

	struct FrameState
	{
		std::vector<CmdCreatePixelShader> pixelShaderCreates;
		std::vector<CmdCreateInputLayout> layoutCreates;
		std::vector<CmdReloadShader> shaderReloads;
	};

	struct
	{
		RdrShader errorShaders[(int)RdrShaderStage::Count];

		RdrShader vertexShaders[(int)RdrVertexShaderType::Count * (int)RdrShaderFlags::NumCombos];
		RdrShader geometryShaders[(int)RdrGeometryShaderType::Count * (int)RdrShaderFlags::NumCombos];
		RdrShader computeShaders[(int)RdrComputeShader::Count];

		RdrShaderHandleMap pixelShaderCache;
		RdrShaderList      pixelShaders;

		RdrInputLayoutList inputLayouts;

		ThreadLock reloadLock;

		FrameState states[2];
		uint       queueState;
	} s_shaderSystem;

	inline FrameState& getQueueState()
	{
		return s_shaderSystem.states[s_shaderSystem.queueState];
	}

	class IncludeHandler : public ID3DInclude
	{
		HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
		{
			char filename[MAX_PATH];
			sprintf_s(filename, "%s/%s", kShaderDataDir, pFileName);

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
		sprintf_s(fullFilename, "%s/%s", kShaderDataDir, filename);

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

	void createDefaultShader(RdrContext* pRdrContext, const RdrShaderStage eStage, const RdrShaderDef& rShaderDef, const RdrShaderFlags flags, RdrShader& rOutShader)
	{
		void* pCompiledData;
		uint compiledDataSize;

		const char* aDefines[kMaxDefines];
		uint numDefines = 0;

		while (rShaderDef.aDefines[numDefines] != 0)
		{
			aDefines[numDefines] = rShaderDef.aDefines[numDefines];
			++numDefines;
		}

		if ((flags & RdrShaderFlags::DepthOnly) != RdrShaderFlags::None)
			aDefines[numDefines++] = "DEPTH_ONLY";
		if ((flags & RdrShaderFlags::CubemapCapture) != RdrShaderFlags::None)
			aDefines[numDefines++] = "CUBEMAP_CAPTURE";
		
		ID3D10Blob* pPreprocData = preprocessShader(rShaderDef.filename, aDefines, numDefines);
		assert(pPreprocData);
	
		const char* pShaderText = (char*)pPreprocData->GetBufferPointer();
		bool res = pRdrContext->CompileShader(eStage, pShaderText, (uint)pPreprocData->GetBufferSize(), &pCompiledData, &compiledDataSize);
		assert(res);

		rOutShader.pTypeless = pRdrContext->CreateShader(eStage, pCompiledData, compiledDataSize);
		rOutShader.eStage = eStage;
		if (eStage == RdrShaderStage::Vertex)
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

	void handleShaderFileChanged(const char* filename, void* pUserData)
	{
		const char* shaderName = filename + strlen(kShaderFolder);
		RdrShaderSystem::ReloadShader(shaderName);
	}

	inline uint getVertexShaderIndex(const RdrVertexShader& shader)
	{
		return (uint)shader.eType * (uint)RdrShaderFlags::NumCombos + (uint)shader.flags;
	}

	inline uint getGeometryShaderIndex(const RdrGeometryShader& shader)
	{
		return (uint)shader.eType * (uint)RdrShaderFlags::NumCombos + (uint)shader.flags;
	}

	inline uint getComputeShaderIndex(const RdrComputeShader& shader)
	{
		return (uint)shader;
	}
}

void RdrShaderSystem::Init(RdrContext* pRdrContext)
{
	//////////////////////////////////////
	// Load default shaders.
	for (int vs = 0; vs < (uint)RdrVertexShaderType::Count; ++vs)
	{
		for (uint flags = 0; flags < (uint)RdrShaderFlags::NumCombos; ++flags)
		{
			createDefaultShader(pRdrContext, RdrShaderStage::Vertex,
				kVertexShaderDefs[vs], (RdrShaderFlags)flags,
				s_shaderSystem.vertexShaders[vs * (uint)RdrShaderFlags::NumCombos + flags]);
		}
	}

	/// Geometry shaders
	for (int gs = 0; gs < (int)RdrGeometryShaderType::Count; ++gs)
	{
		for (uint flags = 0; flags < (uint)RdrShaderFlags::NumCombos; ++flags)
		{
			createDefaultShader(pRdrContext, RdrShaderStage::Geometry,
				kGeometryShaderDefs[gs], (RdrShaderFlags)flags,
				s_shaderSystem.geometryShaders[gs * (uint)RdrShaderFlags::NumCombos + flags]);
		}
	}

	/// Compute shaders
	for (int cs = 0; cs < (int)RdrComputeShader::Count; ++cs)
	{
		createDefaultShader(pRdrContext, RdrShaderStage::Compute,
			kComputeShaderDefs[cs], RdrShaderFlags::None,
			s_shaderSystem.computeShaders[cs]);
	}

	// Error shaders
	RdrShaderDef errorPixelShader = { "p_error.hlsl", 0 };
	createDefaultShader(pRdrContext, RdrShaderStage::Pixel, errorPixelShader, RdrShaderFlags::None, s_shaderSystem.errorShaders[(int)RdrShaderStage::Pixel]);

	FileWatcher::AddListener("shaders/*.*", handleShaderFileChanged, nullptr);
}

void RdrShaderSystem::ReloadShader(const char* filename)
{
	CmdReloadShader cmd;

	RdrShaderHandleMap::iterator iter = s_shaderSystem.pixelShaderCache.find(filename);
	if (iter != s_shaderSystem.pixelShaderCache.end())
	{
		cmd.eStage = RdrShaderStage::Pixel;
		cmd.hPixelShader = iter->second;

		AutoScopedLock lock(s_shaderSystem.reloadLock);
		getQueueState().shaderReloads.push_back(cmd);
	}
	else
	{
		// todo: other shader types
	}
}

RdrShaderHandle RdrShaderSystem::CreatePixelShaderFromFile(const char* filename)
{
	// Find shader in cache
	RdrShaderHandleMap::iterator iter = s_shaderSystem.pixelShaderCache.find(filename);
	if (iter != s_shaderSystem.pixelShaderCache.end())
		return iter->second;

	ID3D10Blob* pBlob = preprocessShader(filename, nullptr, 0);
	if (pBlob)
	{
		RdrShader* pShader = s_shaderSystem.pixelShaders.allocSafe();
		pShader->filename = _strdup(filename);

		CmdCreatePixelShader cmd;
		cmd.hShader = s_shaderSystem.pixelShaders.getId(pShader);
		cmd.textLen = (uint)pBlob->GetBufferSize();
		cmd.pShaderText = new char[cmd.textLen];
		memcpy(cmd.pShaderText, pBlob->GetBufferPointer(), cmd.textLen);

		getQueueState().pixelShaderCreates.push_back(cmd);
		pBlob->Release();

		s_shaderSystem.pixelShaderCache.insert(std::make_pair(filename, cmd.hShader));
		return cmd.hShader;
	}
	else
	{
		return 0;
	}
}

RdrInputLayoutHandle RdrShaderSystem::CreateInputLayout(const RdrVertexShader& vertexShader, const RdrVertexInputElement* aVertexElements, const uint numElements)
{
	RdrInputLayout* pLayout = s_shaderSystem.inputLayouts.allocSafe();

	CmdCreateInputLayout cmd;
	cmd.hLayout = s_shaderSystem.inputLayouts.getId(pLayout);
	cmd.vertexShader = vertexShader;
	cmd.hLayout = s_shaderSystem.inputLayouts.getId(pLayout);
	cmd.numElements = numElements;
	memcpy(cmd.vertexElements, aVertexElements, sizeof(RdrVertexInputElement) * numElements);

	getQueueState().layoutCreates.push_back(cmd);

	return cmd.hLayout;
}

const RdrShader* RdrShaderSystem::GetVertexShader(const RdrVertexShader shader)
{
	uint idx = getVertexShaderIndex(shader);
	return &s_shaderSystem.vertexShaders[idx];
}

const RdrShader* RdrShaderSystem::GetGeometryShader(const RdrGeometryShader shader)
{
	uint idx = getGeometryShaderIndex(shader);
	return &s_shaderSystem.geometryShaders[idx];
}

const RdrShader* RdrShaderSystem::GetComputeShader(const RdrComputeShader eShader)
{
	uint idx = getComputeShaderIndex(eShader);
	return &s_shaderSystem.computeShaders[idx];
}

const RdrShader* RdrShaderSystem::GetPixelShader(const RdrShaderHandle hShader)
{
	return s_shaderSystem.pixelShaders.get(hShader);
}

const RdrInputLayout* RdrShaderSystem::GetInputLayout(const RdrInputLayoutHandle hLayout)
{
	return s_shaderSystem.inputLayouts.get(hLayout);
}

void RdrShaderSystem::FlipState()
{
	s_shaderSystem.queueState = !s_shaderSystem.queueState;
}

void RdrShaderSystem::ProcessCommands(RdrContext* pRdrContext)
{
	FrameState& state = s_shaderSystem.states[!s_shaderSystem.queueState];

	// Shader creates
	uint numCmds = (uint)state.pixelShaderCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreatePixelShader& cmd = state.pixelShaderCreates[i];
		RdrShader* pShader = s_shaderSystem.pixelShaders.get(cmd.hShader);

		void* pCompiledData;
		uint compiledDataSize;

		bool res = pRdrContext->CompileShader(RdrShaderStage::Pixel, cmd.pShaderText, cmd.textLen, &pCompiledData, &compiledDataSize);
		assert(res);

		pShader->pTypeless = pRdrContext->CreateShader(RdrShaderStage::Pixel, pCompiledData, compiledDataSize);
		
		delete cmd.pShaderText;
		delete pCompiledData;
	}

	numCmds = (uint)state.layoutCreates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdCreateInputLayout& cmd = state.layoutCreates[i];
		RdrInputLayout* pLayout = s_shaderSystem.inputLayouts.get(cmd.hLayout);
		const RdrShader* pShader = GetVertexShader(cmd.vertexShader);

		*pLayout = pRdrContext->CreateInputLayout(pShader->pVertexCompiledData, pShader->compiledSize, cmd.vertexElements, cmd.numElements);
	}

	{
		AutoScopedLock lock(s_shaderSystem.reloadLock);

		numCmds = (uint)state.shaderReloads.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			CmdReloadShader& cmd = state.shaderReloads[i];

			RdrShader& rErrorShader = s_shaderSystem.errorShaders[(int)cmd.eStage];
			RdrShader* pShader = nullptr;

			switch (cmd.eStage)
			{
			case RdrShaderStage::Pixel:
				pShader = s_shaderSystem.pixelShaders.get(cmd.hPixelShader);
				break;
			case RdrShaderStage::Vertex:
				pShader = &s_shaderSystem.vertexShaders[getVertexShaderIndex(cmd.vertexShader)];
				break;
			case RdrShaderStage::Geometry:
				pShader = &s_shaderSystem.geometryShaders[getGeometryShaderIndex(cmd.geomShader)];
				break;
			case RdrShaderStage::Compute:
				pShader = &s_shaderSystem.computeShaders[getComputeShaderIndex(cmd.computeShader)];
				break;
			}

			// Free the old shader object if it wasn't the error shader.
			if (pShader->pTypeless != rErrorShader.pTypeless)
			{
				pRdrContext->ReleaseShader(pShader);
			}

			ID3D10Blob* pBlob = preprocessShader(pShader->filename, nullptr, 0);
			if (pBlob)
			{
				uint textLen = (uint)pBlob->GetBufferSize();
				char* pShaderText = new char[textLen];
				memcpy(pShaderText, pBlob->GetBufferPointer(), textLen);

				void* pCompiledData;
				uint compiledDataSize;
				bool succeeded = pRdrContext->CompileShader(cmd.eStage, pShaderText, textLen, &pCompiledData, &compiledDataSize);
				if (succeeded)
				{
					pShader->pTypeless = pRdrContext->CreateShader(cmd.eStage, pCompiledData, compiledDataSize);
					delete pCompiledData;
				}
				else
				{
					pShader->pTypeless = rErrorShader.pTypeless;
				}

				delete pShaderText;
				pBlob->Release();
			}
		}
	}

	state.shaderReloads.clear();
	state.layoutCreates.clear();
	state.pixelShaderCreates.clear();
}
