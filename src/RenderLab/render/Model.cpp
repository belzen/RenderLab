#include "Precompiled.h"
#include "Model.h"
#include "RdrContext.h"
#include "RdrScratchMem.h"
#include "RdrDrawOp.h"
#include "Renderer.h"
#include "Camera.h"
#include "AssetLib/ModelAsset.h"
#include "UtilsLib/Hash.h"

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

	typedef std::map<Hashing::StringHash, Model*> ModelMap;
	ModelMap s_modelCache;
}

Model* Model::LoadFromFile(const char* modelName)
{
	// Find model in cache
	Hashing::StringHash nameHash = Hashing::HashString(modelName);
	ModelMap::iterator iter = s_modelCache.find(nameHash);
	if (iter != s_modelCache.end())
		return iter->second;

	char* pFileData;
	uint fileSize;
	if (!AssetLib::g_modelDef.LoadAsset(modelName, &pFileData, &fileSize))
	{
		Error("Failed to load model: %s", modelName);
		return nullptr;
	}

	AssetLib::Model* pBinData = AssetLib::Model::FromMem(pFileData);

	Model* pModel = s_models.allocSafe();
	pModel->m_pBinData = pBinData;
	pModel->m_subObjectCount = pBinData->subObjectCount;

	assert(pModel->m_modelName[0] == 0);
	strcpy_s(pModel->m_modelName, modelName);

	uint vertAccum = 0;
	uint indicesAccum = 0;
	for (uint i = 0; i < pModel->m_subObjectCount; ++i)
	{
		const AssetLib::Model::SubObject& rBinSubobject = pBinData->subobjects.ptr[i];
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

	s_modelCache.insert(std::make_pair(nameHash, pModel));

	return pModel;
}

void Model::Release()
{
	// todo: refcount and free as necessary
	//memset(m_subObjects, 0, sizeof(m_subObjects));
	//s_models.release(this);
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
		if (rSubObject.pMaterial->bAlphaCutout)
		{
			pDrawOp->graphics.vertexShader.flags |= RdrShaderFlags::AlphaCutout;
		}

		pDrawOp->graphics.hGeo = rSubObject.hGeo;

		rRenderer.AddToBucket(pDrawOp, RdrBucketType::Opaque);
	}
}
