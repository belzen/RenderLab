#include "Precompiled.h"
#include "Model.h"
#include "RdrContext.h"
#include "RdrTransientMem.h"
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
}

Model* Model::LoadFromFile(const char* modelName)
{
	char fullFilename[FILE_MAX_PATH];
	ModelAsset::Definition.BuildFilename(AssetLoc::Src, modelName, fullFilename, ARRAY_SIZE(fullFilename));

	Json::Value jRoot;
	if (!FileLoader::LoadJson(fullFilename, jRoot))
	{
		assert(false);
		return nullptr;
	}

	Model* pModel = s_models.allocSafe();

	assert(pModel->m_modelName[0] == 0);
	strcpy_s(pModel->m_modelName, modelName);

	Json::Value jSubObjectArray = jRoot.get("subobjects", Json::nullValue);
	pModel->m_subObjectCount = jSubObjectArray.size();

	Vec3 boundsMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	Vec3 boundsMin = { FLT_MAX, FLT_MAX, FLT_MAX };
	for (int i = 0; i < pModel->m_subObjectCount; ++i)
	{
		SubObject& rSubObject = pModel->m_subObjects[i];

		Json::Value jSubObject = jSubObjectArray.get(i, Json::nullValue);

		Json::Value jModel = jSubObject.get("geo", Json::Value::null);

		RdrGeoInfo geoInfo;
		rSubObject.hGeo = RdrGeoSystem::CreateGeoFromFile(jModel.asCString(), &geoInfo);
		boundsMax = Vec3Max(boundsMax, geoInfo.boundsMax);
		boundsMin = Vec3Min(boundsMin, geoInfo.boundsMin);

		Json::Value jMaterialName = jSubObject.get("material", Json::Value::null);
		rSubObject.pMaterial = RdrMaterial::LoadFromFile(jMaterialName.asCString());
	}

	if (!s_hModelInputLayout)
	{
		s_hModelInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_modelVertexDesc, ARRAY_SIZE(s_modelVertexDesc));
	}

	float maxLen = Vec3Length(boundsMax);
	float minLen = Vec3Length(boundsMin);
	pModel->m_radius = minLen;

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
	Vec4* pConstants = (Vec4*)RdrTransientMem::AllocAligned(constantsSize, 16);
	*((Matrix44*)pConstants) = Matrix44Transpose(srcWorldMat);
	RdrConstantBufferHandle hVsConstants = RdrResourceSystem::CreateTempConstantBuffer(pConstants, constantsSize);

	for (int i = 0; i < m_subObjectCount; ++i)
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
