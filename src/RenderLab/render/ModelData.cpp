#include "Precompiled.h"
#include "ModelData.h"
#include "RdrContext.h"
#include "RdrDrawOp.h"
#include "RdrFrameMem.h"
#include "RdrResourceSystem.h"
#include "Renderer.h"
#include "Camera.h"
#include "AssetLib/ModelAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "UtilsLib/Hash.h"

namespace
{
	ModelDataFreeList s_models;

	typedef std::map<Hashing::StringHash, ModelData*> ModelDataMap;
	ModelDataMap s_modelCache;
}

const RdrVertexInputElement* ModelData::GetVertexElements(uint nSubObject) const
{
	return &m_pBinData->inputElements.ptr[m_pBinData->subobjects.ptr[nSubObject].nInputElementStart];
}

uint ModelData::GetNumVertexElements(uint nSubObject) const
{
	return m_pBinData->subobjects.ptr[nSubObject].nInputElementCount;
}

ModelData* ModelData::LoadFromFile(const CachedString& modelName)
{
	// Find model in cache
	ModelDataMap::iterator iter = s_modelCache.find(modelName.getHash());
	if (iter != s_modelCache.end())
		return iter->second;

	AssetLib::Model* pBinData = AssetLibrary<AssetLib::Model>::LoadAsset(modelName);
	if (!pBinData)
	{
		Error("Failed to load model: %s", modelName);
		return nullptr;
	}

	ModelData* pModel = s_models.allocSafe();
	pModel->m_pBinData = pBinData;
	pModel->m_subObjectCount = pBinData->nSubObjectCount;

	Assert(!pModel->m_modelName.getString());
	pModel->m_modelName = modelName;

	pModel->m_hVertexBuffer = RdrResourceSystem::CreateVertexBuffer(pBinData->vertexBuffer.ptr, 1, pBinData->nVertexBufferSize, RdrResourceAccessFlags::None, CREATE_BACKPOINTER(pModel));
	pModel->m_hIndexBuffer = RdrResourceSystem::CreateIndexBuffer(pBinData->indexBuffer.ptr, pBinData->nIndexBufferSize, RdrResourceAccessFlags::None, CREATE_BACKPOINTER(pModel));

	for (uint i = 0; i < pModel->m_subObjectCount; ++i)
	{
		const AssetLib::Model::SubObject& rBinSubobject = pBinData->subobjects.ptr[i];
		pModel->m_subObjects[i].hGeo = RdrResourceSystem::CreateGeo(pModel->m_hVertexBuffer, rBinSubobject.nVertexStride, rBinSubobject.nVertexStartByteOffset, rBinSubobject.nVertexCount,
			pModel->m_hIndexBuffer, rBinSubobject.nIndexStartByteOffset, rBinSubobject.nIndexCount, rBinSubobject.eIndexFormat, RdrTopology::TriangleList, rBinSubobject.vBoundsMin, rBinSubobject.vBoundsMax, CREATE_BACKPOINTER(pModel));

		pModel->m_subObjects[i].pMaterial = RdrMaterial::Create(rBinSubobject.strMaterialName, pModel->GetVertexElements(i), pModel->GetNumVertexElements(i));
	}

	float maxLen = Vec3Length(pBinData->vBoundsMax);
	float minLen = Vec3Length(pBinData->vBoundsMin);
	pModel->m_radius = std::max(minLen, maxLen);

	s_modelCache.insert(std::make_pair(modelName.getHash(), pModel));

	return pModel;
}

void ModelData::Release()
{
	// todo: refcount and free as necessary
	//memset(m_subObjects, 0, sizeof(m_subObjects));
	//s_models.release(this);
}
