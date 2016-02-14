#pragma once

#include "RdrContext.h"
#include "Math.h"

struct RdrContext;
class Renderer;

class Model
{
public:
	Model(RdrGeoHandle hGeo,
		ShaderHandle hVertexShader,
		ShaderHandle hPixelShader,
		RdrTextureHandle* ahTextures,
		int numTextures );

	~Model();

	void QueueDraw(Renderer* pRenderer, const Matrix44& worldMat) const;

	RdrGeoHandle GetGeoHandle() const { return m_hGeo; }
private:
	ShaderHandle m_hVertexShader;
	ShaderHandle m_hPixelShader;
	RdrGeoHandle m_hGeo;
	RdrTextureHandle m_hTextures[MAX_TEXTURES_PER_DRAW];
	int m_numTextures;
};