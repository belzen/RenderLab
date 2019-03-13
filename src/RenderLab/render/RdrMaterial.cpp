#include "Precompiled.h"
#include "RdrMaterial.h"
#include "RdrShaderSystem.h"
#include "RdrResourceSystem.h"
#include "RdrFrameMem.h"
#include "RdrShaderConstants.h"
#include "Renderer.h"
#include "AssetLib/MaterialAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "UtilsLib/Hash.h"

namespace
{
	typedef std::map<Hashing::SHA1, RdrMaterial*> RdrMaterialMap;
	typedef FreeList<RdrMaterial, 1024> RdrMaterialList;

	RdrMaterialList s_materials;
	RdrMaterialMap s_materialCache;

	void loadMaterial(const CachedString& materialName, const RdrVertexInputElement* pInputLayout, uint nNumInputElements, RdrMaterial* pOutMaterial)
	{
		AssetLib::Material* pMaterial = AssetLibrary<AssetLib::Material>::LoadAsset(materialName);
		if (!pMaterial)
		{
			assert(false);
			return;
		}

		// Create default pixel shader
		const char* defines[32] = { 0 };
		uint numDefines = 0;
		if (pMaterial->bAlphaCutout)
		{
			defines[numDefines++] = "ALPHA_CUTOUT";
		}

		if (pMaterial->color.a < 1.f)
		{
			pOutMaterial->bHasAlpha = true;
			defines[numDefines++] = "HAS_ALPHA";
		}

		for (int i = 0; i < pMaterial->shaderDefsCount; ++i)
		{
			defines[numDefines++] = pMaterial->pShaderDefs[i];
		}

		RdrVertexShader vertexShader = {};
		if (_stricmp(pMaterial->vertexShader, "model") == 0)
		{
			vertexShader.eType = RdrVertexShaderType::Model;
		}
		else if (_stricmp(pMaterial->vertexShader, "sky") == 0)
		{
			vertexShader.eType = RdrVertexShaderType::Sky;
		}

		if (pMaterial->bAlphaCutout)
		{
			vertexShader.flags |= RdrShaderFlags::AlphaCutout;
		}

		// Normal PSO
		assert(numDefines <= ARRAY_SIZE(defines));

		RdrDepthStencilState depthState = RdrDepthStencilState(true, false, RdrComparisonFunc::Equal);
		RdrBlendMode eBlendMode = RdrBlendMode::kOpaque;
		if (pOutMaterial->bHasAlpha)
		{
			depthState = RdrDepthStencilState(true, false, RdrComparisonFunc::Less);
			eBlendMode = RdrBlendMode::kAlpha;
		}

		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		// Normal
		RdrRasterState rasterState;
		rasterState.bWireframe = false;
		rasterState.bDoubleSided = false;
		rasterState.bEnableMSAA = false; //donotcheckin g_debugState.msaaLevel
		rasterState.bUseSlopeScaledDepthBias = false;
		rasterState.bEnableScissor = false;

		const RdrResourceFormat* pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kScene_GBuffer); 
		uint nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kScene_GBuffer);

		RdrShaderHandle hPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);
		pOutMaterial->CreatePipelineState(RdrShaderMode::Normal,
			vertexShader, hPixelShader,
			pInputLayout, nNumInputElements,
			pRtvFormats, nNumRtvFormats,
			eBlendMode,
			rasterState,
			depthState);


		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		// Wireframe
		rasterState.bWireframe = true;
		rasterState.bDoubleSided = true;
		rasterState.bEnableMSAA = false;

		pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kPrimary);
		nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kPrimary);

		hPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);
		pOutMaterial->CreatePipelineState(RdrShaderMode::Wireframe,
			vertexShader, hPixelShader,
			pInputLayout, nNumInputElements,
			pRtvFormats, nNumRtvFormats,
			eBlendMode,
			rasterState,
			RdrDepthStencilState(true, false, RdrComparisonFunc::Less));


		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		// Editor
		rasterState.bWireframe = false;
		rasterState.bDoubleSided = false;
		rasterState.bEnableMSAA = false;
		rasterState.bUseSlopeScaledDepthBias = false;
		rasterState.bEnableScissor = false;

		pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kUI);
		nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kUI);
		hPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);

		pOutMaterial->CreatePipelineState(RdrShaderMode::Editor,
			vertexShader, hPixelShader,
			pInputLayout, nNumInputElements,
			pRtvFormats, nNumRtvFormats,
			RdrBlendMode::kAlpha,
			rasterState,
			RdrDepthStencilState(true, false, RdrComparisonFunc::Equal));


		if (!pOutMaterial->bHasAlpha)
		{
			//////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////
			// Depth Only
			pOutMaterial->activeShaderStages[(int)RdrShaderMode::DepthOnly] = RdrShaderStageFlags::Vertex;
			pOutMaterial->activeShaderStages[(int)RdrShaderMode::ShadowMap] = RdrShaderStageFlags::Vertex;

			// Alpha cutout requires a depth-only pixel shader.
			if (pMaterial->bAlphaCutout)
			{
				defines[numDefines++] = "DEPTH_ONLY";
				assert(numDefines <= ARRAY_SIZE(defines));

				hPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);
				pOutMaterial->activeShaderStages[(int)RdrShaderMode::DepthOnly] |= RdrShaderStageFlags::Pixel;
				pOutMaterial->activeShaderStages[(int)RdrShaderMode::ShadowMap] |= RdrShaderStageFlags::Pixel;
			}
			else
			{
				hPixelShader = 0;
			}

			pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kScene_ZPrepass);
			nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kScene_ZPrepass);

			rasterState.bWireframe = false;
			rasterState.bDoubleSided = false;
			rasterState.bEnableMSAA = false; // donotcheckin (g_debugState.msaaLevel > 1);
			rasterState.bUseSlopeScaledDepthBias = false;
			rasterState.bEnableScissor = false;

			pOutMaterial->CreatePipelineState(RdrShaderMode::DepthOnly,
				vertexShader, hPixelShader,
				pInputLayout, nNumInputElements,
				pRtvFormats, nNumRtvFormats,
				RdrBlendMode::kOpaque,
				rasterState,
				RdrDepthStencilState(true, true, RdrComparisonFunc::Less));


			//////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////
			// Shadows
			pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kShadowMap);
			nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kShadowMap);

			rasterState.bWireframe = false;
			rasterState.bDoubleSided = true; // donotcheckin - double-sided? opposite cull mode?
			rasterState.bEnableMSAA = false;
			rasterState.bUseSlopeScaledDepthBias = true;
			rasterState.bEnableScissor = false;

			pOutMaterial->CreatePipelineState(RdrShaderMode::ShadowMap,
				vertexShader, hPixelShader,
				pInputLayout, nNumInputElements,
				pRtvFormats, nNumRtvFormats,
				RdrBlendMode::kOpaque,
				rasterState,
				RdrDepthStencilState(true, true, RdrComparisonFunc::Less));
		}

		//////////////////////////////////////////////////////////////////////////
		// 
		pOutMaterial->bNeedsLighting = pMaterial->bNeedsLighting;
		pOutMaterial->bAlphaCutout = pMaterial->bAlphaCutout;
			
		RdrResourceCommandList& rResCommandList = g_pRenderer->GetPreFrameCommandList();
		for (int n = 0; n < pMaterial->texCount; ++n)
		{
			pOutMaterial->ahTextures.assign(n, rResCommandList.CreateTextureFromFile(pMaterial->pTextureNames[n], nullptr));
			pOutMaterial->aSamplers.assign(n, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false));
		}

		// DONOTCHECKIN2 - Copy textures and sampler descriptors to descriptor table


		uint constantsSize = sizeof(MaterialParams);
		MaterialParams* pConstants = (MaterialParams*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pConstants->metalness = pMaterial->metalness;
		pConstants->roughness = pMaterial->roughness;
		pConstants->color = pMaterial->color.asFloat4();
		pOutMaterial->hConstants = rResCommandList.CreateConstantBuffer(pConstants, constantsSize, RdrResourceAccessFlags::CpuRO_GpuRO);

		pOutMaterial->name = materialName;
	}
}

