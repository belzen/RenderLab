#pragma once

#include "IViewModel.h"
#include "components/ModelInstance.h"

class ModelInstanceViewModel : public IViewModel
{
public:
	void SetTarget(ModelInstance* pModel);

	const char* GetTypeName();
	const PropertyDef** GetProperties();
	
private:
	static std::string GetModel(void* pSource);
	static bool SetModel(const std::string& modelName, void* pSource);

private:
	ModelInstance* m_pModel;
};