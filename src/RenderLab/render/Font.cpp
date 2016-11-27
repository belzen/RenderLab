#include "Precompiled.h"
#include "Font.h"
#include "Renderer.h"
#include "RdrDrawOp.h"
#include "RdrFrameMem.h"
#include "Camera.h"
#include "UI.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Text, RdrShaderFlags::None };

	static const RdrVertexInputElement s_vertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RG_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 8, RdrVertexInputClass::PerVertex, 0 }
	};

	struct TextVertex
	{
		Vec2 position;
		Vec2 uv;
	};

	struct
	{
		short glyphWidths[256];
		int glyphPixelSize;

		RdrInputLayoutHandle hInputLayout;
		RdrMaterial material;
	} s_text;

	static void loadFontData(const char* filename)
	{
		char* pFileData;
		uint fileSize;
		FileLoader::Load(filename, &pFileData, &fileSize);
		assert(fileSize == sizeof(s_text.glyphWidths));

		memcpy(s_text.glyphWidths, pFileData, fileSize);
		delete pFileData;
	}

	void queueDrawCommon(Renderer& rRenderer, const UI::Position& uiPos, float size, const TextObject& rText, bool bFreeGeo, Color color)
	{
		Vec3 pos = UI::PosToScreenSpace(uiPos, Vec2(rText.size.x, rText.size.y) * size, rRenderer.GetViewportSize());

		RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp();
		pDrawOp->hGeo = rText.hTextGeo;
		pDrawOp->bFreeGeo = bFreeGeo;
		pDrawOp->hInputLayout = s_text.hInputLayout;
		pDrawOp->vertexShader = kVertexShader;
		pDrawOp->pMaterial = &s_text.material;

		uint constantsSize = sizeof(Vec4) * 2;
		Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pConstants[0] = Vec4(color.r, color.g, color.b, color.a);
		pConstants[1] = Vec4(pos.x, pos.y, pos.z + 1.f, size);
		pDrawOp->hVsConstants = rRenderer.GetActionCommandList()->CreateTempConstantBuffer(pConstants, constantsSize);

		rRenderer.AddDrawOpToBucket(pDrawOp, RdrBucketType::UI);
	}
}

void Font::Init()
{
	// todo: implement font asset & binning
	char filename[FILE_MAX_PATH];
	sprintf_s(filename, "%s/textures/fonts/verdana.dat", Paths::GetDataDir());
	loadFontData(filename);

	s_text.hInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_vertexDesc, ARRAY_SIZE(s_vertexDesc));

	s_text.material.hPixelShaders[(int)RdrShaderMode::Normal] = RdrShaderSystem::CreatePixelShaderFromFile("p_text.hlsl", nullptr, 0);
	s_text.material.aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false));

	RdrTextureInfo texInfo;
	s_text.material.ahTextures.assign(0, g_pRenderer->GetPreFrameCommandList().CreateTextureFromFile("fonts/verdana", &texInfo));
	s_text.glyphPixelSize = texInfo.width / 16;
}

TextObject Font::CreateText(const char* text)
{
	int numQuads = 0;
	int textLen = (int)strlen(text);

	TextVertex* verts = (TextVertex*)RdrFrameMem::Alloc(sizeof(TextVertex) * textLen * 4);
	uint16* indices = (uint16*)RdrFrameMem::Alloc(sizeof(uint16) * textLen * 6);

	float x = 0.0f;
	float y = 0.0f;
	float xMax = 0.f;

	const float uw = 1.f / 16.f;
	const float vh = 1.f / 16.f;
	const float w = 1.f;
	const float h = 1.f;

	const float padding = 5.f / s_text.glyphPixelSize;

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

			float curGlyphHalfWidth = (s_text.glyphWidths[text[i]] / (float)s_text.glyphPixelSize) * 0.5f;
			float nextGlyphHalfWidth = curGlyphHalfWidth;
			if ( i < textLen - 1 )
				nextGlyphHalfWidth = (s_text.glyphWidths[text[i+1]] / (float)s_text.glyphPixelSize) * 0.5f;
			x += curGlyphHalfWidth + nextGlyphHalfWidth + padding;
		}
	}

	Vec3 size(max(x + 0.5f, xMax), abs(y), 0.f);
	TextObject obj;
	obj.size.x = size.x;
	obj.size.y = size.y;
	obj.hTextGeo = g_pRenderer->GetPreFrameCommandList().CreateGeo(verts, sizeof(TextVertex), numQuads * 4, indices, numQuads * 6, RdrTopology::TriangleList, Vec3::kZero, size);
	return obj;
}

void Font::QueueDraw(Renderer& rRenderer, const UI::Position& pos, float size, const char* text, Color color)
{
	TextObject textObj = CreateText(text);
	queueDrawCommon(rRenderer, pos, size, textObj, true, color);
}

void Font::QueueDraw(Renderer& rRenderer, const UI::Position& pos, float size, const TextObject& rText, Color color)
{
	queueDrawCommon(rRenderer, pos, size, rText, false, color);
}
