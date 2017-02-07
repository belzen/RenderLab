#pragma once

#include "IViewModel.h"
#include "components/Decal.h"

class DecalViewModel : public IViewModel
{
public:
	void SetTarget(Decal* pModel);

	const char* GetTypeName();
	const PropertyDef** GetProperties();

private:
	static std::string GetTexture(void* pSource);
	static bool SetTexture(const std::string& textureName, void* pSource);

private:
	Decal* m_pDecal;
};
