#pragma once

#include "render\RdrResource.h"
#include "render\RdrMaterial.h"
#include "AssetLib\AssetLibForwardDecl.h"

class RdrAction;
struct RdrLightList;
class ModelData;

class RdrSky
{
public:
	RdrSky();

	void AssignExternalResources(RdrResourceHandle hVolumetricFogLut);
	void QueueDraw(RdrAction* pAction, const AssetLib::SkySettings& rSkySettings, const RdrLightList* pLightList, RdrConstantBufferHandle* phOutAtmosphereCb);

	RdrResourceHandle GetTransmittanceLut() const;

private:
	void LazyInit();

private:
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;

	RdrMaterial m_material;
	ModelData* m_pSkyDomeModel;

	RdrResourceHandle m_hTransmittanceLut;
	RdrResourceHandle m_hScatteringRayleighDeltaLut;
	RdrResourceHandle m_hScatteringMieDeltaLut;
	RdrResourceHandle m_hScatteringCombinedDeltaLut;
	RdrResourceHandle m_hScatteringSumLuts[2];
	RdrResourceHandle m_hIrradianceDeltaLut;
	RdrResourceHandle m_hIrradianceSumLuts[2];
	RdrResourceHandle m_hRadianceDeltaLut;
	bool m_bNeedsTransmittanceLutUpdate;

	uint m_pendingComputeOps;
};

inline RdrResourceHandle RdrSky::GetTransmittanceLut() const
{
	return m_hTransmittanceLut;
}
