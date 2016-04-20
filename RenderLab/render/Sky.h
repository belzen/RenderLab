#pragma once

#include "RdrGeometry.h"
#include "RdrResource.h"
#include "RdrShaders.h"

class Renderer;
struct RdrMaterial;

class Sky
{
public:
	Sky();

	void Cleanup();

	void Reload();

	void Load(const char* skyName);

	void Update(float dt);

	void QueueDraw(Renderer& rRenderer) const;

	const char* GetName() const;

private:

	RdrGeoHandle m_hGeo;
	const RdrMaterial* m_pMaterial;
	char m_skyName[AssetDef::kMaxNameLen];

	FileWatcher::ListenerID m_reloadListenerId;
	bool m_reloadPending;
};