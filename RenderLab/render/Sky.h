#pragma once

#include "RdrGeometry.h"
#include "RdrResource.h"
#include "RdrShaders.h"

class Renderer;

class Sky
{
public:
	Sky();

	void Init(Renderer& rRenderer, 
		RdrGeoHandle hGeo,
		RdrShaderHandle hPixelShader,
		RdrResourceHandle hSkyTexture);

	void QueueDraw(Renderer& rRenderer) const;

private:
	RdrGeoHandle m_hGeo;
	RdrInputLayoutHandle m_hInputLayout;
	RdrShaderHandle m_hPixelShader;
	RdrResourceHandle m_hSkyTexture;
};