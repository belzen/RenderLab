#pragma once

#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "RdrMaterial.h"

struct RdrDrawOp;

class Terrain
{
public:
	Terrain() {}

	void Init();
	void PrepareDraw();

	const RdrDrawOp* const* GetDrawOps() const;
	uint GetNumDrawOps() const;

private:
	RdrMaterial m_material;
	RdrDrawOp* m_pDrawOp;
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;
	RdrInputLayoutHandle m_hInputLayout;
	RdrGeoHandle m_hGeo;
	RdrResourceHandle m_hInstanceData;
};

inline const RdrDrawOp* const* Terrain::GetDrawOps() const
{
	return &m_pDrawOp;
}

inline uint Terrain::GetNumDrawOps() const
{
	return 1;
}
