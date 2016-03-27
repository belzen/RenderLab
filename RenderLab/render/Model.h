#pragma once

#include "RdrDrawOp.h"
#include "Math.h"

class Renderer;

class Model
{
public:
	Model(RdrGeoHandle hGeo,
		RdrShaderHandle hVertexShader,
		RdrShaderHandle hPixelShader,
		RdrSamplerState* aSamplers,
		RdrResourceHandle* ahTextures,
		int numTextures );

	~Model();

	void QueueDraw(Renderer& rRenderer, const Matrix44& worldMat) const;

	RdrGeoHandle GetGeoHandle() const { return m_hGeo; }
private:
	RdrShaderHandle m_hVertexShader;
	RdrShaderHandle m_hPixelShader;
	RdrGeoHandle m_hGeo;
	RdrSamplerState m_samplers[MAX_TEXTURES_PER_DRAW];
	RdrResourceHandle m_hTextures[MAX_TEXTURES_PER_DRAW];
	int m_numTextures;
};