#pragma once

#include "AssetLib\AssetLibForwardDecl.h"
#include "AssetLib\AssetLibrary.h"
#include "RdrResource.h"
#include "RdrMaterial.h"
#include "ModelData.h"
#include "UtilsLib\FileWatcher.h"

struct RdrDrawOp;
struct RdrComputeOp;
struct RdrVolumetricFogData;
class RdrDrawBuckets;

class Sky : public IAssetReloadListener<AssetLib::Sky>
{
public:
	Sky();

	void Cleanup();

	void Load(const CachedString& skyName);

	void Update();

	void QueueDraw(RdrDrawBuckets* pDrawBuckets, RdrResourceHandle hVolumetricFogLut);

	Vec3 GetSunDirection() const;
	Vec3 GetSunColor() const;

	float GetPssmLambda() const;

	const AssetLib::VolumetricFogSettings& GetVolFogSettings() const;

	RdrConstantBufferHandle GetAtmosphereConstantBuffer() const;
	RdrResourceHandle GetTransmittanceLut() const;

	const AssetLib::Sky* GetSourceAsset() const;

private:
	void OnAssetReloaded(const AssetLib::Sky* pSkyAsset);

private:
	ModelData* m_pModelData;
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;

	RdrMaterial m_material;
	const AssetLib::Sky* m_pSrcAsset;

	bool m_reloadPending;
	
	RdrConstantBufferHandle m_hAtmosphereConstants;
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

inline RdrConstantBufferHandle Sky::GetAtmosphereConstantBuffer() const
{
	return m_hAtmosphereConstants;
}

inline RdrResourceHandle Sky::GetTransmittanceLut() const
{
	return m_hTransmittanceLut;
}
