#include "Precompiled.h"
#include "Font.h"
#include "Renderer.h"
#include "Camera.h"
#include <d3d11.h>
#include <xmllite.h>
#include <shlwapi.h>

#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "shlwapi.lib")

namespace
{
	static D3D11_INPUT_ELEMENT_DESC s_vertexDesc[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	struct TextVertex
	{
		Vec2 position;
		Color color;
		Vec2 uv;
	};

	struct
	{
		short glpyhWidths[256];
		int glyphPixelSize;

		RdrTextureHandle hTexture;

		ShaderHandle hVertexShader;
		ShaderHandle hPixelShader;
		ID3D11InputLayout* pInputLayout;
	} g_text;

	static void loadFontData(const char* filename)
	{
		FILE* file;
		fopen_s(&file, filename, "rb");

		fread(g_text.glpyhWidths, sizeof(short), 256, file);

		fclose(file);
	}

	void queueDrawCommon(Renderer* pRenderer, const Vec3& pos, float size, RdrGeoHandle hTextGeo, bool bFreeGeo)
	{
		RdrContext* pRdrContext = pRenderer->GetContext();

		RdrDrawOp* op = RdrDrawOp::Allocate();
		op->hGeo = hTextGeo;
		op->hVertexShader = g_text.hVertexShader;
		op->hPixelShader = g_text.hPixelShader;
		op->hTextures[0] = g_text.hTexture;
		op->texCount = 1;
		op->bFreeGeo = bFreeGeo;
		op->constants[0] = Vec4(pos.x - pRenderer->GetViewportWidth() * 0.5f,
								pos.y + pRenderer->GetViewportHeight() * 0.5f,
								pos.z + 1.f, 
								size);
		op->numConstants = 1;

		pRenderer->AddToBucket(op, RBT_UI);
	}
}

void Font::Init(RdrContext* pRdrContext)
{
	const char* filename = "fonts/verdana.dds";
	loadFontData("data/textures/fonts/verdana.dat");

	g_text.hTexture = pRdrContext->LoadTexture(filename, false);
	g_text.glyphPixelSize = pRdrContext->m_textures.get(g_text.hTexture)->width / 16;

	g_text.hVertexShader = pRdrContext->LoadVertexShader("v_text.hlsl", s_vertexDesc, ARRAYSIZE(s_vertexDesc));
	g_text.hPixelShader = pRdrContext->LoadPixelShader("p_text.hlsl");
}

RdrGeoHandle Font::CreateTextGeo(Renderer* pRenderer, const char* text, Color color)
{
	static const int kMaxLen = 1024;
	static TextVertex verts[kMaxLen * 4];
	static uint16 indices[kMaxLen * 6];

	int numQuads = 0;
	int textLen = strlen(text);
	assert(textLen < kMaxLen);

	float x = 0.0f;
	float y = 0.0f;

	const float uw = 1.f / 16.f;
	const float vh = 1.f / 16.f;
	const float w = 1.f;
	const float h = 1.f;

	const float padding = 5.f / g_text.glyphPixelSize;

	for (int i = 0; i < textLen; ++i)
	{
		if (text[i] == '\n')
		{
			y -= 1.f;
			x = 0.f;
		}
		else
		{
			int row = text[i] / 16;
			int col = text[i] % 16;

			float u = col / 16.f;
			float v = row / 16.f;

			// Top left
			verts[numQuads * 4 + 0].position = Vec2(x, y);
			verts[numQuads * 4 + 0].uv = Vec2(u, v);
			verts[numQuads * 4 + 0].color = color;

			// Bottom left
			verts[numQuads * 4 + 1].position = Vec2(x, y - h);
			verts[numQuads * 4 + 1].uv = Vec2(u, v + vh);
			verts[numQuads * 4 + 1].color = color;

			// Top right
			verts[numQuads * 4 + 2].position = Vec2(x + w, y);
			verts[numQuads * 4 + 2].uv = Vec2(u + uw, v);
			verts[numQuads * 4 + 2].color = color;

			// Bottom right
			verts[numQuads * 4 + 3].position = Vec2(x + w, y - h);
			verts[numQuads * 4 + 3].uv = Vec2(u + uw, v + vh);
			verts[numQuads * 4 + 3].color = color;

			indices[numQuads * 6 + 0] = numQuads * 4 + 0;
			indices[numQuads * 6 + 1] = numQuads * 4 + 2;
			indices[numQuads * 6 + 2] = numQuads * 4 + 1;
			indices[numQuads * 6 + 3] = numQuads * 4 + 2;
			indices[numQuads * 6 + 4] = numQuads * 4 + 3;
			indices[numQuads * 6 + 5] = numQuads * 4 + 1;

			++numQuads;

			float curGlyphHalfWidth = (g_text.glpyhWidths[text[i]] / (float)g_text.glyphPixelSize) * 0.5f;
			float nextGlyphHalfWidth = 0.f;
			if ( i < textLen - 1 )
				nextGlyphHalfWidth = (g_text.glpyhWidths[text[i+1]] / (float)g_text.glyphPixelSize) * 0.5f;
			x += curGlyphHalfWidth + nextGlyphHalfWidth + padding;
		}

	}

	return pRenderer->GetContext()->CreateGeo(verts, sizeof(TextVertex), numQuads * 4, indices, numQuads * 6);
}

void Font::QueueDraw(Renderer* pRenderer, const Vec3& pos, float size, const char* text, Color color)
{
	RdrGeoHandle hGeo = CreateTextGeo(pRenderer, text, color);
	queueDrawCommon(pRenderer, pos, size, hGeo, true);
}

void Font::QueueDraw(Renderer* pRenderer, const Vec3& pos, float size, const RdrGeoHandle hTextGeo)
{
	queueDrawCommon(pRenderer, pos, size, hTextGeo, false);
}
