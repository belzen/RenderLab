#pragma once

#include "Math.h"
#include "Shaders.h"
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

namespace Font
{
	void Init(RdrContext* pRdrContext);

	RdrGeoHandle CreateTextGeo(Renderer& rRenderer, const char* text);

	void QueueDraw(Renderer& rRenderer, const UI::Position& pos, float size, const char* text, Color color);
	void QueueDraw(Renderer& rRenderer, const UI::Position& pos, float size, const RdrGeoHandle hTextGeo, Color color);
}