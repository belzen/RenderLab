#include "Precompiled.h"
#include "Sky.h"
#include "RdrDrawOp.h"
#include "RdrTransientMem.h"
#include "Renderer.h"
#include "AssetLib/SkyAsset.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Sky, RdrShaderFlags::None };

	static const RdrVertexInputElement s_skyVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 }
	};

	RdrInputLayoutHandle s_hSkyInputLayout = 0;


	void handleSkyFileChanged(const char* filename, void* pUserData)
	{
		char skyName[AssetDef::kMaxNameLen];
		SkyAsset::Definition.ExtractAssetName(filename, skyName, ARRAY_SIZE(skyName));

		Sky* pSky = (Sky*)pUserData;
		if (_stricmp(pSky->GetName(), skyName) == 0)
		{
			pSky->Reload();
		}
	}

}

Sky::Sky()
	: m_hGeo(0)
	, m_pMaterial(nullptr)
	, m_reloadListenerId(0)
	, m_reloadPending(false)
{
	m_skyName[0] = 0;
}

void Sky::Cleanup()
{
	m_hGeo = 0;
	m_pMaterial = nullptr;
	m_skyName[0] = 0;
	FileWatcher::RemoveListener(m_reloadListenerId);
	m_reloadListenerId = 0;
	m_reloadPending = false;
}

void Sky::Reload()
{
	m_reloadPending = true;
}

const char* Sky::GetName() const
{
	return m_skyName;
}

void Sky::Load(const char* skyName)
{
	assert(m_skyName[0] == 0);
	strcpy_s(m_skyName, skyName);

	char fullFilename[FILE_MAX_PATH];
	SkyAsset::Definition.BuildFilename(AssetLoc::Src, skyName, fullFilename, ARRAY_SIZE(fullFilename));

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
		s_hSkyInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_skyVertexDesc, ARRAY_SIZE(s_skyVertexDesc));
	}

	// Listen for changes of the sky file.
	char filePattern[AssetDef::kMaxNameLen];
	SkyAsset::Definition.GetFilePattern(AssetLoc::Src, filePattern, ARRAY_SIZE(filePattern));
	m_reloadListenerId = FileWatcher::AddListener(filePattern, handleSkyFileChanged, this);
}

void Sky::Update(float dt)
{
	if (m_reloadPending)
	{
		char skyName[AssetDef::kMaxNameLen];
		strcpy_s(skyName, m_skyName);

		Cleanup();
		Load(skyName);
		m_reloadPending = false;
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