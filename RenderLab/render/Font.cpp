#include "Precompiled.h"
#include "Font.h"
#include "Renderer.h"
#include "Camera.h"
#include <d3d11.h>
#include <xmllite.h>
#include <shlwapi.h>

#pragma comment(lib, "xmllite.lib")
#pragma comment(lib, "shlwapi.lib")

static D3D11_INPUT_ELEMENT_DESC s_vertexDesc[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

struct FontChar
{
	Rect rect;
	Vec2 offset;
	float width;
};

struct TextVertex
{
	Vec2 position;
	Color color;
	Vec2 uv;
};

struct
{
	FontChar characks[255];
	int texSize;
	int fontHeight;

	RdrTextureHandle hTexture;

	ShaderHandle hVertexShader;
	ShaderHandle hPixelShader;
	ID3D11InputLayout* pInputLayout;
} g_text;


static void loadFontXml(const char* filename)
{
	HRESULT hr;
	IStream *pFileStream = NULL;
	IXmlReader *pReader = NULL;
	XmlNodeType nodeType;
	const WCHAR* pwszLocalName;
	const WCHAR* pwszValue;

	//Open read-only input stream
	hr = SHCreateStreamOnFileA(filename, STGM_READ, &pFileStream);
	assert(hr == S_OK);

	hr = CreateXmlReader(__uuidof(IXmlReader), (void**)&pReader, NULL);
	assert(hr == S_OK);

	hr = pReader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
	assert(hr == S_OK);

	hr = pReader->SetInput(pFileStream);
	assert(hr == S_OK);

	//read until there are no more nodes
	while (S_OK == (hr = pReader->Read(&nodeType)))
	{
		switch (nodeType)
		{
		case XmlNodeType_Element:
			pReader->GetLocalName(&pwszLocalName, NULL);
		 
			if (wcscmp(pwszLocalName, L"Font") == 0)
			{
				pReader->MoveToAttributeByName(L"height", NULL);
				pReader->GetValue(&pwszValue, NULL);
				g_text.fontHeight = StrToIntW(pwszValue);
			}
			else if (wcscmp(pwszLocalName, L"Char") == 0)
			{
				FontChar c = { 0 };
				wchar_t text[255];
				wchar_t* tok;
				wchar_t *context = NULL;

				pReader->MoveToAttributeByName(L"width", NULL);
				pReader->GetValue(&pwszValue, NULL);
				c.width = (float)StrToIntW(pwszValue);

				pReader->MoveToAttributeByName(L"offset", nullptr);
				pReader->GetValue(&pwszValue, NULL);
				wcscpy_s(text, 255, pwszValue);

				tok = wcstok_s(text, L" ", &context);
				c.offset[0] = (float)StrToIntW(tok);
				tok = wcstok_s(NULL, L" ", &context);
				c.offset[1] = (float)StrToIntW(tok);

				pReader->MoveToAttributeByName(L"rect", nullptr);
				pReader->GetValue(&pwszValue, NULL);
				wcscpy_s(text, 255, pwszValue);

				tok = wcstok_s(text, L" ", &context);
				c.rect.left = (float)StrToIntW(tok);
				tok = wcstok_s(NULL, L" ", &context);
				c.rect.top = (float)StrToIntW(tok);
				tok = wcstok_s(NULL, L" ", &context);
				c.rect.width = (float)StrToIntW(tok);
				tok = wcstok_s(NULL, L" ", &context);
				c.rect.height = (float)StrToIntW(tok);

				pReader->MoveToAttributeByName(L"code", nullptr);
				pReader->GetValue(&pwszValue, NULL);

				g_text.characks[pwszValue[0]] = c;
			}
			break;
		}
	}

	pReader->Release();
	pFileStream->Release();
}



void Font::Initialize(RdrContext* pRdrContext)
{
	const char* filename = "consolas_regular_32.dds";
	loadFontXml("data/textures/consolas_regular_32.xml");

	g_text.texSize = 512;
	g_text.hTexture = pRdrContext->LoadTexture(filename, false);

	g_text.hVertexShader = pRdrContext->LoadVertexShader("v_text.hlsl", s_vertexDesc, ARRAYSIZE(s_vertexDesc));
	g_text.hPixelShader = pRdrContext->LoadPixelShader("p_text.hlsl");
}

void Font::QueueDraw(Renderer* pRenderer, float drawx, float drawy, float depth, const char* text, Color color)
{
	static const int kMaxLen = 1024;
	static TextVertex verts[kMaxLen * 4];
	static uint16 indices[kMaxLen * 6];

	int numQuads = 0;
	int textLen = strlen(text);
	assert(textLen < kMaxLen);

	float x = 0.0f;
	float y = 0.0f;
	float scale = 0.5f;

	for (int i = 0; i < textLen; ++i)
	{
		if (text[i] == '\n')
		{
			y += g_text.fontHeight;
			x = 0.f;
		}
		else
		{
			FontChar* charack = &g_text.characks[text[i]];

			float u = charack->rect.left / (float)g_text.texSize;
			float v = charack->rect.top / (float)g_text.texSize;
			float uw = charack->rect.width / (float)g_text.texSize;
			float vh = charack->rect.height / (float)g_text.texSize;

			float px = x + charack->offset[0] * scale;
			float py = y - charack->offset[1] * scale;
			float pw = charack->rect.width * scale;
			float ph = charack->rect.height * scale;

			// Top left
			verts[numQuads * 4 + 0].position = Vec2(px, py);
			verts[numQuads * 4 + 0].uv = Vec2(u, v);
			verts[numQuads * 4 + 0].color = color;

			// Bottom left
			verts[numQuads * 4 + 1].position = Vec2(px, py - ph);
			verts[numQuads * 4 + 1].uv = Vec2(u, v + vh);
			verts[numQuads * 4 + 1].color = color;

			// Top right
			verts[numQuads * 4 + 2].position = Vec2(px + pw, py);
			verts[numQuads * 4 + 2].uv = Vec2(u + uw, v);
			verts[numQuads * 4 + 2].color = color;

			// Bottom right
			verts[numQuads * 4 + 3].position = Vec2(px + pw, py - ph);
			verts[numQuads * 4 + 3].uv = Vec2(u + uw, v + vh);
			verts[numQuads * 4 + 3].color = color;

			indices[numQuads * 6 + 0] = numQuads * 4 + 0;
			indices[numQuads * 6 + 1] = numQuads * 4 + 2;
			indices[numQuads * 6 + 2] = numQuads * 4 + 1;
			indices[numQuads * 6 + 3] = numQuads * 4 + 2;
			indices[numQuads * 6 + 4] = numQuads * 4 + 3;
			indices[numQuads * 6 + 5] = numQuads * 4 + 1;

			++numQuads;
			x += charack->width * scale;
		}

	}

	RdrContext* pRdrContext = pRenderer->GetContext();

	RdrDrawOp* op = RdrDrawOp::Allocate();
	op->hGeo = pRdrContext->CreateGeo(verts, sizeof(TextVertex), numQuads * 4, indices, numQuads * 6);
	op->hVertexShader = g_text.hVertexShader;
	op->hPixelShader = g_text.hPixelShader;
	op->hTextures[0] = g_text.hTexture;
	op->texCount = 1;
	op->bFreeGeo = true;
	op->constants[0] = drawx - pRenderer->GetViewportWidth() * 0.5f;
	op->constants[1] = drawy + pRenderer->GetViewportHeight() * 0.5f;
	op->constants[2] = depth;
	op->constantsByteSize = sizeof(float) * 4;

	pRenderer->AddToBucket(op, RBT_UI);
}