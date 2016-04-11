#include "Precompiled.h"
#include "Model.h"
#include "RdrContext.h"
#include "RdrTransientMem.h"
#include "RdrDrawOp.h"
#include "Renderer.h"
#include "Camera.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Model, RdrShaderFlags::None };

	static const RdrVertexInputElement s_modelVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Normal, 0, RdrVertexInputFormat::RGB_F32, 0, 12, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Color, 0, RdrVertexInputFormat::RGBA_F32, 0, 24, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 40, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Tangent, 0, RdrVertexInputFormat::RGB_F32, 0, 48, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Binormal, 0, RdrVertexInputFormat::RGB_F32, 0, 60, RdrVertexInputClass::PerVertex, 0 }
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
	pDrawOp->eType = RdrDrawOpType::Graphics;

	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrTransientMem::AllocAligned(constantsSize, 16);
	*((Matrix44*)pConstants) = Matrix44Transpose(srcWorldMat);
	pDrawOp->graphics.hVsConstants = rRenderer.GetResourceSystem().CreateTempConstantBuffer(pConstants, constantsSize);

	for (int i = 0; i < m_numTextures; ++i)
	{
		pDrawOp->samplers[i] = m_samplers[i];
		pDrawOp->hTextures[i] = m_hTextures[i];
	}
	pDrawOp->texCount = m_numTextures;

	pDrawOp->graphics.hInputLayout = m_hInputLayout;
	pDrawOp->graphics.vertexShader = kVertexShader;
	pDrawOp->graphics.hPixelShaders[(int)RdrShaderMode::Normal] = m_hPixelShader;

	pDrawOp->graphics.hGeo = m_hGeo;
	pDrawOp->graphics.bNeedsLighting = true;

	rRenderer.AddToBucket(pDrawOp, RdrBucketType::Opaque);
}
