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

	//donotcheckin - Get layout desc from ModelData instead of hardcoding here
	static const RdrVertexInputElement s_modelVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Normal, 0, RdrVertexInputFormat::RGB_F32, 0, 12, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Color, 0, RdrVertexInputFormat::RGBA_F32, 0, 24, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 40, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Tangent, 0, RdrVertexInputFormat::RGB_F32, 0, 48, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Binormal, 0, RdrVertexInputFormat::RGB_F32, 0, 60, RdrVertexInputClass::PerVertex, 0 }
	};
}

const RdrVertexInputElement* ModelData::GetVertexElements() const
{
	return s_modelVertexDesc;
}

uint ModelData::GetNumVertexElements() const
{
	return ARRAYSIZE(s_modelVertexDesc);
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
	pModel->m_subObjectCount = pBinData->subObjectCount;

	assert(!pModel->m_modelName.getString());
	pModel->m_modelName = modelName;

	uint vertAccum = 0;
	uint indicesAccum = 0;
	for (uint i = 0; i < pModel->m_subObjectCount; ++i)
	{
		const AssetLib::Model::SubObject& rBinSubobject = pBinData->subobjects.ptr[i];
		Vertex* pVerts = (Vertex*)RdrFrameMem::Alloc(sizeof(Vertex) * rBinSubobject.vertCount);
		uint16* pIndices = (uint16*)RdrFrameMem::Alloc(sizeof(uint16) * rBinSubobject.indexCount);

		for (uint k = 0; k < rBinSubobject.indexCount; ++k)
		{
			pIndices[k] = pBinData->indices.ptr[indicesAccum + k] - vertAccum;
		}

		for (uint k = 0; k < rBinSubobject.vertCount; ++k)
		{
			Vertex& rVert = pVerts[k];
			rVert.position = pBinData->positions.ptr[vertAccum];
			rVert.texcoord = pBinData->texcoords.ptr[vertAccum];
			rVert.normal = pBinData->normals.ptr[vertAccum];
			rVert.color = pBinData->colors.ptr[vertAccum];
			rVert.tangent = pBinData->tangents.ptr[vertAccum];
			rVert.bitangent = pBinData->bitangents.ptr[vertAccum];

			++vertAccum;
		}

		pModel->m_subObjects[i].hGeo = g_pRenderer->GetPreFrameCommandList().CreateGeo(pVerts, sizeof(Vertex), rBinSubobject.vertCount,
			pIndices, rBinSubobject.indexCount, RdrTopology::TriangleList, rBinSubobject.boundsMin, rBinSubobject.boundsMax);

		pModel->m_subObjects[i].pMaterial = RdrMaterial::Create(rBinSubobject.materialName, s_modelVertexDesc, ARRAYSIZE(s_modelVertexDesc));

		indicesAccum += rBinSubobject.indexCount;
	}

	float maxLen = Vec3Length(pBinData->boundsMax);
	float minLen = Vec3Length(pBinData->boundsMin);
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
