#pragma once

#include "IViewModel.h"
#include "components/ModelComponent.h"

class ModelComponentViewModel : public IViewModel
{
public:
	void SetTarget(ModelComponent* pModel);

	const char* GetTypeName();
	const PropertyDef** GetProperties();
	
private:
	static std::string GetModel(void* pSource);
	static bool SetModel(const std::string& modelName, void* pSource);

private:
	ModelComponent* m_pModel;
};