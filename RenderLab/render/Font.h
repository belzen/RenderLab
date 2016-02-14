#pragma once

#include "Math.h"
#include "Shaders.h"

struct ID3D11Texture2D;
struct ID3D11Device;
struct ID3D11ShaderResourceView;
struct ID3D11InputLayout;
struct RdrContext;
class Renderer;


namespace Font
{
	void Initialize(RdrContext* pRdrContext);
	void QueueDraw(Renderer* pRenderer, float x, float y, float depth, const char* text, Color color);
};