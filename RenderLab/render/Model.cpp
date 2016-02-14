#include "Precompiled.h"
#include "Model.h"
#include "RdrContext.h"
#include "Renderer.h"
#include "Camera.h"


Model::Model(RdrGeoHandle hGeo,
	ShaderHandle hVertexShader,
	ShaderHandle hPixelShader,
	RdrTextureHandle* ahTextures,
	int numTextures)
	: m_hVertexShader(hVertexShader)
	, m_hPixelShader(hPixelShader)
	, m_hGeo(hGeo)
	, m_numTextures(numTextures)
{
	for (int i = 0; i < numTextures; ++i)
	{
		m_hTextures[i] = ahTextures[i];
	}
}


Model::~Model()
{
}

void Model::QueueDraw(Renderer* pRenderer, const Matrix44& srcWorldMat) const
{
	RdrDrawOp* pDrawOp = RdrDrawOp::Allocate();

	Matrix44 worldMat = Matrix44Transpose(srcWorldMat);
	for (int i = 0; i < 4; ++i)
	{
		pDrawOp->constants[i * 4 + 0] = worldMat.m[i][0];
		pDrawOp->constants[i * 4 + 1] = worldMat.m[i][1];
		pDrawOp->constants[i * 4 + 2] = worldMat.m[i][2];
		pDrawOp->constants[i * 4 + 3] = worldMat.m[i][3];
	}
	pDrawOp->constantsByteSize = sizeof(float) * 16;

	for (int i = 0; i < m_numTextures; ++i)
	{
		pDrawOp->hTextures[i] = m_hTextures[i];
	}
	pDrawOp->texCount = m_numTextures;

	pDrawOp->hVertexShader = m_hVertexShader;
	pDrawOp->hPixelShader = m_hPixelShader;
	pDrawOp->hGeo = m_hGeo;
	pDrawOp->needsLighting = true;

	pRenderer->AddToBucket(pDrawOp, RBT_OPAQUE);
}
