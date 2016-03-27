#pragma once

#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "RdrDeviceTypes.h"

class Renderer;
class RdrContext;

class Sprite
{
public:
	// Texcoords: TL TR BL BR
	void Init(RdrContext* pRdrContext, const Vec2 aTexcoords[4], const char* texture);

	void QueueDraw(Renderer& rRenderer, const Vec3& pos, const Vec2& scale, float alpha);

private:
	RdrShaderHandle m_hVertexShader;
	RdrShaderHandle m_hPixelShader;
	RdrGeoHandle m_hGeo;
	RdrResourceHandle m_hTexture;
};
