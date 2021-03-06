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

	m_hGeo = RdrResourceSystem::CreateGeo(verts, sizeof(SpriteVertex), 4, indices, 6, RdrIndexBufferFormat::R16_UINT, RdrTopology::TriangleList, Vec3::kZero, Vec3(1.f, 1.f, 0.0f), CREATE_BACKPOINTER(this));

	const RdrResourceFormat* pRtvFormats;
	uint nNumRtvFormats;
	Renderer::GetStageRenderTargetFormats(RdrRenderStage::kUI, &pRtvFormats, &nNumRtvFormats);

	const RdrShader* pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_sprite.hlsl", nullptr, 0);

	RdrRasterState rasterState;
	rasterState.bWireframe = false;
	rasterState.bDoubleSided = false;
	rasterState.bEnableMSAA = false;
	rasterState.bUseSlopeScaledDepthBias = false;
	rasterState.bEnableScissor = false;

	m_material.Init("Sprite", RdrMaterialFlags::None);
	m_material.CreatePipelineState(RdrShaderMode::Normal,
		kVertexShader, pPixelShader, 
		s_vertexDesc, ARRAY_SIZE(s_vertexDesc), 
		pRtvFormats, nNumRtvFormats,
		RdrBlendMode::kAlpha,
		rasterState,
		RdrDepthStencilState(false, false, RdrComparisonFunc::Never));

	RdrResourceHandle hTexture = RdrResourceSystem::CreateTextureFromFile(textureName, nullptr, CREATE_BACKPOINTER(this));
	m_material.SetTextures(0, 1, &hTexture);

	RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
	m_material.SetSamplers(0, 1, &sampler);
}

void Sprite::QueueDraw(RdrAction* pAction, const Vec3& pos, const Vec2& scale, float alpha)
{
	RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp(CREATE_BACKPOINTER(this));
	pDrawOp->hGeo = m_hGeo;
	pDrawOp->pMaterial = &m_material;

	const Rect& viewport = pAction->GetViewport();
	uint constantsSize = sizeof(Vec4) * 2;
	Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
	pConstants[0] = Vec4(pos.x - viewport.width * 0.5f, pos.y + viewport.height * 0.5f, pos.z + 1.f, 0.f);
	pConstants[1] = Vec4(scale.x, scale.y, alpha, 0.f);
	pDrawOp->hVsConstants = RdrResourceSystem::CreateTempConstantBuffer(pConstants, constantsSize, CREATE_BACKPOINTER(this));

	pAction->AddDrawOp(pDrawOp, RdrBucketType::UI);
}
