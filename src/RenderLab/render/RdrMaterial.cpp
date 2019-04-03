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
		const RdrShader* pPixelShader;
		const RdrResourceFormat* pRtvFormats;
		uint nNumRtvFormats;

		if (pMaterial->bForEditor)
		{
			rasterState.bWireframe = false;
			rasterState.bDoubleSided = false;
			rasterState.bEnableMSAA = false;
			rasterState.bUseSlopeScaledDepthBias = false;
			rasterState.bEnableScissor = false;

			pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kUI);
			nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kUI);
			pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);

			pOutMaterial->CreatePipelineState(RdrShaderMode::Normal,
				vertexShader, pPixelShader,
				pInputLayout, nNumInputElements,
				pRtvFormats, nNumRtvFormats,
				RdrBlendMode::kAlpha,
				rasterState,
				RdrDepthStencilState(false, false, RdrComparisonFunc::Never));
		}
		else
		{
			rasterState.bWireframe = false;
			rasterState.bDoubleSided = false;
			rasterState.bEnableMSAA = (g_debugState.msaaLevel > 1);
			rasterState.bUseSlopeScaledDepthBias = false;
			rasterState.bEnableScissor = false;

			pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kScene_GBuffer); 
			nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kScene_GBuffer);

			pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);
			pOutMaterial->CreatePipelineState(RdrShaderMode::Normal,
				vertexShader, pPixelShader,
				pInputLayout, nNumInputElements,
				pRtvFormats, nNumRtvFormats,
				eBlendMode,
				rasterState,
				depthState);
		}


		//////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////
		// Wireframe
		rasterState.bWireframe = true;
		rasterState.bDoubleSided = true;
		rasterState.bEnableMSAA = false;
		rasterState.bUseSlopeScaledDepthBias = false;
		rasterState.bEnableScissor = false;

		pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kPrimary);
		nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kPrimary);

		pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);
		pOutMaterial->CreatePipelineState(RdrShaderMode::Wireframe,
			vertexShader, pPixelShader,
			pInputLayout, nNumInputElements,
			pRtvFormats, nNumRtvFormats,
			eBlendMode,
			rasterState,
			RdrDepthStencilState(true, false, RdrComparisonFunc::Less));

		if (!pOutMaterial->bHasAlpha)
		{
			//////////////////////////////////////////////////////////////////////////
			//////////////////////////////////////////////////////////////////////////
			// Depth Only
			RdrVertexShader depthVertexShader = vertexShader;
			depthVertexShader.flags |= RdrShaderFlags::DepthOnly;

			// Alpha cutout requires a depth-only pixel shader.
			if (pMaterial->bAlphaCutout)
			{
				defines[numDefines++] = "DEPTH_ONLY";
				assert(numDefines <= ARRAY_SIZE(defines));

				pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);
			}
			else
			{
				pPixelShader = nullptr;
			}

			pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kScene_ZPrepass);
			nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kScene_ZPrepass);

			rasterState.bWireframe = false;
			rasterState.bDoubleSided = false;
			rasterState.bEnableMSAA = (g_debugState.msaaLevel > 1);
			rasterState.bUseSlopeScaledDepthBias = false;
			rasterState.bEnableScissor = false;

			pOutMaterial->CreatePipelineState(RdrShaderMode::DepthOnly,
				depthVertexShader, pPixelShader,
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
				depthVertexShader, pPixelShader,
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
			
		RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();
		for (int n = 0; n < pMaterial->texCount; ++n)
		{
			pOutMaterial->ahTextures.assign(n, rResCommandList.CreateTextureFromFile(pMaterial->pTextureNames[n], nullptr, pOutMaterial));
			pOutMaterial->aSamplers.assign(n, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false));
		}

		// DONOTCHECKIN2 - Copy textures and sampler descriptors to descriptor table


		uint constantsSize = sizeof(MaterialParams);
		MaterialParams* pConstants = (MaterialParams*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pConstants->metalness = pMaterial->metalness;
		pConstants->roughness = pMaterial->roughness;
		pConstants->color = pMaterial->color.asFloat4();
		pOutMaterial->hConstants = rResCommandList.CreateConstantBuffer(pConstants, constantsSize, RdrResourceAccessFlags::CpuRO_GpuRO, pOutMaterial);

		pOutMaterial->name = materialName;
	}
}

void RdrMaterial::CreatePipelineState(
	RdrShaderMode eMode,
	const RdrVertexShader& vertexShader, const RdrShader* pPixelShader,
	const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
	const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
	const RdrBlendMode eBlendMode,
	const RdrRasterState& rasterState,
	const RdrDepthStencilState& depthStencilState)
{
	const RdrShader* pVertexShader = RdrShaderSystem::GetVertexShader(vertexShader);

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