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

	void LoadFromFile(const char* skyName);

	void QueueDraw(Renderer& rRenderer) const;

private:
	RdrGeoHandle m_hGeo;
	const RdrMaterial* m_pMaterial;
};