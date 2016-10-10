#pragma once

#include "AssetLib\AssetLibForwardDecl.h"
#include "RdrResource.h"
#include "ModelData.h"
#include "UtilsLib\FileWatcher.h"
#include "Light.h"

struct RdrDrawOp;
struct RdrComputeOp;
struct RdrMaterial;
class RdrDrawBuckets;

class Sky
{
public:
	Sky();

	void Cleanup();

	void Reload();

	void Load(const char* skyName);

	void Update(float dt);

	void QueueDraw(RdrDrawBuckets* pDrawBuckets);

	const char* GetName() const;

	Light GetSunLight() const;
	Vec3 GetSunDirection() const;

	float GetPssmLambda() const;

	RdrConstantBufferHandle GetAtmosphereConstantBuffer() const;
	RdrResourceHandle GetTransmittanceLut() const;

private:
	ModelData* m_pModelData;
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;

	const RdrMaterial* m_pMaterial;
	const AssetLib::Sky* m_pSrcAsset;

	FileWatcher::ListenerID m_reloadListenerId;
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

