#include "Precompiled.h"
#include "Sprite.h"
#include "Renderer.h"
#include "RdrFrameMem.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Sprite, RdrShaderFlags::None };

	struct SpriteVertex
	{
		Vec2 position;
		Vec2 uv;
	};

	static const RdrVertexInputElement s_vertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RG_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 8, RdrVertexInputClass::PerVertex, 0 }
	};
}

void Sprite::Init(const Vec2 aTexcoords[4], const char* textureName)
{
	SpriteVertex* verts = (SpriteVertex*)RdrFrameMem::Alloc(sizeof(SpriteVertex) * 4);
	verts[0].position = Vec2(0.f, 0.f);
	verts[0].uv = aTexcoords[0];
	verts[1].position = Vec2(1.f, 0.f);
	verts[1].uv = aTexcoords[1];
	verts[2].position = Vec2(0.f, -1.f);
	verts[2].uv = aTexcoords[2];
	verts[3].position = Vec2(1.f, -1.f);
	verts[3].uv = aTexcoords[3];

	uint16* indices = (uint16*)RdrFrameMem::Alloc(sizeof(uint16) * 6);
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 1;
	indices[4] = 3;
	indices[5] = 2;

	RdrResourceCommandList& rResCommandList = g_pRenderer->GetPreFrameCommandList();
	m_hGeo = rResCommandList.CreateGeo(verts, sizeof(SpriteVertex), 4, indices, 6, RdrTopology::TriangleList, Vec3::kZero, Vec3(1.f, 1.f, 0.0f));

	const RdrResourceFormat* pRtvFormats = Renderer::GetStageRTVFormats(RdrRenderStage::kUI);
	uint nNumRtvFormats = Renderer::GetNumStageRTVFormats(RdrRenderStage::kUI);
	const RdrShader* pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_sprite.hlsl", nullptr, 0);

	RdrRasterState rasterState;
	rasterState.bWireframe = false;
	rasterState.bDoubleSided = false;
	rasterState.bEnableMSAA = false;
	rasterState.bUseSlopeScaledDepthBias = false;
	rasterState.bEnableScissor = false;

	m_material.name = "Sprite";
	m_material.CreatePipelineState(RdrShaderMode::Normal,
		kVertexShader, pPixelShader, 
		s_vertexDesc, ARRAY_SIZE(s_vertexDesc), 
		pRtvFormats, nNumRtvFormats,
		RdrBlendMode::kAlpha,
		rasterState,
		RdrDepthStencilState(false, false, RdrComparisonFunc::Never));
	m_material.ahTextures.assign(0, rResCommandList.CreateTextureFromFile(textureName, nullptr));
	m_material.aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false));
}

void Sprite::QueueDraw(RdrAction* pAction, const Vec3& pos, const Vec2& scale, float alpha)
{
	RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp();
	pDrawOp->hGeo = m_hGeo;
	pDrawOp->pMaterial = &m_material;

	const Rect& viewport = pAction->GetViewport();
	uint constantsSize = sizeof(Vec4) * 2;
	Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
	pConstants[0] = Vec4(pos.x - viewport.width * 0.5f, pos.y + viewport.height * 0.5f, pos.z + 1.f, 0.f);
	pConstants[1] = Vec4(scale.x, scale.y, alpha, 0.f);
	pDrawOp->hVsConstants = pAction->GetResCommandList().CreateTempConstantBuffer(pConstants, constantsSize);

	pAction->AddDrawOp(pDrawOp, RdrBucketType::UI);
}
