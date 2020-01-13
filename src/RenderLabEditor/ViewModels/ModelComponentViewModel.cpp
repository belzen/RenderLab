#include "Precompiled.h"
#include "ModelComponentViewModel.h"
#include "PropertyTables.h"

void ModelComponentViewModel::SetTarget(ModelComponent* pModel)
{
	m_pModel = pModel;
}

const char* ModelComponentViewModel::GetTypeName()
{
	return "Model Instance";
}

const PropertyDef** ModelComponentViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
		new AssetPropertyDef("Model", "", WidgetDragDataType::kModelAsset, GetModel, SetModel),
		new TextPropertyDef("Materials", "", GetMaterials, nullptr),
		nullptr
	};
	return s_ppProperties;
}

std::string ModelComponentViewModel::GetModel(void* pSource)
{
	ModelComponentViewModel* pViewModel = (ModelComponentViewModel*)pSource;
	return pViewModel->m_pModel ? pViewModel->m_pModel->GetModelData()->GetName() : "";
}

bool ModelComponentViewModel::SetModel(const std::string& modelName, void* pSource)
{
	ModelComponentViewModel* pViewModel = (ModelComponentViewModel*)pSource;
	pViewModel->m_pModel->SetModelData(modelName.c_str(), nullptr, 0);
	return true;
}

std::string ModelComponentViewModel::GetMaterials(void* pSource)
{
	ModelComponentViewModel* pViewModel = (ModelComponentViewModel*)pSource;
	std::string strMaterials = "";

	if (pViewModel->m_pModel)
	{
		const ModelData* pModelData = pViewModel->m_pModel->GetModelData();
		uint numSubobjects = pModelData->GetNumSubObjects();
		for (uint i = 0; i < numSubobjects; ++i)
		{
			strMaterials += pModelData->GetSubObject(i).pMaterial->GetName().getString();
			strMaterials += "\n";
		}
	}

	return strMaterials;
}
