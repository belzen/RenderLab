#include "Precompiled.h"
#include "Terrain.h"
#include "RdrResourceSystem.h"
#include "RdrShaderSystem.h"
#include "RdrDrawOp.h"
#include "RdrFrameMem.h"
#include "RdrAction.h"
#include "Renderer.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Terrain, RdrShaderFlags::None };
	const RdrTessellationShader kTessellationShader = { RdrTessellationShaderType::Terrain, RdrShaderFlags::None };

	static const RdrVertexInputElement s_terrainVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RG_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RGBA_F32, 1, 0, RdrVertexInputClass::PerInstance, 1 },
	};
}

Terrain::Terrain()
	: m_initialized(false)
{

}

void Terrain::Init(const AssetLib::Terrain& rTerrainAsset)
{
	m_initialized = true;
	m_srcData = rTerrainAsset;

	Vec2* aVertices = (Vec2*)RdrFrameMem::AllocAligned(sizeof(Vec2) * 4, 16);
	aVertices[0] = Vec2(0.f, 0.f);
	aVertices[1] = Vec2(0.f, 1.f);
	aVertices[2] = Vec2(1.f, 1.f);
	aVertices[3] = Vec2(1.f, 0.f);

	m_hGeo = RdrResourceSystem::CreateGeo(
		aVertices, sizeof(aVertices[0]), 4, 
		nullptr, 0,
		RdrTopology::Quad, Vec3(0.f, 0.f, 0.f), Vec3(1.f, 0.f, 1.f),
		CREATE_BACKPOINTER(this));

	// Instance buffer
	float cellSize = 16.f;
	m_gridSize.x = (uint)((m_srcData.cornerMax.x - m_srcData.cornerMin.x) / cellSize);
	m_gridSize.y = (uint)((m_srcData.cornerMax.y - m_srcData.cornerMin.y) / cellSize);
	Vec4* aInstanceData = (Vec4*)RdrFrameMem::AllocAligned(sizeof(Vec4) * m_gridSize.x * m_gridSize.y, 16);
	for (uint x = 0; x < m_gridSize.x; ++x)
	{
		for (uint y = 0; y < m_gridSize.y; ++y)
		{
			aInstanceData[x + y * (int)m_gridSize.x] = Vec4(
				m_srcData.cornerMin.x + x * cellSize, 
				m_srcData.cornerMin.y + y * cellSize,
				cellSize, 
				0.f);
		}
	}
	m_hInstanceData = RdrResourceSystem::CreateVertexBuffer(aInstanceData
		, sizeof(aInstanceData[0]), m_gridSize.x * m_gridSize.y
		, RdrResourceAccessFlags::None
		, CREATE_BACKPOINTER(this));

	// Create shaders
	const RdrResourceFormat* pRtvFormats;
	uint nNumRtvFormats;
	Renderer::GetStageRenderTargetFormats(RdrRenderStage::kScene_GBuffer, &pRtvFormats, &nNumRtvFormats);
	const RdrShader* pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_terrain.hlsl", nullptr, 0);

	const RdrShader* pHullShader = RdrShaderSystem::GetHullShader(kTessellationShader);
	const RdrShader* pDomainShader = RdrShaderSystem::GetDomainShader(kTessellationShader);

	// Rasterizer state
	RdrRasterState rasterState;
	rasterState.bWireframe = false;
	rasterState.bDoubleSided = false;
	rasterState.bEnableMSAA = (g_debugState.msaaLevel > 1);
	rasterState.bUseSlopeScaledDepthBias = false;
	rasterState.bEnableScissor = false;

	// Setup material
	m_material.Init("Terrain", RdrMaterialFlags::NeedsLighting);
	m_material.FillConstantBuffer(nullptr, 16, RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));

	m_material.CreateTessellationPipelineState(RdrShaderMode::Normal,
		kVertexShader, pPixelShader,
		pHullShader, pDomainShader,
		s_terrainVertexDesc, ARRAY_SIZE(s_terrainVertexDesc), 
		pRtvFormats, nNumRtvFormats,
		RdrBlendMode::kOpaque,
		rasterState,
		RdrDepthStencilState(true, true, RdrComparisonFunc::LessEqual));

	RdrResourceHandle ahTextures[2];
	ahTextures[0] = RdrResourceSystem::CreateTextureFromFile("white", nullptr, CREATE_BACKPOINTER(this));
	ahTextures[1] = RdrResourceSystem::CreateTextureFromFile("flat_ddn", nullptr, CREATE_BACKPOINTER(this));
	m_material.SetTextures(0, 2, ahTextures);

	RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
	m_material.SetSamplers(0, 1, &sampler);

	// Tessellation
	RdrTextureInfo heightmapTexInfo;
	RdrResourceHandle hHeightmapTexture = RdrResourceSystem::CreateTextureFromFile(m_srcData.heightmapName, &heightmapTexInfo, CREATE_BACKPOINTER(this));
	m_material.SetTessellationTextures(0, 1, &hHeightmapTexture);

	RdrSamplerState heightmapSampler(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
	m_material.SetTessellationSamplers(0, 1, &heightmapSampler);
	m_heightmapSize = UVec2(heightmapTexInfo.width, heightmapTexInfo.height);

	DsTerrain* pDsConstants = (DsTerrain*)RdrFrameMem::AllocAligned(sizeof(DsTerrain), 16);
	pDsConstants->lods[0].minPos = m_srcData.cornerMin;
	pDsConstants->lods[0].size = (m_srcData.cornerMax - m_srcData.cornerMin);
	pDsConstants->heightmapTexelSize.x = 1.f / m_heightmapSize.x;
	pDsConstants->heightmapTexelSize.y = 1.f / m_heightmapSize.y;
	pDsConstants->heightScale = m_srcData.heightScale;

	m_material.FillTessellationConstantBuffer(pDsConstants, sizeof(DsTerrain), RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));
}

RdrDrawOpSet Terrain::BuildDrawOps(RdrAction* pAction)
{
	// TODO: Remaining terrain tasks:
	//	* Update instance buffer with LOD cells culled to the camera
	//	* Terrain shadows
	//	* Clipmapped height data
	//	* Materials
	if (!m_initialized)
		return RdrDrawOpSet();

	//////////////////////////////////////////////////////////////////////////
	// Fill out the draw op
	RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp(CREATE_BACKPOINTER(this));
	pDrawOp->hGeo = m_hGeo;
	pDrawOp->hCustomInstanceBuffer = m_hInstanceData;
	pDrawOp->pMaterial = &m_material;
	pDrawOp->instanceCount = m_gridSize.x * m_gridSize.y;

	return RdrDrawOpSet(pDrawOp, 1);
}