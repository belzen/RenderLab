#pragma once

#include "RdrContext.h"

class Renderer;

class Sprite
{
public:
	// Texcoords: TL TR BL BR
	void Init(RdrContext* pRdrContext, const Vec2 aTexcoords[4], const char* texture);

	void QueueDraw(Renderer& rRenderer, const Vec3& pos, const Vec2& scale, float alpha);

private:
	ShaderHandle m_hVertexShader;
	ShaderHandle m_hPixelShader;
	RdrGeoHandle m_hGeo;
	RdrTextureHandle m_hTexture;
};
