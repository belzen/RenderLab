#include "Precompiled.h"
#include "Sky.h"
#include "RdrDrawOp.h"
#include "RdrTransientMem.h"
#include "Renderer.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Sky, (RdrShaderFlags)0 };

	static const RdrVertexInputElement s_skyVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 }
	};
}

Sky::Sky()
{

}

void Sky::Init(Renderer& rRenderer,
	RdrGeoHandle hGeo,
	RdrShaderHandle hPixelShader,
	RdrResourceHandle hSkyTexture)
{
	m_hGeo = hGeo;
	m_hPixelShader = hPixelShader;
	m_hSkyTexture = hSkyTexture;

	m_hInputLayout = rRenderer.GetShaderSystem().CreateInputLayout(kVertexShader, s_skyVertexDesc, ARRAYSIZE(s_skyVertexDesc));
}

void Sky::QueueDraw(Renderer& rRenderer) const
{
	RdrDrawOp* pDrawOp = RdrDrawOp::Allocate();
	pDrawOp->eType = RdrDrawOpType::Graphics;

	Vec3 pos = rRenderer.GetCurrentCamera().GetPosition();
	Matrix44 mtxWorld = Matrix44Translation(pos);

	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrTransientMem::AllocAligned(constantsSize, 16);
	*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);
	pDrawOp->graphics.hVsConstants = rRenderer.GetResourceSystem().CreateTempConstantBuffer(pConstants, constantsSize);

	pDrawOp->samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
	pDrawOp->hTextures[0] = m_hSkyTexture;
	pDrawOp->texCount = 1;

	pDrawOp->graphics.hInputLayout = m_hInputLayout;
	pDrawOp->graphics.vertexShader = kVertexShader;
	pDrawOp->graphics.hPixelShaders[(int)RdrShaderMode::Normal] = m_hPixelShader;

	pDrawOp->graphics.hGeo = m_hGeo;
	pDrawOp->graphics.bNeedsLighting = false;

	rRenderer.AddToBucket(pDrawOp, RdrBucketType::Sky);
}