void RdrMaterial::CreatePipelineState(
	RdrShaderMode eMode,
	const RdrVertexShader& vertexShader, const RdrShaderHandle hPixelShader,
	const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
	const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
	const RdrBlendMode eBlendMode,
	const RdrRasterState& rasterState,
	const RdrDepthStencilState& depthStencilState)
{
	const RdrShader* pVertexShader = RdrShaderSystem::GetVertexShader(vertexShader);
	const RdrShader* pPixelShader = hPixelShader ? RdrShaderSystem::GetPixelShader(hPixelShader) : nullptr;

	hPipelineStates[(int)eMode] = g_pRenderer->GetContext()->CreateGraphicsPipelineState(
		pVertexShader, pPixelShader,
		pInputLayoutElements, nNumInputElements,
		pRtvFormats, nNumRtvFormats,
		eBlendMode,
		rasterState,
		depthStencilState);

	activeShaderStages[(int)eMode] = RdrShaderStageFlags::Vertex;
	if (pPixelShader)
	{
		activeShaderStages[(int)eMode] |= RdrShaderStageFlags::Pixel;
	}
}

void RdrMaterial::ReloadMaterial(const char* materialName)
{
#if 0 //donotcheckin
	Hashing::StringHash nameHash = Hashing::HashString(materialName);
	RdrMaterialMap::iterator iter = s_materialCache.find(nameHash);
	if (iter != s_materialCache.end())
	{
		loadMaterial(materialName, iter->second);
	}
#endif
}

uint16 RdrMaterial::GetMaterialId(const RdrMaterial* pMaterial)
{
	return s_materials.getId(pMaterial);
}

RdrMaterial* RdrMaterial::Create(const CachedString& materialName, const RdrVertexInputElement* pInputLayout, uint nNumInputElements)
{
	if (!AssetLib::Material::GetAssetDef().HasReloadHandler())
	{
		AssetLib::Material::GetAssetDef().SetReloadHandler(RdrMaterial::ReloadMaterial);
	}
	
	Hashing::SHA1HashState* pHashState = Hashing::SHA1::Begin(materialName.getString(), (uint)strlen(materialName.getString()));
	Hashing::SHA1::Update(pHashState, pInputLayout, nNumInputElements * sizeof(pInputLayout[0]));

	Hashing::SHA1 materialHash;
	Hashing::SHA1::Finish(pHashState, materialHash);

	// Check if the material's already been loaded
	RdrMaterialMap::iterator iter = s_materialCache.find(materialHash);
	if (iter != s_materialCache.end())
		return iter->second;

	// Material not cached, create it.
	RdrMaterial* pMaterial = s_materials.allocSafe();

	loadMaterial(materialName, pInputLayout, nNumInputElements, pMaterial);

	s_materialCache.insert(std::make_pair(materialHash, pMaterial));

	return pMaterial;
}