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

		// Setup vertex shader
		RdrVertexShader vertexShader = {};
		if (_stricmp(pMaterial->vertexShader, "model") == 0)
		{
			vertexShader.eType = RdrVertexShaderType::Model;
		}
		else if (_stricmp(pMaterial->vertexShader, "sky") == 0)
		{
			vertexShader.eType = RdrVertexShaderType::Sky;
		}

		// Apply flags
		RdrMaterialFlags materialFlags = RdrMaterialFlags::None;

		const char* defines[32] = { 0 };
		uint numDefines = 0;
		if (pMaterial->bAlphaCutout)
		{
			vertexShader.flags |= RdrShaderFlags::AlphaCutout;
			materialFlags |= RdrMaterialFlags::AlphaCutout;
			defines[numDefines++] = "ALPHA_CUTOUT";
		}

		if (pMaterial->bNeedsLighting)
		{
			materialFlags |= RdrMaterialFlags::NeedsLighting;
		}

		if (pMaterial->color.a < 1.f)
		{
			materialFlags |= RdrMaterialFlags::HasAlpha;
			defines[numDefines++] = "HAS_ALPHA";
		}

		for (int i = 0; i < pMaterial->shaderDefsCount; ++i)
		{
			defines[numDefines++] = pMaterial->pShaderDefs[i];
		}

		// Normal PSO
		assert(numDefines <= ARRAY_SIZE(defines));

		RdrDepthStencilState depthState = RdrDepthStencilState(true, false, RdrComparisonFunc::Equal);
		RdrBlendMode eBlendMode = RdrBlendMode::kOpaque;
		if (IsFlagSet(materialFlags, RdrMaterialFlags::HasAlpha))
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

			Renderer::GetStageRenderTargetFormats(RdrRenderStage::kUI, &pRtvFormats, &nNumRtvFormats);

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

			Renderer::GetStageRenderTargetFormats(RdrRenderStage::kScene_GBuffer, &pRtvFormats, &nNumRtvFormats);

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

		Renderer::GetStageRenderTargetFormats(RdrRenderStage::kPrimary, &pRtvFormats, &nNumRtvFormats);

		pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);
		pOutMaterial->CreatePipelineState(RdrShaderMode::Wireframe,
			vertexShader, pPixelShader,
			pInputLayout, nNumInputElements,
			pRtvFormats, nNumRtvFormats,
			eBlendMode,
			rasterState,
			RdrDepthStencilState(true, false, RdrComparisonFunc::Less));

		if (!pOutMaterial->HasAlpha())
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

			Renderer::GetStageRenderTargetFormats(RdrRenderStage::kScene_ZPrepass, &pRtvFormats, &nNumRtvFormats);

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
			Renderer::GetStageRenderTargetFormats(RdrRenderStage::kShadowMap, &pRtvFormats, &nNumRtvFormats);

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
		pOutMaterial->Init(materialName, materialFlags);
			
		RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();
		RdrResourceHandle ahTextures[RdrMaterial::kMaxTextures];
		RdrSamplerState aSamplers[RdrMaterial::kMaxTextures];
		for (int n = 0; n < pMaterial->texCount; ++n)
		{
			ahTextures[n] = RdrResourceSystem::CreateTextureFromFile(pMaterial->pTextureNames[n], nullptr, CREATE_BACKPOINTER(pOutMaterial));
			aSamplers[n] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
		}

		if (pMaterial->texCount > 0)
		{
			pOutMaterial->SetTextures(0, pMaterial->texCount, ahTextures);
			pOutMaterial->SetSamplers(0, pMaterial->texCount, aSamplers);
		}

		uint constantsSize = sizeof(MaterialParams);
		MaterialParams* pConstants = (MaterialParams*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pConstants->metalness = pMaterial->metalness;
		pConstants->roughness = pMaterial->roughness;
		pConstants->color = pMaterial->color.asFloat4();

		pOutMaterial->FillConstantBuffer(pConstants, constantsSize, RdrResourceAccessFlags::CpuRO_GpuRO, CREATE_BACKPOINTER(pOutMaterial));
	}
}

