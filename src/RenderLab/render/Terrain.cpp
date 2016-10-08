#include "Precompiled.h"
#include "Terrain.h"
#include "RdrResourceSystem.h"
#include "RdrShaderSystem.h"
#include "RdrDrawOp.h"
#include "RdrScratchMem.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Terrain, RdrShaderFlags::None };
	const RdrTessellationShader kTessellationShader = { RdrTessellationShaderType::Terrain, RdrShaderFlags::None };

	static const RdrVertexInputElement s_terrainVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RG_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RGB_F32, 1, 0, RdrVertexInputClass::PerInstance, 1 },
	};
}

void Terrain::Init()
{
	Vec2* aVertices = (Vec2*)RdrScratchMem::AllocAligned(sizeof(Vec2) * 4, 16);
	aVertices[0] = Vec2(0.f, 0.f);
	aVertices[1] = Vec2(0.f, 1.f);
	aVertices[2] = Vec2(1.f, 1.f);
	aVertices[3] = Vec2(1.f, 0.f);

	m_hGeo = RdrResourceSystem::CreateGeo(
		aVertices, sizeof(aVertices[0]), 4, 
		nullptr, 0,
		RdrTopology::Quad, Vec3(0.f, 0.f, 0.f), Vec3(1.f, 0.f, 1.f));

	int gridSize = 8;
	float cellSize = 10.f;
	Vec3* aInstanceData = (Vec3*)RdrScratchMem::AllocAligned(sizeof(Vec3) * gridSize * gridSize, 16);
	for (int x = 0; x < gridSize; ++x)
	{
		for (int y = 0; y < gridSize; ++y)
		{
			aInstanceData[x + y * gridSize] = Vec3(x * cellSize, y * cellSize, cellSize);
		}
	}
	m_hInstanceData = RdrResourceSystem::CreateVertexBuffer(aInstanceData, sizeof(aInstanceData[0]), gridSize * gridSize, RdrResourceUsage::Immutable);

	m_hInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_terrainVertexDesc, ARRAY_SIZE(s_terrainVertexDesc));

	// Setup material
	memset(&m_material, 0, sizeof(m_material));
	m_material.bNeedsLighting = true;
	m_material.hPixelShaders[(int)RdrShaderMode::Normal] = RdrShaderSystem::CreatePixelShaderFromFile("p_terrain.hlsl", nullptr, 0);
	m_material.hTextures[0] = RdrResourceSystem::CreateTextureFromFile("white", nullptr);
	m_material.hTextures[1] = RdrResourceSystem::CreateTextureFromFile("test_ddn", nullptr);
	m_material.samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
	m_material.samplers[1] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
	m_material.texCount = 2;
}

void Terrain::PrepareDraw()
{
	if (!m_pDrawOp)
	{
		m_pDrawOp = RdrDrawOp::Allocate();
		m_pDrawOp->hGeo = m_hGeo;
		m_pDrawOp->hCustomInstanceBuffer = m_hInstanceData;
		m_pDrawOp->hInputLayout = m_hInputLayout;
		m_pDrawOp->vertexShader = kVertexShader;
		m_pDrawOp->tessellationShader = kTessellationShader;
		m_pDrawOp->pMaterial = &m_material;
		m_pDrawOp->instanceCount = 8 * 8;
	}
}