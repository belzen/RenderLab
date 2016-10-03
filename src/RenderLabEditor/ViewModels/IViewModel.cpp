#include "Precompiled.h"
#include "IViewModel.h"
#include "PostProcessEffectsViewModel.h"
#include "SkyViewModel.h"
#include "WorldObjectViewModel.h"

IViewModel* IViewModel::Create(uint64 typeId, void* pTypeData)
{
	if (typeId == typeid(AssetLib::PostProcessEffects).hash_code())
	{
		PostProcessEffectsViewModel* pViewModel = new PostProcessEffectsViewModel();
		pViewModel->SetTarget(static_cast<AssetLib::PostProcessEffects*>(pTypeData));
		return pViewModel;
	}
	else if (typeId == typeid(AssetLib::Sky).hash_code())
	{
		SkyViewModel* pViewModel = new SkyViewModel();
		pViewModel->SetTarget(static_cast<AssetLib::Sky*>(pTypeData));
		return pViewModel;
	}
	else if (typeId == typeid(WorldObject).hash_code())
	{
		WorldObjectViewModel* pViewModel = new WorldObjectViewModel();
		pViewModel->SetTarget(static_cast<WorldObject*>(pTypeData));
		return pViewModel;
	}

	return nullptr;
}