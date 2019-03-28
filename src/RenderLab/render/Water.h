#pragma once

#include "AssetLib/SceneAsset.h"
#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "RdrMaterial.h"
#include "RdrTessellationMaterial.h"
#include "RdrDrawOp.h"
#include "RdrComputeOp.h"

struct WaveParticle;

class Water
{
public:
	void Init(int gridSize);

	void Update();

	RdrDrawOpSet BuildDrawOps(RdrAction* pAction);

	RdrComputeOp BuildComputeOps(RdrAction* pAction);

private:
	void InitGridResources();
	void InitParticleResources();

	WaveParticle* m_aParticles;
	int m_numParticles;
	int m_maxParticles;

	RdrMaterial m_gridMaterial;

	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	RdrInputLayoutHandle m_hGridInputLayout;
	RdrGeoHandle m_hGridGeo;

	RdrResourceHandle m_hParticleBuffer;

	int m_gridSize;

	float m_tileSize;
	float m_waveHeightScalar;
	float m_timer;

	bool m_initialized;
};