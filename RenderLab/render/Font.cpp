#include "Precompiled.h"
#include "Font.h"
#include "Renderer.h"
#include "RdrDrawOp.h"
#include "RdrTransientHeap.h"
#include "Camera.h"
#include "UI.h"
#include <d3d11.h>
#include <xmllite.h>
#include <shlwapi.h>

#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "shlwapi.lib")

namespace
{
	const RdrVertexShader kVertexShader = { kRdrVertexShader_Text, 0 };

	static RdrVertexInputElement s_vertexDesc[] = {
		{ kRdrShaderSemantic_Position, 0, kRdrVertexInputFormat_RG_F32, 0, 0, kRdrVertexInputClass_PerVertex, 0 },
		{ kRdrShaderSemantic_Texcoord, 0, kRdrVertexInputFormat_RG_F32, 0, 8, kRdrVertexInputClass_PerVertex, 0 }
	};

	struct TextVertex
	{
		Vec2 position;
		Vec2 uv;
	};

	struct
	{
		short glpyhWidths[256];
		int glyphPixelSize;

		RdrResourceHandle hTexture;

		RdrInputLayoutHandle hInputLayout;
		RdrShaderHandle hPixelShader;
	} g_text;

	static void loadFontData(const char* filename)
	{
		FILE* file;
		fopen_s(&file, filename, "rb");

		fread(g_text.glpyhWidths, sizeof(short), 256, file);

		fclose(file);
	}

	void queueDrawCommon(Renderer& rRenderer, const UI::Position& uiPos, float size, const TextObject& rText, bool bFreeGeo, Color color)
	{
		Vec3 pos = UI::PosToScreenSpace(uiPos, Vec2(rText.size.x, rText.size.y) * size, rRenderer.GetViewportSize());

		RdrDrawOp* op = RdrDrawOp::Allocate();
		op->eType = kRdrDrawOpType_Graphics;
		op->graphics.hGeo = rText.hTextGeo;
		op->graphics.bFreeGeo = bFreeGeo;
		op->graphics.hInputLayout = g_text.hInputLayout;
		op->graphics.vertexShader = kVertexShader;
		op->graphics.hPixelShaders[kRdrShaderMode_Normal] = g_text.hPixelShader;
		op->samplers[0] = RdrSamplerState(kComparisonFunc_Never, kRdrTexCoordMode_Wrap, false);
		op->hTextures[0] = g_text.hTexture;
		op->texCount = 1;

		uint constantsSize = sizeof(Vec4) * 2;
		Vec4* pConstants = (Vec4*)RdrTransientHeap::AllocAligned(constantsSize, 16);
		pConstants[0] = Vec4(color.r, color.g, color.b, color.a);
		pConstants[1] = Vec4(pos.x, pos.y, pos.z + 1.f, size);
		op->graphics.hVsConstants = rRenderer.GetResourceSystem().CreateTempConstantBuffer(pConstants, constantsSize, kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);

		rRenderer.AddToBucket(op, kRdrBucketType_UI);
	}
}

void Font::Init(Renderer& rRenderer)
{
	const char* filename = "fonts/verdana.dds";
	loadFontData("data/textures/fonts/verdana.dat");

	RdrTextureInfo texInfo;
	g_text.hTexture = rRenderer.GetResourceSystem().CreateTextureFromFile(filename, &texInfo);
	g_text.glyphPixelSize = texInfo.width / 16;

	g_text.hInputLayout = rRenderer.GetShaderSystem().CreateInputLayout(kVertexShader, s_vertexDesc, ARRAYSIZE(s_vertexDesc));
	g_text.hPixelShader = rRenderer.GetShaderSystem().CreatePixelShaderFromFile("p_text.hlsl");
}

TextObject Font::CreateText(Renderer& rRenderer, const char* text)
{
	int numQuads = 0;
	int textLen = (int)strlen(text);

	TextVertex* verts = (TextVertex*)RdrTransientHeap::Alloc(sizeof(TextVertex) * textLen * 4);
	uint16* indices = (uint16*)RdrTransientHeap::Alloc(sizeof(uint16) * textLen * 6);

	float x = 0.0f;
	float y = 0.0f;
	float xMax = 0.f;

	const float uw = 1.f / 16.f;
	const float vh = 1.f / 16.f;
	const float w = 1.f;
	const float h = 1.f;

	const float padding = 5.f / g_text.glyphPixelSize;

	for (int i = 0; i < textLen; ++i)
	{
		if (text[i] == '\n')
		{
			xMax = max(x, xMax);
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

			// Bottom left
			verts[numQuads * 4 + 1].position = Vec2(x, y - h);
			verts[numQuads * 4 + 1].uv = Vec2(u, v + vh);

			// Top right
			verts[numQuads * 4 + 2].position = Vec2(x + w, y);
			verts[numQuads * 4 + 2].uv = Vec2(u + uw, v);

			// Bottom right
			verts[numQuads * 4 + 3].position = Vec2(x + w, y - h);
			verts[numQuads * 4 + 3].uv = Vec2(u + uw, v + vh);

			indices[numQuads * 6 + 0] = numQuads * 4 + 0;
			indices[numQuads * 6 + 1] = numQuads * 4 + 2;
			indices[numQuads * 6 + 2] = numQuads * 4 + 1;
			indices[numQuads * 6 + 3] = numQuads * 4 + 2;
			indices[numQuads * 6 + 4] = numQuads * 4 + 3;
			indices[numQuads * 6 + 5] = numQuads * 4 + 1;

			++numQuads;

			float curGlyphHalfWidth = (g_text.glpyhWidths[text[i]] / (float)g_text.glyphPixelSize) * 0.5f;
			float nextGlyphHalfWidth = curGlyphHalfWidth;
			if ( i < textLen - 1 )
				nextGlyphHalfWidth = (g_text.glpyhWidths[text[i+1]] / (float)g_text.glyphPixelSize) * 0.5f;
			x += curGlyphHalfWidth + nextGlyphHalfWidth + padding;
		}
	}

	Vec3 size(max(x + 0.5f, xMax), abs(y), 0.f);
	TextObject obj;
	obj.size.x = size.x;
	obj.size.y = size.y;
	obj.hTextGeo = rRenderer.GetGeoSystem().CreateGeo(verts, sizeof(TextVertex), numQuads * 4, indices, numQuads * 6, size);
	return obj;
}

void Font::QueueDraw(Renderer& rRenderer, const UI::Position& pos, float size, const char* text, Color color)
{
	TextObject textObj = CreateText(rRenderer, text);
	queueDrawCommon(rRenderer, pos, size, textObj, true, color);
}

void Font::QueueDraw(Renderer& rRenderer, const UI::Position& pos, float size, const TextObject& rText, Color color)
{
	queueDrawCommon(rRenderer, pos, size, rText, false, color);
}
