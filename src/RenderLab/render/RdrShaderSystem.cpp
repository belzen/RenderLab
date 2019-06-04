#include "Precompiled.h"
#include "RdrShaderSystem.h"
#include "UtilsLib/Hash.h"
#include "UtilsLib/FixedVector.h"
#include <d3dcompiler.h>
#include "debug/DebugConsole.h"
#include "Renderer.h"

namespace
{
	const char* kShaderFolder = "shaders/";
	const char* kShaderFilePattern = "shaders/*.*";

	const uint kMaxDefines = 16;

	struct RdrShaderDef
	{
		const char* filename;
		const char* aDefines[kMaxDefines];
	};

	const RdrShaderDef kVertexShaderDefs[] = {
		{ "v_model.hlsl", 0 },  // RdrVertexShaderType::Model
		{ "v_decal.hlsl", 0 },  // RdrVertexShaderType::Decal
		{ "v_text.hlsl", 0},    // RdrVertexShaderType::Text
		{ "v_sprite.hlsl", 0 }, // RdrVertexShaderType::Sprite
		{ "v_sky.hlsl", 0 },    // RdrVertexShaderType::Sky
		{ "v_terrain.hlsl", 0 },// RdrVertexShaderType::Terrain
		{ "v_ocean.hlsl", 0 },  // RdrVertexShaderType::Ocean
		{ "v_screen.hlsl", 0 }, // RdrVertexShaderType::Screen
	};
	static_assert(ARRAY_SIZE(kVertexShaderDefs) == (int)RdrVertexShaderType::Count, "Missing vertex shader defs!");

	const RdrShaderDef kGeometryShaderDefs[] = {
		{ "g_cubemap.hlsl", 0 },  // RdrGeometryShaderType::Model_CubemapCapture
	};
	static_assert(ARRAY_SIZE(kGeometryShaderDefs) == (int)RdrGeometryShaderType::Count, "Missing geometry shader defs!");

	const RdrShaderDef kHullShaderDefs[] = {
		{ nullptr },				// RdrTessellationShaderType::None
		{ "h_terrain.hlsl", 0 },	// RdrTessellationShaderType::Terrain
	};
	static_assert(ARRAY_SIZE(kHullShaderDefs) == (int)RdrTessellationShaderType::Count, "Missing hull shader defs!");

	const RdrShaderDef kDomainShaderDefs[] = {
		{ nullptr },				// RdrTessellationShaderType::None
		{ "d_terrain.hlsl", 0 },	// RdrTessellationShaderType::Terrain
	};
	static_assert(ARRAY_SIZE(kDomainShaderDefs) == (int)RdrTessellationShaderType::Count, "Missing domain shader defs!");

