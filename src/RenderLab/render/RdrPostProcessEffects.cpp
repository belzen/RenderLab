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
	m_effects = *pEffects;
}

void RdrPostProcessEffects::PrepareForDraw(Renderer& rRenderer)
{
	if (!m_hToneMapInputConstants)
	{
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(ToneMapInputParams));
		ToneMapInputParams* pTonemapSettings = (ToneMapInputParams*)RdrScratchMem::AllocAligned(constantsSize, 16);
		pTonemapSettings->white = m_effects.eyeAdaptation.white;
		pTonemapSettings->middleGrey = m_effects.eyeAdaptation.middleGrey;
		pTonemapSettings->bloomThreshold = m_effects.bloom.threshold;

		m_hToneMapInputConstants = RdrResourceSystem::CreateConstantBuffer(pTonemapSettings, sizeof(ToneMapInputParams), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}
}
