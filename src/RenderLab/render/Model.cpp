#include "Precompiled.h"
#include "Model.h"
#include "RdrContext.h"
#include "RdrScratchMem.h"
#include "RdrDrawOp.h"
#include "Renderer.h"
#include "Camera.h"
#include "AssetLib/ModelAsset.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Model, RdrShaderFlags::None };

	static const RdrVertexInputElement s_modelVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Normal, 0, RdrVertexInputFormat::RGB_F32, 0, 12, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Color, 0, RdrVertexInputFormat::RGBA_F32, 0, 24, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 40, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Tangent, 0, RdrVertexInputFormat::RGB_F32, 0, 48, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Binormal, 0, RdrVertexInputFormat::RGB_F32, 0, 60, RdrVertexInputClass::PerVertex, 0 }
	};

	RdrInputLayoutHandle s_hModelInputLayout = 0;
	ModelFreeList s_models;

	typedef std::map<std::string, Model*> ModelMap;
	ModelMap s_modelCache;
}

Model* Model::LoadFromFile(const char* modelName)
{
	// Find model in cache
	ModelMap::iterator iter = s_modelCache.find(modelName);
	if (iter != s_modelCache.end())
		return iter->second;

	char fullFilename[FILE_MAX_PATH];
	ModelAsset::Definition.BuildFilename(AssetLoc::Bin, modelName, fullFilename, ARRAY_SIZE(fullFilename));

	char* pFileData;
	uint fileSize;
	if (!FileLoader::Load(fullFilename, &pFileData, &fileSize))
	{
		Error("Failed to load model: %s", fullFilename);
		return nullptr;
	}

	ModelAsset::BinData* pBinData = ModelAsset::BinData::FromMem(pFileData);

	Model* pModel = s_models.allocSafe();
	pModel->m_pBinData = pBinData;
	pModel->m_subObjectCount = pBinData->subObjectCount;

	assert(pModel->m_modelName[0] == 0);
	strcpy_s(pModel->m_modelName, modelName);

	uint vertAccum = 0;
	uint indicesAccum = 0;
	for (uint i = 0; i < pModel->m_subObjectCount; ++i)
	{
		const ModelAsset::BinData::SubObject& rBinSubobject = pBinData->subobjects.ptr[i];
		Vertex* pVerts = (Vertex*)RdrScratchMem::Alloc(sizeof(Vertex) * rBinSubobject.vertCount);
		uint16* pIndices = (uint16*)RdrScratchMem::Alloc(sizeof(uint16) * rBinSubobject.indexCount);

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

		pModel->m_subObjects[i].hGeo = RdrGeoSystem::CreateGeo(pVerts, sizeof(Vertex), rBinSubobject.vertCount, pIndices, rBinSubobject.indexCount, rBinSubobject.boundsMin, rBinSubobject.boundsMax);
		pModel->m_subObjects[i].pMaterial = RdrMaterial::LoadFromFile(rBinSubobject.materialName);

		indicesAccum += rBinSubobject.indexCount;
	}

	if (!s_hModelInputLayout)
	{
		s_hModelInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_modelVertexDesc, ARRAY_SIZE(s_modelVertexDesc));
	}

	float maxLen = Vec3Length(pBinData->boundsMax);
	float minLen = Vec3Length(pBinData->boundsMin);
	pModel->m_radius = minLen;

	s_modelCache.insert(std::make_pair(modelName, pModel));

	return pModel;
}

void Model::Release()
{
	memset(m_subObjects, 0, sizeof(m_subObjects));
	s_models.release(this);
}

void Model::QueueDraw(Renderer& rRenderer, const Matrix44& srcWorldMat)
{
	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrScratchMem::AllocAligned(constantsSize, 16);
	*((Matrix44*)pConstants) = Matrix44Transpose(srcWorldMat);
	RdrConstantBufferHandle hVsConstants = RdrResourceSystem::CreateTempConstantBuffer(pConstants, constantsSize);

	for (uint i = 0; i < m_subObjectCount; ++i)
	{
		SubObject& rSubObject = m_subObjects[i];

		RdrDrawOp* pDrawOp = RdrDrawOp::Allocate();
		pDrawOp->eType = RdrDrawOpType::Graphics;

		pDrawOp->graphics.hVsConstants = hVsConstants;

		pDrawOp->graphics.pMaterial = rSubObject.pMaterial;

		pDrawOp->graphics.hInputLayout = s_hModelInputLayout;
		pDrawOp->graphics.vertexShader = kVertexShader;

		pDrawOp->graphics.hGeo = rSubObject.hGeo;

		rRenderer.AddToBucket(pDrawOp, RdrBucketType::Opaque);
	}
}
