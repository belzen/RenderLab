#pragma once

#include "Math.h"
#include "RdrShaders.h"
#include "RdrContext.h"

struct ID3D11Texture2D;
struct ID3D11Device;
struct ID3D11ShaderResourceView;
struct ID3D11InputLayout;
class Renderer;

namespace UI
{
	struct Position;
}

struct TextObject
{
	RdrGeoHandle hTextGeo;
	Vec2 size;
};

namespace Font
{
	void Init();

	TextObject CreateText(const char* text);

	void QueueDraw(RdrAction* pAction, const UI::Position& pos, float size, const char* text, Color color);
	void QueueDraw(RdrAction* pAction, const UI::Position& pos, float size, const TextObject &rText, Color color);
}