#include "Precompiled.h"
#include "Sprite.h"
#include "Renderer.h"
#include "RdrDrawOp.h"
#include "RdrTransientHeap.h"
#include <d3d11.h>

namespace
{
	struct SpriteVertex
	{
		Vec2 position;
		Vec2 uv;
	};

	static RdrVertexInputElement s_vertexDesc[] = {
		{ kRdrShaderSemantic_Position, 0, kRdrVertexInputFormat_RG_F32, 0, 0, kRdrVertexInputClass_PerVertex, 0 },
		{ kRdrShaderSemantic_Texcoord, 0, kRdrVertexInputFormat_RG_F32, 0, 8, kRdrVertexInputClass_PerVertex, 0 }
	};
}

void Sprite::Init(Renderer& rRenderer, const Vec2 aTexcoords[4], const char* textureName)
{
	m_hVertexShader = rRenderer.GetShaderSystem().CreateShaderFromFile(kRdrShaderType_Vertex, "v_sprite.hlsl");
	m_hInputLayout = rRenderer.GetShaderSystem().CreateInputLayout(m_hVertexShader, s_vertexDesc, ARRAYSIZE(s_vertexDesc));
	m_hPixelShader = rRenderer.GetShaderSystem().CreateShaderFromFile(kRdrShaderType_Pixel, "p_sprite.hlsl");
	m_hTexture = rRenderer.GetResourceSystem().CreateTextureFromFile(textureName, nullptr);

	SpriteVertex* verts = (SpriteVertex*)RdrTransientHeap::Alloc(sizeof(SpriteVertex) * 4);
	verts[0].position = Vec2(0.f, 0.f);
	verts[0].uv = aTexcoords[0];
	verts[1].position = Vec2(1.f, 0.f);
	verts[1].uv = aTexcoords[1];
	verts[2].position = Vec2(0.f, -1.f);
	verts[2].uv = aTexcoords[2];
	verts[3].position = Vec2(1.f, -1.f);
	verts[3].uv = aTexcoords[3];

	uint16* indices = (uint16*)RdrTransientHeap::Alloc(sizeof(uint16) * 6);
	indices[0] = 0;
	indices[1] = 1;
	indices[2] = 2;
	indices[3] = 1;
	indices[4] = 3;
	indices[5] = 2;

	m_hGeo = rRenderer.GetGeoSystem().CreateGeo(verts, sizeof(SpriteVertex), 4, indices, 6, Vec3(1.f, 1.f, 0.0f));
}

void Sprite::QueueDraw(Renderer& rRenderer, const Vec3& pos, const Vec2& scale, float alpha)
{
	RdrDrawOp* op = RdrDrawOp::Allocate();
	op->hGeo = m_hGeo;
	op->hInputLayouts[kRdrShaderMode_Normal] = m_hVertexShader;
	op->hVertexShaders[kRdrShaderMode_Normal] = m_hInputLayout;
	op->hPixelShaders[kRdrShaderMode_Normal] = m_hPixelShader;
	op->samplers[0] = RdrSamplerState(kComparisonFunc_Never, kRdrTexCoordMode_Wrap, false);
	op->hTextures[0] = m_hTexture;
	op->texCount = 1;
	op->bFreeGeo = false;
	op->constants[0] = Vec4(pos.x - rRenderer.GetViewportWidth() * 0.5f, pos.y + rRenderer.GetViewportHeight() * 0.5f, pos.z + 1.f, 0.f);
	op->constants[1] = Vec4(scale.x, scale.y, alpha, 0.f);
	op->numConstants = 2;

	rRenderer.AddToBucket(op, kRdrBucketType_UI);
}
