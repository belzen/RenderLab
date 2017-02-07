#include "Precompiled.h"
#include "DecalViewModel.h"
#include "PropertyTables.h"

void DecalViewModel::SetTarget(Decal* pDecal)
{
	m_pDecal = pDecal;
}

const char* DecalViewModel::GetTypeName()
{
	return "Decal";
}

const PropertyDef** DecalViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
		new AssetPropertyDef("Texture", "", WidgetDragDataType::kModelAsset, GetTexture, SetTexture),
		nullptr
	};
	return s_ppProperties;
}

std::string DecalViewModel::GetTexture(void* pSource)
{
	DecalViewModel* pViewModel = (DecalViewModel*)pSource;
	return pViewModel->m_pDecal ? pViewModel->m_pDecal->GetTexture().getString() : "";
}

bool DecalViewModel::SetTexture(const std::string& textureName, void* pSource)
{
	DecalViewModel* pViewModel = (DecalViewModel*)pSource;
	pViewModel->m_pDecal->SetTexture(textureName.c_str());
	return true;
}
