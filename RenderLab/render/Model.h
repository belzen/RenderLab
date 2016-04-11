#pragma once

#include "RdrDrawOp.h"

class Renderer;

class Model
{
public:
	Model(Renderer& rRenderer, 
		RdrGeoHandle hGeo,
		RdrShaderHandle hPixelShader,
		RdrSamplerState* aSamplers,
		RdrResourceHandle* ahTextures,
		int numTextures );

	~Model();

	void QueueDraw(Renderer& rRenderer, const Matrix44& worldMat);

	RdrGeoHandle GetGeoHandle() const;

private:
	RdrInputLayoutHandle m_hInputLayout;
	RdrShaderHandle m_hPixelShader;
	RdrGeoHandle m_hGeo;
	RdrSamplerState m_samplers[MAX_TEXTURES_PER_DRAW];
	RdrResourceHandle m_hTextures[MAX_TEXTURES_PER_DRAW];
	int m_numTextures;
};

inline RdrGeoHandle Model::GetGeoHandle() const
{ 
	return m_hGeo; 
}
