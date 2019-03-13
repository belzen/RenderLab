#include "Precompiled.h"
#include "Decal.h"
#include "ComponentAllocator.h"
#include "render/ModelData.h"
#include "render/RdrFrameMem.h"
#include "render/RdrDrawOp.h"
#include "render/RdrResourceSystem.h"
#include "render/RdrShaderSystem.h"
#include "render/RdrAction.h"
#include "render/Renderer.h"

namespace
{
	RdrGeoHandle s_hDecalBoxGeo = 0;
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Decal, RdrShaderFlags::None };

	static const RdrVertexInputElement s_decalVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 }
	};

	struct DecalVertex
	{
		Vec3 position;
	};

	RdrGeoHandle createDecalGeo()
	{
		const Vec3 boundsMin = -Vec3::kOne;
		const Vec3 boundsMax = Vec3::kOne;

		static DecalVertex aVertices[] = 
		{
			{ Vec3(-1.f, -1.f, -1.f) },
			{ Vec3( 1.f, -1.f, -1.f) },
			{ Vec3( 1.f, -1.f,  1.f) },
			{ Vec3(-1.f, -1.f,  1.f) },
			{ Vec3(-1.f,  1.f, -1.f) },
			{ Vec3( 1.f,  1.f, -1.f) },
			{ Vec3( 1.f,  1.f,  1.f) },
			{ Vec3(-1.f,  1.f,  1.f) },
		};

		// Vertex ordering here is random.  Splats are double-sided, so it winding order doesn't matter.
		static uint16 aIndices[] = {
			0, 1, 2, 0, 2, 3, // Bottom
			4, 5, 6, 4, 6, 7, // Top
			6, 5, 1, 6, 1, 2, // Right
			4, 7, 3, 4, 3, 0, // Left
			7, 6, 2, 7, 2, 3, // Front
			5, 4, 0, 5, 0, 1  // Back
		};

		return g_pRenderer->GetPreFrameCommandList().CreateGeo(aVertices, sizeof(DecalVertex), ARRAY_SIZE(aVertices),
			aIndices, ARRAY_SIZE(aIndices), RdrTopology::TriangleList, boundsMin, boundsMax);
	}
}


Decal* Decal::Create(IComponentAllocator* pAllocator, const CachedString& textureName)
{
	if (!s_hDecalBoxGeo)
	{
		s_hDecalBoxGeo = createDecalGeo();
	}

	Decal* pDecal = pAllocator->AllocDecal();

	const RdrResourceFormat* pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kScene_GBuffer);
	uint nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kScene_GBuffer);
	RdrShaderHandle hPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_decal.hlsl", nullptr, 0);

	RdrRasterState rasterState;
	rasterState.bWireframe = false;
	rasterState.bDoubleSided = true;
	rasterState.bEnableMSAA = false;//donotcheckin - match g_debugState
	rasterState.bUseSlopeScaledDepthBias = false;
	rasterState.bEnableScissor = false;

	// Setup material
	pDecal->m_material.name = "Decal";
	pDecal->m_material.bHasAlpha = true;
	pDecal->m_material.CreatePipelineState(RdrShaderMode::Normal,
		kVertexShader, hPixelShader, 
		s_decalVertexDesc, ARRAY_SIZE(s_decalVertexDesc), 
		pRtvFormats, nNumRtvFormats,
		RdrBlendMode::kAlpha,
		rasterState,
		RdrDepthStencilState(false, false, RdrComparisonFunc::Always));
	pDecal->SetTexture(textureName);
	pDecal->m_material.ahTextures.assign(1, RdrGlobalResourceHandles::kDepthBuffer);
	pDecal->m_material.aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false));

	return pDecal;
}

void Decal::Release()
{
	if (m_hVsPerObjectConstantBuffer)
	{
		g_pRenderer->GetPreFrameCommandList().ReleaseConstantBuffer(m_hVsPerObjectConstantBuffer);
		m_hVsPerObjectConstantBuffer = 0;
	}
	if (m_hPsMaterialBuffer)
	{
		g_pRenderer->GetPreFrameCommandList().ReleaseConstantBuffer(m_hPsMaterialBuffer);
		m_hPsMaterialBuffer = 0;
	}

	m_pAllocator->ReleaseComponent(this);
}

void Decal::OnAttached(Entity* pEntity)
{
	m_pEntity = pEntity;
}

void Decal::OnDetached(Entity* pEntity)
{
	m_pEntity = nullptr;
}

RdrDrawOpSet Decal::BuildDrawOps(RdrAction* pAction)
{
	if (!m_hVsPerObjectConstantBuffer || m_lastTransformId != m_pEntity->GetTransformId())
	{
		Matrix44 mtxWorld = m_pEntity->GetTransform();

		// VS per-object
		uint constantsSize = sizeof(VsPerObject);
		VsPerObject* pVsPerObject = (VsPerObject*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pVsPerObject->mtxWorld = Matrix44Transpose(mtxWorld);

		m_hVsPerObjectConstantBuffer = pAction->GetResCommandList().CreateUpdateConstantBuffer(m_hVsPerObjectConstantBuffer,
			pVsPerObject, constantsSize, RdrResourceAccessFlags::CpuRW_GpuRO);

		// Decal material
		constantsSize = sizeof(DecalMaterialParams);
		DecalMaterialParams* pPsMaterial = (DecalMaterialParams*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pPsMaterial->mtxInvWorld = Matrix44Inverse(mtxWorld);
		pPsMaterial->mtxInvWorld = Matrix44Transpose(pPsMaterial->mtxInvWorld);

		m_hPsMaterialBuffer = pAction->GetResCommandList().CreateUpdateConstantBuffer(m_hPsMaterialBuffer,
			pPsMaterial, constantsSize, RdrResourceAccessFlags::CpuRW_GpuRO);
		m_material.hConstants = m_hPsMaterialBuffer;

		///
		m_lastTransformId = m_pEntity->GetTransformId();
	}

	///
	RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp();

	pDrawOp->hVsConstants = m_hVsPerObjectConstantBuffer;
	pDrawOp->pMaterial = &m_material;

	pDrawOp->hGeo = s_hDecalBoxGeo;
	pDrawOp->bHasAlpha = true;

	return RdrDrawOpSet(pDrawOp, 1);
}

void Decal::SetTexture(const CachedString& textureName)
{
	m_textureName = textureName;
	m_hTexture = g_pRenderer->GetPreFrameCommandList().CreateTextureFromFile(textureName, nullptr);
	m_material.ahTextures.assign(0, m_hTexture);
}

const CachedString& Decal::GetTexture() const
{
	return m_textureName;
}