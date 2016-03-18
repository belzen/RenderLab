#include "Precompiled.h"
#include "Model.h"
#include "RdrContext.h"
#include "Renderer.h"
#include "Camera.h"


Model::Model(RdrGeoHandle hGeo,
	ShaderHandle hVertexShader,
	ShaderHandle hPixelShader,
	RdrSamplerState* aSamplers,
	RdrResourceHandle* ahTextures,
	int numTextures)
	: m_hVertexShader(hVertexShader)
	, m_hPixelShader(hPixelShader)
	, m_hGeo(hGeo)
	, m_numTextures(numTextures)
{
	for (int i = 0; i < numTextures; ++i)
	{
		m_samplers[i] = aSamplers[i];
		m_hTextures[i] = ahTextures[i];
	}
}


Model::~Model()
{
}

void Model::QueueDraw(Renderer& rRenderer, const Matrix44& srcWorldMat) const
{
	RdrDrawOp* pDrawOp = RdrDrawOp::Allocate();

	pDrawOp->position = srcWorldMat.GetTranslation();

	Matrix44 worldMat = Matrix44Transpose(srcWorldMat);
	for (int i = 0; i < 4; ++i)
	{
		pDrawOp->constants[i].x = worldMat.m[i][0];
		pDrawOp->constants[i].y = worldMat.m[i][1];
		pDrawOp->constants[i].z = worldMat.m[i][2];
		pDrawOp->constants[i].w = worldMat.m[i][3];
	}
	pDrawOp->numConstants = 4;

	for (int i = 0; i < m_numTextures; ++i)
	{
		pDrawOp->samplers[i] = m_samplers[i];
		pDrawOp->hTextures[i] = m_hTextures[i];
	}
	pDrawOp->texCount = m_numTextures;

	pDrawOp->hVertexShader = m_hVertexShader;
	pDrawOp->hPixelShader = m_hPixelShader;
	pDrawOp->hGeo = m_hGeo;
	pDrawOp->needsLighting = true;

	rRenderer.AddToBucket(pDrawOp, kRdrBucketType_Opaque);
}
