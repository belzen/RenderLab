#include "Precompiled.h"
#include "Sprite.h"
#include "Renderer.h"
#include "RdrDrawOp.h"
#include "RdrTransientMem.h"
#include "RdrResource.h"
#include <d3d11.h>

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
	m_hInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_vertexDesc, ARRAYSIZE(s_vertexDesc));

	SpriteVertex* verts = (SpriteVertex*)RdrTransientMem::Alloc(sizeof(SpriteVertex) * 4);
	verts[0].position = Vec2(0.f, 0.f);
	verts[0].uv = aTexcoords[0];
	verts[1].position = Vec2(1.f, 0.f);
	verts[1].uv = aTexcoords[1];
	verts[2].position = Vec2(0.f, -1.f);
	verts[2].uv = aTexcoords[2];
	verts[3].position = Vec2(1.f, -1.f);
	verts[3].uv = aTexcoords[3];

	uint16* indices = (uint16*)RdrTransientMem::Alloc(sizeof(uint16) * 6);
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 1;
	indices[4] = 3;
	indices[5] = 2;

	m_hGeo = RdrGeoSystem::CreateGeo(verts, sizeof(SpriteVertex), 4, indices, 6, Vec3(1.f, 1.f, 0.0f));

	m_material.hPixelShaders[(int)RdrShaderMode::Normal] = RdrShaderSystem::CreatePixelShaderFromFile("p_sprite.hlsl");
	m_material.hTextures[0] = RdrResourceSystem::CreateTextureFromFile(textureName, nullptr);
	m_material.samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
	m_material.texCount = 1;
}

void Sprite::QueueDraw(Renderer& rRenderer, const Vec3& pos, const Vec2& scale, float alpha)
{
	RdrDrawOp* op = RdrDrawOp::Allocate();
	op->eType = RdrDrawOpType::Graphics;
	op->graphics.hGeo = m_hGeo;
	op->graphics.bFreeGeo = false;
	op->graphics.hInputLayout = m_hInputLayout;
	op->graphics.vertexShader = kVertexShader;
	op->graphics.pMaterial = &m_material;

	uint constantsSize = sizeof(Vec4) * 2;
	Vec4* pConstants = (Vec4*)RdrTransientMem::AllocAligned(constantsSize, 16);
	pConstants[0] = Vec4(pos.x - rRenderer.GetViewportWidth() * 0.5f, pos.y + rRenderer.GetViewportHeight() * 0.5f, pos.z + 1.f, 0.f);
	pConstants[1] = Vec4(scale.x, scale.y, alpha, 0.f);
	op->graphics.hVsConstants = RdrResourceSystem::CreateTempConstantBuffer(pConstants, constantsSize);

	rRenderer.AddToBucket(op, RdrBucketType::UI);
}
