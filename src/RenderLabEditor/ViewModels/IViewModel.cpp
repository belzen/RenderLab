#include "Precompiled.h"
#include "IViewModel.h"
#include "PostProcessVolumeViewModel.h"
#include "SkyVolumeViewModel.h"
#include "LightViewModel.h"
#include "ModelComponentViewModel.h"
#include "EntityViewModel.h"

IViewModel* IViewModel::Create(const std::type_info& typeId, void* pTypeData)
{
	if (typeId == typeid(PostProcessVolume))
	{
		PostProcessVolumeViewModel* pViewModel = new PostProcessVolumeViewModel();
		pViewModel->SetTarget(static_cast<PostProcessVolume*>(pTypeData));
		return pViewModel;
	}
	else if (typeId == typeid(SkyVolume))
	{
		SkyVolumeViewModel* pViewModel = new SkyVolumeViewModel();
		pViewModel->SetTarget(static_cast<SkyVolume*>(pTypeData));
		return pViewModel;
	}
	else if (typeId == typeid(Light))
	{
		LightViewModel* pViewModel = new LightViewModel();
		pViewModel->SetTarget(static_cast<Light*>(pTypeData));
		return pViewModel;
	}
	else if (typeId == typeid(ModelComponent))
	{
		ModelComponentViewModel* pViewModel = new ModelComponentViewModel();
		pViewModel->SetTarget(static_cast<ModelComponent*>(pTypeData));
		return pViewModel;
	}
	else if (typeId == typeid(Entity))
	{
		EntityViewModel* pViewModel = new EntityViewModel();
		pViewModel->SetTarget(static_cast<Entity*>(pTypeData));
		return pViewModel;
	}

	return nullptr;
}