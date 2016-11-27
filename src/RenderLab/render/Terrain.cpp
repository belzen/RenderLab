#include "Precompiled.h"
#include "Terrain.h"
#include "RdrResourceSystem.h"
#include "RdrShaderSystem.h"
#include "RdrDrawOp.h"
#include "RdrFrameMem.h"
#include "RdrFrameState.h"
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
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetPreFrameCommandList();

	m_initialized = true;
	m_srcData = rTerrainAsset;

	Vec2* aVertices = (Vec2*)RdrFrameMem::AllocAligned(sizeof(Vec2) * 4, 16);
	aVertices[0] = Vec2(0.f, 0.f);
	aVertices[1] = Vec2(0.f, 1.f);
	aVertices[2] = Vec2(1.f, 1.f);
	aVertices[3] = Vec2(1.f, 0.f);

	m_hInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_terrainVertexDesc, ARRAY_SIZE(s_terrainVertexDesc));
	m_hGeo = rResCommandList.CreateGeo(
		aVertices, sizeof(aVertices[0]), 4, 
		nullptr, 0,
		RdrTopology::Quad, Vec3(0.f, 0.f, 0.f), Vec3(1.f, 0.f, 1.f));

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
	m_hInstanceData = rResCommandList.CreateVertexBuffer(aInstanceData, sizeof(aInstanceData[0]), m_gridSize.x * m_gridSize.y, RdrResourceUsage::Immutable);

	// Tessellation material
	RdrTextureInfo heightmapTexInfo;
	m_tessMaterial.shader = kTessellationShader;
	m_tessMaterial.ahResources.assign(0, rResCommandList.CreateTextureFromFile(m_srcData.heightmapName, &heightmapTexInfo));
	m_tessMaterial.aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false));

	m_heightmapSize = UVec2(heightmapTexInfo.width, heightmapTexInfo.height);

	// Setup pixel material
	memset(&m_material, 0, sizeof(m_material));
	m_material.bNeedsLighting = true;
	m_material.hPixelShaders[(int)RdrShaderMode::Normal] = RdrShaderSystem::CreatePixelShaderFromFile("p_terrain.hlsl", nullptr, 0);
	m_material.ahTextures.assign(0, rResCommandList.CreateTextureFromFile("white", nullptr));
	m_material.ahTextures.assign(1, rResCommandList.CreateTextureFromFile("test_ddn", nullptr));
	m_material.aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false));
}

void Terrain::QueueDraw(RdrDrawBuckets* pDrawBuckets, const Camera& rCamera)
{
	// TODO: Remaining terrain tasks:
	//	* Update instance buffer with LOD cells culled to the camera
	//	* Terrain shadows
	//	* Clipmapped height data
	//	* Materials
	if (!m_initialized)
		return;

	DsTerrain* pDsConstants = (DsTerrain*)RdrFrameMem::AllocAligned(sizeof(DsTerrain), 16);
	pDsConstants->lods[0].minPos = m_srcData.cornerMin;
	pDsConstants->lods[0].size = (m_srcData.cornerMax - m_srcData.cornerMin);
	pDsConstants->heightmapTexelSize.x = 1.f / m_heightmapSize.x;
	pDsConstants->heightmapTexelSize.y = 1.f / m_heightmapSize.y;
	pDsConstants->heightScale = m_srcData.heightScale;
	m_tessMaterial.hDsConstants = g_pRenderer->GetActionCommandList()->CreateUpdateConstantBuffer(m_tessMaterial.hDsConstants,
		pDsConstants, sizeof(DsTerrain), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	
	//////////////////////////////////////////////////////////////////////////
	// Fill out the draw op
	RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp();
	pDrawOp->hGeo = m_hGeo;
	pDrawOp->hCustomInstanceBuffer = m_hInstanceData;
	pDrawOp->hInputLayout = m_hInputLayout;
	pDrawOp->vertexShader = kVertexShader;
	pDrawOp->pMaterial = &m_material;
	pDrawOp->pTessellationMaterial = &m_tessMaterial;
	pDrawOp->instanceCount = m_gridSize.x * m_gridSize.y;

	pDrawBuckets->AddDrawOp(pDrawOp, RdrBucketType::Opaque);
}