	const RdrShaderDef kComputeShaderDefs[] = {
		{ "c_tiled_depth_calc.hlsl",			{ 0 } },						// RdrComputeShader::TiledDepthMinMax
		{ "c_tiled_light_cull.hlsl",			{ 0 } },						// RdrComputeShader::TiledLightCull
		{ "c_clustered_light_cull.hlsl",		{ 0 } },						// RdrComputeShader::ClusteredLightCull
		{ "c_luminance_measure.hlsl",			{ "STEP_ONE", 0 } },			// RdrComputeShader::LuminanceMeasure_First,
		{ "c_luminance_measure.hlsl",			{ "STEP_MID", 0 } },			// RdrComputeShader::LuminanceMeasure_Mid,
		{ "c_luminance_measure.hlsl",			{ "STEP_FINAL", 0 } },			// RdrComputeShader::LuminanceMeasure_Final,
		{ "c_luminance_histogram.hlsl",			{ "STEP_TILE", 0 } },			// RdrComputeShader::LuminanceHistogram_Tile,
		{ "c_luminance_histogram.hlsl",			{ "STEP_MERGE", 0 } },			// RdrComputeShader::LuminanceHistogram_Merge
		{ "c_luminance_histogram.hlsl",			{ "STEP_CURVE", 0 } },			// RdrComputeShader::LuminanceHistogram_ResponseCurve
		{ "c_shrink.hlsl",						{ "BLOOM_HIGHPASS", 0 } },		// RdrComputeShader::BloomShrink,
		{ "c_shrink.hlsl",						{ 0 } },						// RdrComputeShader::Shrink,
		{ "c_blend_2d.hlsl",					{ 0 } },						// RdrComputeShader::Blend2d,
		{ "c_add_2d.hlsl",						{ 0 } },						// RdrComputeShader::Add2d,
		{ "c_add_3d.hlsl",						{ 0 } },						// RdrComputeShader::Add3d,
		{ "c_blur.hlsl",						{ "BLUR_HORIZONTAL", 0 } },		// RdrComputeShader::BlurHorizontal,
		{ "c_blur.hlsl",						{ "BLUR_VERTICAL", 0 } },		// RdrComputeShader::BlurVertical,
		{ "c_clear_2d.hlsl",					{ 0 } },						// RdrComputeShader::Clear2d,
		{ "c_clear_3d.hlsl",					{ 0 } },						// RdrComputeShader::Clear3d,
		{ "c_copy_2d.hlsl",						{ 0 } },						// RdrComputeShader::Copy2d,
		{ "c_copy_3d.hlsl",						{ 0 } },						// RdrComputeShader::Copy3d,
		{ "c_atmosphere_transmittance.hlsl",	{ 0 } },						// RdrComputeShader::AtmosphereTransmittanceLut
		{ "c_atmosphere_scatter_single.hlsl",	{ 0 } },						// RdrComputeShader::AtmosphereScatterLut_Single
		{ "c_atmosphere_scatter_n.hlsl",		{ 0 } },						// RdrComputeShader::AtmosphereScatterLut_N
		{ "c_atmosphere_irradiance.hlsl",		{ "IRRADIANCE_INITIAL", 0 } },	// RdrComputeShader::AtmosphereIrradianceLut_Initial
		{ "c_atmosphere_irradiance.hlsl",		{ 0 } },						// RdrComputeShader::AtmosphereIrradianceLut_N
		{ "c_atmosphere_irradiance.hlsl",		{ "COMBINED_SCATTER", 0 } },	// RdrComputeShader::AtmosphereIrradianceLut_N_CombinedScatter
		{ "c_atmosphere_radiance.hlsl",			{ 0 } },						// RdrComputeShader::AtmosphereRadianceLut
		{ "c_atmosphere_radiance.hlsl",			{ 0 } },						// RdrComputeShader::AtmosphereRadianceLut

		{ "c_volumetric_fog_light.hlsl",		{ 0 } },						// RdrComputeShader::VolumetricFog_Light
		{ "c_volumetric_fog_accum.hlsl",		{ 0 } },						// RdrComputeShader::VolumetricFog_Accum
	};
	static_assert(ARRAY_SIZE(kComputeShaderDefs) == (int)RdrComputeShader::Count, "Missing compute shader defs!");


	typedef std::map<Hashing::StringHash, RdrShader*> RdrShaderNameMap;

	struct ShdrCmdReloadShader
	{
		union
		{
			RdrShader* pPixelShader;
			RdrVertexShaderType vertexShader;
			RdrGeometryShaderType geomShader;
			RdrComputeShader computeShader;
		};
		RdrShaderStage eStage;
	};

	struct ShdrFrameState
	{
		FixedVector<ShdrCmdReloadShader, 256>      shaderReloads;
	};

	struct
	{
		RdrShader errorShaders[(int)RdrShaderStage::Count];

		RdrShader vertexShaders[(int)RdrVertexShaderType::Count * (int)RdrShaderFlags::NumCombos];

		RdrShader geometryShaders[(int)RdrGeometryShaderType::Count * (int)RdrShaderFlags::NumCombos];
		RdrShader hullShaders[(int)RdrTessellationShaderType::Count * (int)RdrShaderFlags::NumCombos];
		RdrShader domainShaders[(int)RdrTessellationShaderType::Count * (int)RdrShaderFlags::NumCombos];

