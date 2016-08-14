#include "Precompiled.h"
#include "RdrPostProcessEffects.h"
#include "RdrResourceSystem.h"
#include "RdrShaderConstants.h"
#include "RdrScratchMem.h"

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
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(ToneMapInputParams));
		ToneMapInputParams* pTonemapSettings = (ToneMapInputParams*)RdrScratchMem::AllocAligned(constantsSize, 16);
		pTonemapSettings->white = m_pEffects->eyeAdaptation.white;
		pTonemapSettings->middleGrey = m_pEffects->eyeAdaptation.middleGrey;
		pTonemapSettings->bloomThreshold = m_pEffects->bloom.threshold;

		if (m_hToneMapInputConstants)
		{
			RdrResourceSystem::UpdateConstantBuffer(m_hToneMapInputConstants, pTonemapSettings);
		}
		else
		{
			m_hToneMapInputConstants = RdrResourceSystem::CreateConstantBuffer(pTonemapSettings, sizeof(ToneMapInputParams), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
		}

		m_updateTime = m_pEffects->timeLastModified;
	}
}
