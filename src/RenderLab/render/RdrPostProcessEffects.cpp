#include "Precompiled.h"
#include "RdrPostProcessEffects.h"
#include "RdrResourceSystem.h"
#include "RdrShaderConstants.h"
#include "RdrFrameMem.h"
#include "Renderer.h"

RdrPostProcessEffects::RdrPostProcessEffects()
	: m_hToneMapInputConstants(0)
{

}

void RdrPostProcessEffects::Init(const AssetLib::PostProcessEffects* pEffects)
{
	m_pEffects = pEffects;
}

void RdrPostProcessEffects::PrepareDraw()
{
	uint constantsSize = sizeof(ToneMapInputParams);
	ToneMapInputParams* pTonemapSettings = (ToneMapInputParams*)RdrFrameMem::AllocAligned(constantsSize, 16);
	pTonemapSettings->white = m_pEffects->eyeAdaptation.white;
	pTonemapSettings->middleGrey = m_pEffects->eyeAdaptation.middleGrey;
	pTonemapSettings->minExposure = pow(2.f, m_pEffects->eyeAdaptation.minExposure);
	pTonemapSettings->maxExposure = pow(2.f, m_pEffects->eyeAdaptation.maxExposure);
	pTonemapSettings->bloomThreshold = m_pEffects->bloom.threshold;
	pTonemapSettings->frameTime = Time::FrameTime();

	m_hToneMapInputConstants = g_pRenderer->GetActionCommandList()->CreateUpdateConstantBuffer(m_hToneMapInputConstants,
		pTonemapSettings, sizeof(ToneMapInputParams), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
}
