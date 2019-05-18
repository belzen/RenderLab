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

	void queueDrawCommon(RdrAction* pAction, const UI::Position& uiPos, float size, const TextObject& rText, Color color)
	{
		const Rect& viewport = pAction->GetViewport();
		Vec3 pos = UI::PosToScreenSpace(uiPos, Vec2(rText.size.x, rText.size.y) * size, Vec2(viewport.width, viewport.height));

		RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp(CREATE_NULL_BACKPOINTER);
		pDrawOp->hGeo = rText.hTextGeo;
		pDrawOp->pMaterial = &s_text.material;

		uint constantsSize = sizeof(Vec4) * 2;
		Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pConstants[0] = Vec4(color.r, color.g, color.b, color.a);
		pConstants[1] = Vec4(pos.x, pos.y, pos.z + 1.f, size);
		pDrawOp->hVsConstants = RdrResourceSystem::CreateTempConstantBuffer(pConstants, constantsSize, CREATE_NULL_BACKPOINTER);

		pAction->AddDrawOp(pDrawOp, RdrBucketType::UI);
	}
}

void Font::Init()
{
	// todo: implement font asset & binning
	char filename[FILE_MAX_PATH];
	sprintf_s(filename, "%s/textures/fonts/verdana.dat", Paths::GetDataDir());
	loadFontData(filename);

	const RdrResourceFormat* pRtvFormats;
	uint nNumRtvFormats;
	Renderer::GetStageRenderTargetFormats(RdrRenderStage::kUI, &pRtvFormats, &nNumRtvFormats);

	const RdrShader* pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_text.hlsl", nullptr, 0);

	RdrRasterState rasterState;
	rasterState.bWireframe = false;
	rasterState.bDoubleSided = false;
	rasterState.bEnableMSAA = false;
	rasterState.bUseSlopeScaledDepthBias = false;
	rasterState.bEnableScissor = false;

	s_text.material.Init("Font", RdrMaterialFlags::None);
	s_text.material.CreatePipelineState(RdrShaderMode::Normal,
		kVertexShader, pPixelShader,
		s_vertexDesc, ARRAY_SIZE(s_vertexDesc),
		pRtvFormats, nNumRtvFormats,
		RdrBlendMode::kAlpha,
		rasterState,
		RdrDepthStencilState(false, false, RdrComparisonFunc::Never));

	RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
	s_text.material.SetSamplers(0, 1, &sampler);

	RdrTextureInfo texInfo;
	RdrResourceHandle hTexture = RdrResourceSystem::CreateTextureFromFile("fonts/verdana", &texInfo, CREATE_NULL_BACKPOINTER);
	s_text.material.SetTextures(0, 1, &hTexture);
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
	obj.hTextGeo = RdrResourceSystem::CreateGeo(verts, sizeof(TextVertex), numQuads * 4, indices, numQuads * 6, RdrTopology::TriangleList, Vec3::kZero, size, CREATE_NULL_BACKPOINTER);
	return obj;
}

void Font::QueueDraw(RdrAction* pAction, const UI::Position& pos, float size, const char* text, Color color)
{
	TextObject textObj = CreateText(text);
	queueDrawCommon(pAction, pos, size, textObj, color);
	g_pRenderer->GetResourceCommandList().ReleaseGeo(textObj.hTextGeo, CREATE_NULL_BACKPOINTER);
}

void Font::QueueDraw(RdrAction* pAction, const UI::Position& pos, float size, const TextObject& rText, Color color)
{
	queueDrawCommon(pAction, pos, size, rText, color);
}
