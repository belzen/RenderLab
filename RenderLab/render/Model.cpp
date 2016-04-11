#include "Precompiled.h"
#include "Model.h"
#include "RdrContext.h"
#include "RdrTransientHeap.h"
#include "RdrDrawOp.h"
#include "Renderer.h"
#include "Camera.h"

namespace
{
	const RdrVertexShader kVertexShader = { kRdrVertexShader_Model, (RdrShaderFlags)0 };

	static const RdrVertexInputElement s_modelVertexDesc[] = {
		{ kRdrShaderSemantic_Position, 0, kRdrVertexInputFormat_RGB_F32, 0, 0, kRdrVertexInputClass_PerVertex, 0 },
		{ kRdrShaderSemantic_Normal, 0, kRdrVertexInputFormat_RGB_F32, 0, 12, kRdrVertexInputClass_PerVertex, 0 },
		{ kRdrShaderSemantic_Color, 0, kRdrVertexInputFormat_RGBA_F32, 0, 24, kRdrVertexInputClass_PerVertex, 0 },
		{ kRdrShaderSemantic_Texcoord, 0, kRdrVertexInputFormat_RG_F32, 0, 40, kRdrVertexInputClass_PerVertex, 0 },
		{ kRdrShaderSemantic_Tangent, 0, kRdrVertexInputFormat_RGB_F32, 0, 48, kRdrVertexInputClass_PerVertex, 0 },
		{ kRdrShaderSemantic_Binormal, 0, kRdrVertexInputFormat_RGB_F32, 0, 60, kRdrVertexInputClass_PerVertex, 0 }
	};
}

Model::Model(Renderer& rRenderer,
	RdrGeoHandle hGeo,
	RdrShaderHandle hPixelShader,
	RdrSamplerState* aSamplers,
	RdrResourceHandle* ahTextures,
	int numTextures)
	: m_hPixelShader(hPixelShader)
	, m_hGeo(hGeo)
	, m_numTextures(numTextures)
{
	for (int i = 0; i < numTextures; ++i)
	{
		m_samplers[i] = aSamplers[i];
		m_hTextures[i] = ahTextures[i];
	}

	m_hInputLayout = rRenderer.GetShaderSystem().CreateInputLayout(kVertexShader, s_modelVertexDesc, ARRAYSIZE(s_modelVertexDesc));
}


Model::~Model()
{
}

void Model::QueueDraw(Renderer& rRenderer, const Matrix44& srcWorldMat)
{
	RdrDrawOp* pDrawOp = RdrDrawOp::Allocate();
	pDrawOp->eType = kRdrDrawOpType_Graphics;

	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrTransientHeap::AllocAligned(constantsSize, 16);
	*((Matrix44*)pConstants) = Matrix44Transpose(srcWorldMat);
	pDrawOp->graphics.hVsConstants = rRenderer.GetResourceSystem().CreateTempConstantBuffer(pConstants, constantsSize, kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);

	for (int i = 0; i < m_numTextures; ++i)
	{
		pDrawOp->samplers[i] = m_samplers[i];
		pDrawOp->hTextures[i] = m_hTextures[i];
	}
	pDrawOp->texCount = m_numTextures;

	pDrawOp->graphics.hInputLayout = m_hInputLayout;
	pDrawOp->graphics.vertexShader = kVertexShader;
	pDrawOp->graphics.hPixelShaders[kRdrShaderMode_Normal] = m_hPixelShader;

	pDrawOp->graphics.hGeo = m_hGeo;
	pDrawOp->graphics.bNeedsLighting = true;

	rRenderer.AddToBucket(pDrawOp, kRdrBucketType_Opaque);
}
