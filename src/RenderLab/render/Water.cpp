#include "Precompiled.h"
#include "Water.h"
#include "RdrResourceSystem.h"
#include "RdrShaderSystem.h"
#include "RdrDrawOp.h"
#include "RdrFrameMem.h"
#include "RdrAction.h"
#include "Renderer.h"

namespace
{
	// Grid shader/geo setup
	static const RdrVertexShader kGridVertexShader = { RdrVertexShaderType::Water, RdrShaderFlags::None };
	static const RdrVertexInputElement kGridVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RG_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 8, RdrVertexInputClass::PerVertex, 0 },
	};

	struct GridVertex
	{
		Vec2 position;
		Vec2 texcoord;
	};
}

void Water::Init(int gridSize)
{
	m_gridSize = gridSize;

	m_aParticles = new WaveParticle[512];
	m_numParticles = 0;
	m_maxParticles = 512;

	InitGridResources();
	InitParticleResources();

	m_initialized = true;
}

void Water::InitGridResources()
{
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();

	// Create the water grid geometry
	int numVerts = m_gridSize * m_gridSize;
	GridVertex* aVertices = (GridVertex*)RdrFrameMem::AllocAligned(sizeof(GridVertex) * numVerts, 16);

	for (int x = 0; x < m_gridSize; ++x)
	{
		for (int z = 0; z < m_gridSize; ++z)
		{
			aVertices[x + z * m_gridSize].position = Vec2((float)x, (float)z);
			aVertices[x + z * m_gridSize].texcoord = Vec2(((float)x / m_gridSize) + 0.5f, ((float)z / m_gridSize) + 0.5f);
		}
	}

	const int numColumns = (m_gridSize - 1);
	const int numRowTris = numColumns * 2;
	const int numTriangles = (m_gridSize - 1) * numRowTris;
	uint16* aIndices = (uint16*)RdrFrameMem::AllocAligned(sizeof(uint16) * numTriangles * 3, 16);

	for (int m = 0; m < m_gridSize - 1; ++m)
	{
		for (int n = 0; n < m_gridSize - 1; ++n)
		{
			int idx = (numRowTris * m * 3) + (n * 2 * 3);
			aIndices[idx + 0] = (m * m_gridSize) + n;
			aIndices[idx + 1] = ((m + 1) * m_gridSize) + n;
			aIndices[idx + 2] = (m * m_gridSize) + n + 1;

			aIndices[idx + 3] = (m * m_gridSize) + n + 1;
			aIndices[idx + 4] = ((m + 1) * m_gridSize) + n;
			aIndices[idx + 5] = ((m + 1) * m_gridSize) + n + 1;
		}
	}

	m_hGridGeo = RdrResourceSystem::CreateGeo(
		aVertices, sizeof(aVertices[0]), numVerts,
		aIndices, numTriangles * 3,
		RdrTopology::TriangleList, Vec3(0.f, 0.f, 0.f), Vec3(1.f, 0.f, 1.f), CREATE_BACKPOINTER(this));

	const RdrResourceFormat* pRtvFormats;
	uint nNumRtvFormats;
	Renderer::GetStageRenderTargetFormats(RdrRenderStage::kScene_GBuffer, &pRtvFormats, &nNumRtvFormats);

	const RdrShader* pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_water.hlsl", nullptr, 0);

	RdrRasterState rasterState;
	rasterState.bWireframe = false;
	rasterState.bDoubleSided = false;
	rasterState.bEnableMSAA = (g_debugState.msaaLevel > 1);
	rasterState.bUseSlopeScaledDepthBias = false;
	rasterState.bEnableScissor = false;

	// Setup pixel material
	m_material.Init("Water", RdrMaterialFlags::None);
	m_material.CreatePipelineState(RdrShaderMode::Normal,
		kGridVertexShader, pPixelShader,
		kGridVertexDesc, ARRAY_SIZE(kGridVertexDesc),
		pRtvFormats, nNumRtvFormats,
		RdrBlendMode::kAlpha,
		rasterState,
		RdrDepthStencilState(false, false, RdrComparisonFunc::Always));

	// Setup per-object constants
	Matrix44 mtxWorld = Matrix44Translation(Vec3(0.f, 5.f, 0.f));
	uint constantsSize = sizeof(VsPerObject);
	VsPerObject* pVsPerObject = (VsPerObject*)RdrFrameMem::AllocAligned(constantsSize, 16);
	pVsPerObject->mtxWorld = Matrix44Transpose(mtxWorld);

	m_hVsPerObjectConstantBuffer = RdrResourceSystem::CreateUpdateConstantBuffer(m_hVsPerObjectConstantBuffer,
		pVsPerObject, constantsSize, RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));

}

void Water::InitParticleResources()
{
	m_hParticleBuffer = RdrResourceSystem::CreateStructuredBuffer(m_aParticles, m_maxParticles, sizeof(WaveParticle), RdrResourceAccessFlags::CpuRO_GpuRW, CREATE_BACKPOINTER(this));
}

void Water::Update()
{
	if (!m_initialized)
		return;

}

RdrDrawOpSet Water::BuildDrawOps(RdrAction* pAction)
{
	if (!m_initialized)
		return RdrDrawOpSet();

	// Draw the water grid
	RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp(CREATE_BACKPOINTER(this));
	pDrawOp->hVsConstants = m_hVsPerObjectConstantBuffer;
	pDrawOp->hGeo = m_hGridGeo;
	pDrawOp->pMaterial = &m_material;

	return RdrDrawOpSet(pDrawOp, 1);
}

RdrComputeOp Water::BuildComputeOps(RdrAction* pAction)
{
	// Update particles
	g_pRenderer->GetResourceCommandList().UpdateBuffer(m_hParticleBuffer, m_aParticles, m_numParticles, CREATE_BACKPOINTER(this));

	// Issue particle compute shader
	return RdrComputeOp();
}