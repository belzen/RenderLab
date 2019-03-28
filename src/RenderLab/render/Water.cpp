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
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetPreFrameCommandList();

	// Create the water grid geometry
	int numVerts = m_gridSize * m_gridSize;
	GridVertex* aVertices = (GridVertex*)RdrFrameMem::AllocAligned(sizeof(GridVertex) * numVerts, 16);

	for (int x = 0; x < m_gridSize; ++x)
	{
		for (int z = 0; z < m_gridSize; ++z)
		{
			aVertices[x + z * m_gridSize].position = Vec2(x, z);
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

	m_hGridInputLayout = RdrShaderSystem::CreateInputLayout(kGridVertexShader, kGridVertexDesc, ARRAY_SIZE(kGridVertexDesc));
	m_hGridGeo = rResCommandList.CreateGeo(
		aVertices, sizeof(aVertices[0]), numVerts,
		aIndices, numTriangles * 3,
		RdrTopology::TriangleList, Vec3(0.f, 0.f, 0.f), Vec3(1.f, 0.f, 1.f));


	// Setup pixel material
	memset(&m_gridMaterial, 0, sizeof(m_gridMaterial));
	m_gridMaterial.bNeedsLighting = true;
	m_gridMaterial.hConstants = rResCommandList.CreateConstantBuffer(nullptr, 16, RdrCpuAccessFlags::None, RdrResourceUsage::Default);
	m_gridMaterial.hPixelShaders[(int)RdrShaderMode::Normal] = RdrShaderSystem::CreatePixelShaderFromFile("p_water.hlsl", nullptr, 0);

	// Setup per-object constants
	Matrix44 mtxWorld = Matrix44Translation(Vec3(0.f, 5.f, 0.f));
	uint constantsSize = sizeof(VsPerObject);
	VsPerObject* pVsPerObject = (VsPerObject*)RdrFrameMem::AllocAligned(constantsSize, 16);
	pVsPerObject->mtxWorld = Matrix44Transpose(mtxWorld);

	m_hVsPerObjectConstantBuffer = g_pRenderer->GetPreFrameCommandList().CreateUpdateConstantBuffer(m_hVsPerObjectConstantBuffer,
		pVsPerObject, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);

}

void Water::InitParticleResources()
{
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetPreFrameCommandList();
	m_hParticleBuffer = rResCommandList.CreateStructuredBuffer(m_aParticles, m_maxParticles, sizeof(WaveParticle), RdrResourceUsage::Dynamic);
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
	RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp();
	pDrawOp->hVsConstants = m_hVsPerObjectConstantBuffer;
	pDrawOp->hGeo = m_hGridGeo;
	pDrawOp->hInputLayout = m_hGridInputLayout;
	pDrawOp->vertexShader = kGridVertexShader;
	pDrawOp->pMaterial = &m_gridMaterial;

	return RdrDrawOpSet(pDrawOp, 1);
}

RdrComputeOp Water::BuildComputeOps(RdrAction* pAction)
{
	// Update particles
	pAction->GetResCommandList().UpdateBuffer(m_hParticleBuffer, m_aParticles, m_numParticles);

	// Issue particle compute shader
	return RdrComputeOp();
}