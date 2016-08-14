#pragma once

#include "RdrResource.h"
#include "ModelData.h"
#include "UtilsLib\FileWatcher.h"
#include "Light.h"

namespace AssetLib
{
	struct Sky;
}

struct RdrDrawOp;
struct RdrComputeOp;
struct RdrMaterial;

class Sky
{
public:
	Sky();

	void Cleanup();

	void Reload();

	void Load(const char* skyName);

	void Update(float dt);

	void PrepareDraw();

	const char* GetName() const;

	const RdrDrawOp** GetDrawOps() const;
	uint GetNumDrawOps() const;

	const RdrComputeOp** GetComputeOps() const;
	uint GetNumComputeOps() const;

	Light GetSunLight() const;
	Vec3 GetSunDirection() const;

	RdrConstantBufferHandle GetAtmosphereConstantBuffer() const;

private:
	RdrComputeOp* m_pComputeOps[32];

	ModelData* m_pModelData;
	RdrDrawOp* m_pSubObjectDrawOps[ModelData::kMaxSubObjects];
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

inline const RdrDrawOp** Sky::GetDrawOps() const
{
	return (const RdrDrawOp**)m_pSubObjectDrawOps;
}

inline uint Sky::GetNumDrawOps() const
{
	return ModelData::kMaxSubObjects;
}

inline const RdrComputeOp** Sky::GetComputeOps() const
{
	return (const RdrComputeOp**)m_pComputeOps;
}

inline uint Sky::GetNumComputeOps() const
{
	return m_pendingComputeOps;
}

inline RdrConstantBufferHandle Sky::GetAtmosphereConstantBuffer() const
{
	return m_hAtmosphereConstants;
}