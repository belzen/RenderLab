#include "Precompiled.h"
#include "RdrPostProcessEffects.h"
#include "RdrResourceSystem.h"
#include "RdrShaderConstants.h"
#include "RdrFrameMem.h"

RdrPostProcessEffects::RdrPostProcessEffects()
	: m_hToneMapInputConstants(0)
{

}

void RdrPostProcessEffects::Init(const AssetLib::PostProcessEffects* pEffects)
{
	m_pEffects = pEffects;
	if (m_hToneMapInputConstants)
	{
		RdrResourceSystem::ReleaseConstantBuffer(m_hToneMapInputConstants);
		m_hToneMapInputConstants = 0;
	}
}

void RdrPostProcessEffects::PrepareDraw()
{
	if (!m_hToneMapInputConstants || m_pEffects->timeLastModified != m_updateTime)
	{
		uint constantsSize = sizeof(ToneMapInputParams);
		ToneMapInputParams* pTonemapSettings = (ToneMapInputParams*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pTonemapSettings->white = m_pEffects->eyeAdaptation.white;
		pTonemapSettings->middleGrey = m_pEffects->eyeAdaptation.middleGrey;
		pTonemapSettings->bloomThreshold = m_pEffects->bloom.threshold;

		m_hToneMapInputConstants = RdrResourceSystem::CreateUpdateConstantBuffer(m_hToneMapInputConstants, 
			pTonemapSettings, sizeof(ToneMapInputParams), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);

		m_updateTime = m_pEffects->timeLastModified;
	}
}
