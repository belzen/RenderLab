#include "Precompiled.h"
#include "Sky.h"
#include "RdrDrawOp.h"
#include "RdrTransientMem.h"
#include "Renderer.h"
#include "FileLoader.h"
#include "json/json.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Sky, RdrShaderFlags::None };

	static const RdrVertexInputElement s_skyVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 }
	};

	RdrInputLayoutHandle s_hSkyInputLayout = 0;
}

Sky::Sky()
	: m_hGeo(0)
	, m_pMaterial(nullptr)
{

}

void Sky::LoadFromFile(const char* skyName)
{
	char fullFilename[MAX_PATH];
	sprintf_s(fullFilename, "data/skies/%s.sky", skyName);

	Json::Value jRoot;
	if (!FileLoader::LoadJson(fullFilename, jRoot))
	{
		assert(false);
		return;
	}

	Json::Value jModel = jRoot.get("geo", Json::Value::null);
	m_hGeo = RdrGeoSystem::CreateGeoFromFile(jModel.asCString(), nullptr);

	Json::Value jMaterialName = jRoot.get("material", Json::Value::null);
	m_pMaterial = RdrMaterial::LoadFromFile(jMaterialName.asCString());

	if (!s_hSkyInputLayout)
	{
		s_hSkyInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_skyVertexDesc, ARRAYSIZE(s_skyVertexDesc));
	}
}

void Sky::QueueDraw(Renderer& rRenderer) const
{
	RdrDrawOp* pDrawOp = RdrDrawOp::Allocate();
	pDrawOp->eType = RdrDrawOpType::Graphics;

	Vec3 pos = rRenderer.GetCurrentCamera().GetPosition();
	Matrix44 mtxWorld = Matrix44Translation(pos);

	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrTransientMem::AllocAligned(constantsSize, 16);
	*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);
	pDrawOp->graphics.hVsConstants = RdrResourceSystem::CreateTempConstantBuffer(pConstants, constantsSize);

	pDrawOp->graphics.pMaterial = m_pMaterial;

	pDrawOp->graphics.hInputLayout = s_hSkyInputLayout;
	pDrawOp->graphics.vertexShader = kVertexShader;

	pDrawOp->graphics.hGeo = m_hGeo;

	rRenderer.AddToBucket(pDrawOp, RdrBucketType::Sky);
}