RdrMaterial::RdrMaterial()
	: m_name()
	, m_flags(RdrMaterialFlags::None)
	, m_hConstants(0)
	, m_pResourceDescriptorTable(nullptr)
	, m_pSamplerDescriptorTable(nullptr)
{
}

RdrMaterial::~RdrMaterial()
{
	Cleanup();
}

void RdrMaterial::Init(const CachedString& name, RdrMaterialFlags flags)
{
	m_name = name;
	m_flags = flags;
}

void RdrMaterial::Cleanup()
{
	if (m_hConstants)
	{
		g_pRenderer->GetResourceCommandList().ReleaseConstantBuffer(m_hConstants, CREATE_BACKPOINTER(this));
		m_hConstants = 0;
	}

	for (RdrPipelineState& pipelineState : m_hPipelineStates)
	{
		//donotcheckin destroy
	}

	// Note: Intentionally not destroying resources as they are managed elsewhere

	if (m_pResourceDescriptorTable)
	{
		g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(m_pResourceDescriptorTable, CREATE_BACKPOINTER(this));
		m_pResourceDescriptorTable = nullptr;
	}

	if (m_pSamplerDescriptorTable)
	{
		g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(m_pSamplerDescriptorTable, CREATE_BACKPOINTER(this));
		m_pSamplerDescriptorTable = nullptr;
	}

	//////////////////////////////////////////////////////////////////////////
	// Tessellation
	if (m_tessellation.hDsConstants)
	{
		g_pRenderer->GetResourceCommandList().ReleaseConstantBuffer(m_tessellation.hDsConstants, CREATE_BACKPOINTER(this));
		m_tessellation.hDsConstants = 0;
	}

	//donotcheckin resource refcounts
	/*
	uint nNumResources = m_tessellation.ahResources.size();
	for (uint i = 0; i < nNumResources; ++i)
	{
		RdrResourceHandle hResource = m_tessellation.ahResources.get(i);
		if (hResource)
		{
			g_pRenderer->GetResourceCommandList().ReleaseResource(hResource, CREATE_BACKPOINTER(this));
			m_tessellation.ahResources.assign(i, 0);
		}
	}
	*/

	if (m_tessellation.pResourceDescriptorTable)
	{
		g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(m_tessellation.pResourceDescriptorTable, CREATE_BACKPOINTER(this));
		m_tessellation.pResourceDescriptorTable = nullptr;
	}

	if (m_tessellation.pSamplerDescriptorTable)
	{
		g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(m_tessellation.pSamplerDescriptorTable, CREATE_BACKPOINTER(this));
		m_tessellation.pSamplerDescriptorTable = nullptr;
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

	m_hPipelineStates[(int)eMode] = g_pRenderer->GetContext()->CreateGraphicsPipelineState(
		pVertexShader, pPixelShader,
		nullptr, nullptr,
		pInputLayoutElements, nNumInputElements,
		pRtvFormats, nNumRtvFormats,
		eBlendMode,
		rasterState,
		depthStencilState);

	m_activeShaderStages[(int)eMode] = RdrShaderStageFlags::Vertex;
	if (pPixelShader)
	{
		m_activeShaderStages[(int)eMode] |= RdrShaderStageFlags::Pixel;
	}
}

void RdrMaterial::CreateTessellationPipelineState(
	RdrShaderMode eMode,
	const RdrVertexShader& vertexShader, const RdrShader* pPixelShader,
	const RdrShader* pHullShader, const RdrShader* pDomainShader,
	const RdrVertexInputElement* pInputLayoutElements, uint nNumInputElements,
	const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
	const RdrBlendMode eBlendMode,
	const RdrRasterState& rasterState,
	const RdrDepthStencilState& depthStencilState)
{
	const RdrShader* pVertexShader = RdrShaderSystem::GetVertexShader(vertexShader);

	m_hPipelineStates[(int)eMode] = g_pRenderer->GetContext()->CreateGraphicsPipelineState(
		pVertexShader, pPixelShader,
		pHullShader, pDomainShader,
		pInputLayoutElements, nNumInputElements,
		pRtvFormats, nNumRtvFormats,
		eBlendMode,
		rasterState,
		depthStencilState);

	m_activeShaderStages[(int)eMode] = RdrShaderStageFlags::Vertex | RdrShaderStageFlags::Hull | RdrShaderStageFlags::Domain;
	if (pPixelShader)
	{
		m_activeShaderStages[(int)eMode] |= RdrShaderStageFlags::Pixel;
	}
}

void RdrMaterial::SetTextures(uint startSlot, uint count, const RdrResourceHandle* ahTextures)
{
	for (uint i = 0; i < count; ++i)
	{
		m_ahTextures.assign(i + startSlot, ahTextures[i]);
	}

	if (m_pResourceDescriptorTable)
	{
		g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(m_pResourceDescriptorTable, CREATE_BACKPOINTER(this));
	}
	m_pResourceDescriptorTable = RdrResourceSystem::CreateShaderResourceViewTable(m_ahTextures.data(), m_ahTextures.size(), CREATE_BACKPOINTER(this));
}

void RdrMaterial::SetSamplers(uint startSlot, uint count, const RdrSamplerState* aSamplers)
{
	for (uint i = 0; i < count; ++i)
	{
		m_aSamplers.assign(i + startSlot, aSamplers[i]);
	}

	if (m_pSamplerDescriptorTable)
	{
		g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(m_pSamplerDescriptorTable, CREATE_BACKPOINTER(this));
	}
	m_pSamplerDescriptorTable = RdrResourceSystem::CreateSamplerTable(m_aSamplers.data(), m_aSamplers.size(), CREATE_BACKPOINTER(this));
}

void RdrMaterial::FillConstantBuffer(void* pData, uint nDataSize, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	m_hConstants = RdrResourceSystem::CreateConstantBuffer(pData, nDataSize, accessFlags, debug);
}

void RdrMaterial::SetTessellationTextures(uint startSlot, uint count, const RdrResourceHandle* ahTextures)
{
	for (uint i = 0; i < count; ++i)
	{
		m_tessellation.ahResources.assign(i + startSlot, ahTextures[i]);
	}

	if (m_tessellation.pResourceDescriptorTable)
	{
		g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(m_tessellation.pResourceDescriptorTable, CREATE_BACKPOINTER(this));
	}
	m_tessellation.pResourceDescriptorTable = RdrResourceSystem::CreateShaderResourceViewTable(m_tessellation.ahResources.data(), m_tessellation.ahResources.size(), CREATE_BACKPOINTER(this));
}

void RdrMaterial::SetTessellationSamplers(uint startSlot, uint count, const RdrSamplerState* aSamplers)
{
	for (uint i = 0; i < count; ++i)
	{
		m_tessellation.aSamplers.assign(i + startSlot, aSamplers[i]);
	}

	if (m_tessellation.pSamplerDescriptorTable)
	{
		g_pRenderer->GetResourceCommandList().ReleaseDescriptorTable(m_tessellation.pSamplerDescriptorTable, CREATE_BACKPOINTER(this));
	}
	m_tessellation.pSamplerDescriptorTable = RdrResourceSystem::CreateSamplerTable(m_tessellation.aSamplers.data(), m_tessellation.aSamplers.size(), CREATE_BACKPOINTER(this));
}

void RdrMaterial::FillTessellationConstantBuffer(void* pData, uint nDataSize, RdrResourceAccessFlags accessFlags, const RdrDebugBackpointer& debug)
{
	m_tessellation.hDsConstants = RdrResourceSystem::CreateConstantBuffer(pData, nDataSize, accessFlags, debug);
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
