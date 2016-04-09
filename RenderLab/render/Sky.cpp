#include "Precompiled.h"
#include "Sky.h"
#include "RdrDrawOp.h"
#include "RdrTransientHeap.h"
#include "Renderer.h"

namespace
{
	const RdrVertexShader kVertexShader = { kRdrVertexShader_Sky, (RdrShaderFlags)0 };

	static RdrVertexInputElement s_skyVertexDesc[] = {
		{ kRdrShaderSemantic_Position, 0, kRdrVertexInputFormat_RGB_F32, 0, 0, kRdrVertexInputClass_PerVertex, 0 }
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
	pDrawOp->eType = kRdrDrawOpType_Graphics;

	Vec3 pos = rRenderer.GetCurrentCamera().GetPosition();
	Matrix44 mtxWorld = Matrix44Translation(pos);

	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrTransientHeap::AllocAligned(constantsSize, 16);
	*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);
	pDrawOp->graphics.hVsConstants = rRenderer.GetResourceSystem().CreateTempConstantBuffer(pConstants, constantsSize, kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);

	pDrawOp->samplers[0] = RdrSamplerState(kComparisonFunc_Never, kRdrTexCoordMode_Wrap, false);
	pDrawOp->hTextures[0] = m_hSkyTexture;
	pDrawOp->texCount = 1;

	pDrawOp->graphics.hInputLayout = m_hInputLayout;
	pDrawOp->graphics.vertexShader = kVertexShader;
	pDrawOp->graphics.hPixelShaders[kRdrShaderMode_Normal] = m_hPixelShader;

	pDrawOp->graphics.hGeo = m_hGeo;
	pDrawOp->graphics.bNeedsLighting = false;

	rRenderer.AddToBucket(pDrawOp, kRdrBucketType_Sky);
}