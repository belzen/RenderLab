#include "Precompiled.h"
#include "ModelInstanceViewModel.h"
#include "PropertyTables.h"

void ModelInstanceViewModel::SetTarget(ModelInstance* pModel)
{
	m_pModel = pModel;
}

const char* ModelInstanceViewModel::GetTypeName()
{
	return "Model Instance";
}

const PropertyDef** ModelInstanceViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
		new AssetPropertyDef("Model", "", WidgetDragDataType::kModelAsset, GetModel, SetModel),
		nullptr
	};
	return s_ppProperties;
}

std::string ModelInstanceViewModel::GetModel(void* pSource)
{
	ModelInstanceViewModel* pViewModel = (ModelInstanceViewModel*)pSource;
	return pViewModel->m_pModel ? pViewModel->m_pModel->GetModelData()->GetName() : "";
}

bool ModelInstanceViewModel::SetModel(const std::string& modelName, void* pSource)
{
	ModelInstanceViewModel* pViewModel = (ModelInstanceViewModel*)pSource;
	pViewModel->m_pModel->SetModelData(modelName.c_str(), nullptr, 0);
	return true;
}
