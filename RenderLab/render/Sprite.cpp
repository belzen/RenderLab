#include "Precompiled.h"
#include "Sprite.h"
#include "Renderer.h"
#include <d3d11.h>

namespace
{
	struct SpriteVertex
	{
		Vec2 position;
		Vec2 uv;
	};

	static D3D11_INPUT_ELEMENT_DESC s_vertexDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
}


void Sprite::Init(RdrContext* pRdrContext, const Vec2 aTexcoords[4], const char* textureName)
{
	m_hVertexShader = pRdrContext->LoadVertexShader("v_sprite.hlsl", s_vertexDesc, ARRAYSIZE(s_vertexDesc));
	m_hPixelShader = pRdrContext->LoadPixelShader("p_sprite.hlsl");
	m_hTexture = pRdrContext->LoadTexture(textureName, false);

	SpriteVertex verts[4];
	verts[0].position = Vec2(0.f, 0.f);
	verts[0].uv = aTexcoords[0];
	verts[1].position = Vec2(1.f, 0.f);
	verts[1].uv = aTexcoords[1];
	verts[2].position = Vec2(0.f, -1.f);
	verts[2].uv = aTexcoords[2];
	verts[3].position = Vec2(1.f, -1.f);
	verts[3].uv = aTexcoords[3];

	uint16 indices[6] = { 0, 1, 2, 1, 3, 2 };
	m_hGeo = pRdrContext->CreateGeo(verts, sizeof(SpriteVertex), 4, indices, 6, Vec3(1.f, 1.f, 0.0f));
}

void Sprite::QueueDraw(Renderer& rRenderer, const Vec3& pos, const Vec2& scale, float alpha)
{
	RdrContext* pRdrContext = rRenderer.GetContext();

	RdrDrawOp* op = RdrDrawOp::Allocate();
	op->hGeo = m_hGeo;
	op->hVertexShader = m_hVertexShader;
	op->hPixelShader = m_hPixelShader;
	op->hTextures[0] = m_hTexture;
	op->texCount = 1;
	op->bFreeGeo = false;
	op->constants[0] = Vec4(pos.x - rRenderer.GetViewportWidth() * 0.5f, pos.y + rRenderer.GetViewportHeight() * 0.5f, pos.z + 1.f, 0.f);
	op->constants[1] = Vec4(scale.x, scale.y, alpha, 0.f);
	op->numConstants = 2;

	rRenderer.AddToBucket(op, RBT_UI);
}