		RdrShader computeShaders[(int)RdrComputeShader::Count];
		const RdrPipelineState* computeShaderPipelineStates[(int)RdrComputeShader::Count];

		RdrShader wireframePixelShader;

		RdrShaderNameMap pixelShaderCache;
		RdrShaderList      pixelShaders;

		ThreadMutex reloadMutex;

		ShdrFrameState states[2];
		uint       queueState;

		// Defines that are auto-applied to all shaders.
		std::vector<std::string> globalShaderDefines;

		bool bInitialized;
	} s_shaderSystem;

	inline ShdrFrameState& getQueueState()
	{
		return s_shaderSystem.states[s_shaderSystem.queueState];
	}

	class IncludeHandler : public ID3DInclude
	{
		HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
		{
			char filename[FILE_MAX_PATH];
			sprintf_s(filename, "%s/%s/%s", Paths::GetDataDir(), kShaderFolder, pFileName);

			uint size;
			char* data;
			FileLoader::Load(filename, &data, &size);

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

	ID3D10Blob* preprocessShader(const char* filename, const std::vector<const char*>& defines)
	{
		HRESULT hr;
		ID3D10Blob* pPreprocessedData = nullptr;
		ID3D10Blob* pErrors = nullptr;

		char fullFilename[FILE_MAX_PATH];
		sprintf_s(fullFilename, "%s/%s/%s", Paths::GetDataDir(), kShaderFolder, filename);

		char* pFileData;
		uint fileSize;
		if (!FileLoader::Load(fullFilename, &pFileData, &fileSize))
		{
			return nullptr;
		}

		uint numDefines = (uint)defines.size();
		assert(numDefines < kMaxDefines);

		D3D_SHADER_MACRO macroDefines[kMaxDefines] = { 0 };
		for (uint i = 0; i < numDefines; ++i)
		{
			macroDefines[i].Name = defines[i];
			macroDefines[i].Definition = "1";
		}

		for (uint i = 0; i < s_shaderSystem.globalShaderDefines.size(); ++i)
		{
			macroDefines[numDefines + i].Name = s_shaderSystem.globalShaderDefines[i].c_str();
			macroDefines[numDefines + i].Definition = "1";
		}

		IncludeHandler include;
		hr = D3DPreprocess(pFileData, fileSize, nullptr, macroDefines, &include, &pPreprocessedData, &pErrors);
		if (hr != S_OK)
		{
			if (pPreprocessedData)
				pPreprocessedData->Release();
		}

		if (pErrors)
		{
			char errorMsg[2048];
			strcpy_s(errorMsg, sizeof(errorMsg), (char*)pErrors->GetBufferPointer());
			OutputDebugStringA(errorMsg);
			pErrors->Release();
		}

		delete pFileData;

		return pPreprocessedData;
	}

	void createDefaultShader(RdrContext* pRdrContext, const RdrShaderStage eStage, const RdrShaderDef& rShaderDef, const RdrShaderFlags flags, RdrShader& rOutShader)
	{
		if (!rShaderDef.filename)
			return;

		std::vector<const char*> defines;
		uint numDefines = 0;
		while (rShaderDef.aDefines[numDefines] != 0)
		{
			defines.push_back(rShaderDef.aDefines[numDefines]);
			++numDefines;
		}

		if ((flags & RdrShaderFlags::DepthOnly) != RdrShaderFlags::None)
			defines.push_back("DEPTH_ONLY");
		if ((flags & RdrShaderFlags::AlphaCutout) != RdrShaderFlags::None)
			defines.push_back("ALPHA_CUTOUT");
		if ((flags & RdrShaderFlags::IsInstanced) != RdrShaderFlags::None)
			defines.push_back("IS_INSTANCED");
		
		ID3D10Blob* pPreprocData = preprocessShader(rShaderDef.filename, defines);
		assert(pPreprocData);

		void* pCompiledData;
		uint compiledDataSize;

		const char* pShaderText = (char*)pPreprocData->GetBufferPointer();
		bool res = pRdrContext->CompileShader(eStage, pShaderText, (uint)pPreprocData->GetBufferSize(), &pCompiledData, &compiledDataSize);
		assert(res);

		rOutShader.filename = rShaderDef.filename;
		rOutShader.pCompiledData = pCompiledData;
		rOutShader.compiledSize = compiledDataSize;
		rOutShader.eStage = eStage;

		rOutShader.pCompiledData = pCompiledData;
		rOutShader.compiledSize = compiledDataSize;
		rOutShader.defines = defines;

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

	inline uint getTessellationShaderIndex(const RdrTessellationShader& shader)
	{
		return (uint)shader.eType * (uint)RdrShaderFlags::NumCombos + (uint)shader.flags;
	}

	inline uint getComputeShaderIndex(const RdrComputeShader& shader)
	{
		return (uint)shader;
	}

	void processDefaultShaderReload(RdrShaderStage eStage, RdrShader* pShader, const RdrShaderDef& rShaderDef, RdrShaderFlags flags, RdrShader& rErrorShader, RdrContext* pRdrContext)
	{
		// Free the old shader object if it wasn't the error shader.
		if (pShader->pCompiledData != rErrorShader.pCompiledData)
		{
			pRdrContext->ReleaseShader(pShader);
		}

		createDefaultShader(pRdrContext, eStage, rShaderDef, flags, *pShader);
	}

	void processShaderReload(RdrShaderStage eStage, RdrShader* pShader, RdrShader& rErrorShader, RdrContext* pRdrContext)
	{
		// Free the old shader object if it wasn't the error shader.
		if (pShader->pCompiledData != rErrorShader.pCompiledData)
		{
			pRdrContext->ReleaseShader(pShader);
		}

		ID3D10Blob* pBlob = preprocessShader(pShader->filename, pShader->defines);
		if (pBlob)
		{
			uint textLen = (uint)pBlob->GetBufferSize();
			char* pShaderText = new char[textLen];
			memcpy(pShaderText, pBlob->GetBufferPointer(), textLen);

			void* pCompiledData;
			uint compiledDataSize;
			bool succeeded = pRdrContext->CompileShader(eStage, pShaderText, textLen, &pCompiledData, &compiledDataSize);
			if (succeeded)
			{
				pShader->pCompiledData = pCompiledData;
				pShader->compiledSize = compiledDataSize;
			}
			else
			{
				pShader->pCompiledData = rErrorShader.pCompiledData;
				pShader->compiledSize = rErrorShader.compiledSize;
			}

			delete pShaderText;
			pBlob->Release();
		}
	}

	void cmdSetGlobalShaderDefine(DebugCommandArg* args, int numArgs)
	{
		RdrShaderSystem::SetGlobalShaderDefine(args[0].val.str, (args[0].val.inum != 0));
	}

	void RecreatePipelineStatesForShader(const RdrShader* pShader)
	{
		for (const RdrPipelineState* pPipelineState : pShader->refPipelineStates)
		{
			const_cast<RdrPipelineState*>(pPipelineState)->ReCreate();
		}
	}
}

void RdrShaderSystem::Init(RdrContext* pRdrContext)
{
	// Debug commands
	DebugConsole::RegisterCommand("SetGlobalShaderDefine", cmdSetGlobalShaderDefine, DebugCommandArgType::String, DebugCommandArgType::Integer);

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

	/// Tessellation shaders
	for (int ts = 0; ts < (int)RdrTessellationShaderType::Count; ++ts)
	{
		for (uint flags = 0; flags < (uint)RdrShaderFlags::NumCombos; ++flags)
		{
			createDefaultShader(pRdrContext, RdrShaderStage::Hull,
				kHullShaderDefs[ts], (RdrShaderFlags)flags,
				s_shaderSystem.hullShaders[ts * (uint)RdrShaderFlags::NumCombos + flags]);
			createDefaultShader(pRdrContext, RdrShaderStage::Domain,
				kDomainShaderDefs[ts], (RdrShaderFlags)flags,
				s_shaderSystem.domainShaders[ts * (uint)RdrShaderFlags::NumCombos + flags]);
		}
	}

	/// Compute shaders
	for (int cs = 0; cs < (int)RdrComputeShader::Count; ++cs)
	{
		createDefaultShader(pRdrContext, RdrShaderStage::Compute,
			kComputeShaderDefs[cs], RdrShaderFlags::None,
			s_shaderSystem.computeShaders[cs]);

		s_shaderSystem.computeShaderPipelineStates[cs] = RdrPipelineState::CreateComputePipelineState(&s_shaderSystem.computeShaders[cs]);
	}

	// Error shaders
	RdrShaderDef errorPixelShader = { "p_error.hlsl", 0 };
	createDefaultShader(pRdrContext, RdrShaderStage::Pixel, errorPixelShader, RdrShaderFlags::None, s_shaderSystem.errorShaders[(int)RdrShaderStage::Pixel]);

	RdrShaderDef wireframePixelShader = { "p_wireframe.hlsl", 0 };
	createDefaultShader(pRdrContext, RdrShaderStage::Pixel, wireframePixelShader, RdrShaderFlags::None, s_shaderSystem.wireframePixelShader);

	FileWatcher::AddListener(kShaderFilePattern, handleShaderFileChanged, nullptr);

	s_shaderSystem.bInitialized = true;
}

void RdrShaderSystem::ReloadShader(const char* filename)
{
	AutoScopedLock lock(s_shaderSystem.reloadMutex);

	bool found = false;

	Hashing::StringHash nameHash = Hashing::HashString(filename);
	RdrShaderNameMap::iterator iter = s_shaderSystem.pixelShaderCache.find(nameHash);
	if (iter != s_shaderSystem.pixelShaderCache.end())
	{
		ShdrCmdReloadShader& cmd = getQueueState().shaderReloads.pushSafe();
		cmd.eStage = RdrShaderStage::Pixel;
		cmd.pPixelShader = iter->second;
		found = true;
	}

	for (int i = 0; i < (int)RdrVertexShaderType::Count; ++i)
	{
		if (_stricmp(filename, kVertexShaderDefs[i].filename) == 0)
		{
			ShdrCmdReloadShader& cmd = getQueueState().shaderReloads.pushSafe();
			cmd.eStage = RdrShaderStage::Vertex;
			cmd.vertexShader = (RdrVertexShaderType)i;
			found = true;
		}
	}

	for (int i = 0; i < (int)RdrComputeShader::Count; ++i)
	{
		if (_stricmp(filename, kComputeShaderDefs[i].filename) == 0)
		{
			ShdrCmdReloadShader& cmd = getQueueState().shaderReloads.pushSafe();
			cmd.eStage = RdrShaderStage::Compute;
			cmd.computeShader = (RdrComputeShader)i;
			found = true;
		}
	}

	for (int i = 0; i < (int)RdrGeometryShaderType::Count; ++i)
	{
		if (_stricmp(filename, kGeometryShaderDefs[i].filename) == 0)
		{
			ShdrCmdReloadShader& cmd = getQueueState().shaderReloads.pushSafe();
			cmd.eStage = RdrShaderStage::Geometry;
			cmd.geomShader = (RdrGeometryShaderType)i;
			found = true;
		}
	}

	uint nFilenameLen = (uint)strlen(filename);
	if (!found && 
		(_stricmp(&filename[nFilenameLen - 6], ".hlsli") == 0) || (_stricmp(&filename[nFilenameLen - 2], ".h") == 0))
	{
		// Couldn't find any specific shader... but it has a header extension.  Just reload all shaders
		ReloadAllShaders();
	}
}

void RdrShaderSystem::ReloadAllShaders()
{
	if (!s_shaderSystem.bInitialized)
		return;

	for (int i = 0; i < (int)RdrVertexShaderType::Count; ++i)
	{
		ShdrCmdReloadShader& cmd = getQueueState().shaderReloads.pushSafe();
		cmd.eStage = RdrShaderStage::Vertex;
		cmd.vertexShader = (RdrVertexShaderType)i;
	}
	for (int i = 0; i < (int)RdrComputeShader::Count; ++i)
	{
		ShdrCmdReloadShader& cmd = getQueueState().shaderReloads.pushSafe();
		cmd.eStage = RdrShaderStage::Compute;
		cmd.computeShader = (RdrComputeShader)i;
	}
	for (int i = 0; i < (int)RdrGeometryShaderType::Count; ++i)
	{
		ShdrCmdReloadShader& cmd = getQueueState().shaderReloads.pushSafe();
		cmd.eStage = RdrShaderStage::Geometry;
		cmd.geomShader = (RdrGeometryShaderType)i;
	}

	RdrShaderNameMap::iterator iter = s_shaderSystem.pixelShaderCache.begin();
	for (; iter != s_shaderSystem.pixelShaderCache.end(); ++iter)
	{
		ShdrCmdReloadShader& cmd = getQueueState().shaderReloads.pushSafe();
		cmd.eStage = RdrShaderStage::Pixel;
		cmd.pPixelShader = iter->second;
	}
}

void RdrShaderSystem::SetGlobalShaderDefine(const char* define, bool enable)
{
	auto iter = s_shaderSystem.globalShaderDefines.begin();
	for (; iter != s_shaderSystem.globalShaderDefines.end(); ++iter)
	{
		if (strcmp(define, iter->c_str()) == 0)
		{
			break;
		}
	}

	if (enable)
	{
		if (iter == s_shaderSystem.globalShaderDefines.end())
		{
			s_shaderSystem.globalShaderDefines.push_back(define);
			ReloadAllShaders();
		}
	}
	else
	{
		if (iter != s_shaderSystem.globalShaderDefines.end())
		{
			s_shaderSystem.globalShaderDefines.erase(iter);
			ReloadAllShaders();
		}
	}
}

const RdrShader* RdrShaderSystem::CreatePixelShaderFromFile(const char* filename, const char** aDefines, uint numDefines)
{
	// Find shader in cache
	char cacheName[256] = { 0 };
	strcat_s(cacheName, filename);
	for (uint i = 0; i < numDefines; ++i)
	{
		strcat_s(cacheName, "#");
		strcat_s(cacheName, aDefines[i]);
	}

	Hashing::StringHash nameHash = Hashing::HashString(cacheName);
	RdrShaderNameMap::iterator iter = s_shaderSystem.pixelShaderCache.find(nameHash);
	if (iter != s_shaderSystem.pixelShaderCache.end())
		return iter->second;

	std::vector<const char*> defines;
	for (uint i = 0; i < numDefines; ++i)
	{
		defines.push_back(aDefines[i]);
	}

	ID3D10Blob* pBlob = preprocessShader(filename, defines);
	if (pBlob)
	{
		RdrShader* pShader = s_shaderSystem.pixelShaders.allocSafe();
		pShader->filename = _strdup(filename);
		pShader->defines = defines;

		s_shaderSystem.pixelShaderCache.insert(std::make_pair(nameHash, pShader));

		bool res = g_pRenderer->GetContext()->CompileShader(RdrShaderStage::Pixel, (char*)pBlob->GetBufferPointer(), (uint)pBlob->GetBufferSize(), &pShader->pCompiledData, &pShader->compiledSize);
		assert(res);

		return pShader;
	}
	else
	{
		return 0;
	}
}

const RdrShader* RdrShaderSystem::GetVertexShader(const RdrVertexShader& shader)
{
	uint idx = getVertexShaderIndex(shader);
	return &s_shaderSystem.vertexShaders[idx];
}

const RdrShader* RdrShaderSystem::GetGeometryShader(const RdrGeometryShader& shader)
{
	uint idx = getGeometryShaderIndex(shader);
	return &s_shaderSystem.geometryShaders[idx];
}

const RdrShader* RdrShaderSystem::GetHullShader(const RdrTessellationShader& shader)
{
	uint idx = getTessellationShaderIndex(shader);
	return &s_shaderSystem.hullShaders[idx];
}

const RdrShader* RdrShaderSystem::GetDomainShader(const RdrTessellationShader& shader)
{
	uint idx = getTessellationShaderIndex(shader);
	return &s_shaderSystem.domainShaders[idx];
}

const RdrShader* RdrShaderSystem::GetComputeShader(const RdrComputeShader eShader)
{
	uint idx = getComputeShaderIndex(eShader);
	return &s_shaderSystem.computeShaders[idx];
}

const RdrPipelineState* RdrShaderSystem::GetComputeShaderPipelineState(const RdrComputeShader eShader)
{
	uint idx = getComputeShaderIndex(eShader);
	return s_shaderSystem.computeShaderPipelineStates[idx];
}

const RdrShader* RdrShaderSystem::GetWireframePixelShader()
{
	return &s_shaderSystem.wireframePixelShader;
}

void RdrShaderSystem::FlipState()
{
	s_shaderSystem.queueState = !s_shaderSystem.queueState;
}

void RdrShaderSystem::ProcessCommands(RdrContext* pRdrContext)
{
	ShdrFrameState& state = s_shaderSystem.states[!s_shaderSystem.queueState];

	// Shader creates
	{
		AutoScopedLock lock(s_shaderSystem.reloadMutex);

		uint numCmds = (uint)state.shaderReloads.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			ShdrCmdReloadShader& cmd = state.shaderReloads[i];

			RdrShader& rErrorShader = s_shaderSystem.errorShaders[(int)cmd.eStage];
			RdrShader* pShader = nullptr;

			switch (cmd.eStage)
			{
			case RdrShaderStage::Pixel:
				{
					pShader = cmd.pPixelShader;
					processShaderReload(cmd.eStage, pShader, rErrorShader, pRdrContext);
				}
				break;
			case RdrShaderStage::Vertex:
				{
					RdrVertexShader vertexShader;
					vertexShader.eType = cmd.vertexShader;
					for (uint flags = 0; flags < (uint)RdrShaderFlags::NumCombos; ++flags)
					{
						vertexShader.flags = (RdrShaderFlags)flags;
						pShader = &s_shaderSystem.vertexShaders[getVertexShaderIndex(vertexShader)];
						processDefaultShaderReload(cmd.eStage, pShader, kVertexShaderDefs[(int)cmd.vertexShader], vertexShader.flags, rErrorShader, pRdrContext);
					}
				}
				break;
			case RdrShaderStage::Geometry:
				{
					RdrGeometryShader geomShader;
					geomShader.eType = cmd.geomShader;
					for (uint flags = 0; flags < (uint)RdrShaderFlags::NumCombos; ++flags)
					{
						geomShader.flags = (RdrShaderFlags)flags;
						pShader = &s_shaderSystem.geometryShaders[getGeometryShaderIndex(geomShader)];
						processDefaultShaderReload(cmd.eStage, pShader, kGeometryShaderDefs[(int)cmd.geomShader], geomShader.flags, rErrorShader, pRdrContext);
					}
				}
				break;
			case RdrShaderStage::Compute:
				{
					pShader = &s_shaderSystem.computeShaders[getComputeShaderIndex(cmd.computeShader)];
					processDefaultShaderReload(cmd.eStage, pShader, kComputeShaderDefs[(int)cmd.computeShader], RdrShaderFlags::None, rErrorShader, pRdrContext);
				}
				break;
			}

			RecreatePipelineStatesForShader(pShader);
		}
	}

	state.shaderReloads.clear();
}
