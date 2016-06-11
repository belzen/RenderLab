#pragma once

#include "RdrResource.h"
#include "ModelData.h"

struct RdrDrawOp;
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

private:
	ModelData* m_pModelData;
	RdrDrawOp* m_pSubObjectDrawOps[ModelData::kMaxSubObjects];
	RdrConstantBufferHandle m_hVsPerObjectConstantBuffer;

	const RdrMaterial* m_pMaterial;
	char m_skyName[AssetLib::AssetDef::kMaxNameLen];

	FileWatcher::ListenerID m_reloadListenerId;
	bool m_reloadPending;
};

inline const RdrDrawOp** Sky::GetDrawOps() const
{
	return (const RdrDrawOp**)m_pSubObjectDrawOps;
}

inline uint Sky::GetNumDrawOps() const
{
	return ModelData::kMaxSubObjects;
}