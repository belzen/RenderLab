#pragma once

#include "Math.h"
#include "RdrShaders.h"
#include "RdrContext.h"

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