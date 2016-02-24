#pragma once

#include "Math.h"
#include "Shaders.h"
#include "RdrContext.h"

struct ID3D11Texture2D;
struct ID3D11Device;
struct ID3D11ShaderResourceView;
struct ID3D11InputLayout;
class Renderer;

namespace Font
{
	void Init(RdrContext* pRdrContext);

	RdrGeoHandle CreateTextGeo(Renderer* pRenderer, const char* text, Color color);

	void QueueDraw(Renderer* pRenderer, const Vec3& pos, float size, const char* text, Color color);
	void QueueDraw(Renderer* pRenderer, const Vec3& pos, float size, const RdrGeoHandle hTextGeo);
}