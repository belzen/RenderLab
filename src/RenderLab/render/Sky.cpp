#include "Precompiled.h"
#include "Sky.h"
#include "ModelData.h"
#include "RdrDrawOp.h"
#include "RdrScratchMem.h"
#include "Renderer.h"
#include "AssetLib/SkyAsset.h"

namespace
{
	const RdrVertexShader kVertexShader = { RdrVertexShaderType::Sky, RdrShaderFlags::None };

	static const RdrVertexInputElement s_skyVertexDesc[] = {
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RGB_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		// todo: Remove all these.  Models need to be more flexible.
		{ RdrShaderSemantic::Normal, 0, RdrVertexInputFormat::RGB_F32, 0, 12, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Color, 0, RdrVertexInputFormat::RGBA_F32, 0, 24, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 40, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Tangent, 0, RdrVertexInputFormat::RGB_F32, 0, 48, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Binormal, 0, RdrVertexInputFormat::RGB_F32, 0, 60, RdrVertexInputClass::PerVertex, 0 }
	};

	RdrInputLayoutHandle s_hSkyInputLayout = 0;


	void handleSkyFileChanged(const char* filename, void* pUserData)
	{
		char skyName[AssetLib::AssetDef::kMaxNameLen];
		AssetLib::g_skyDef.ExtractAssetName(filename, skyName, ARRAY_SIZE(skyName));

		Sky* pSky = (Sky*)pUserData;
		if (_stricmp(pSky->GetName(), skyName) == 0)
		{
			pSky->Reload();
		}
	}

}

Sky::Sky()
	: m_pModelData(nullptr)
	, m_hVsPerObjectConstantBuffer(0)
	, m_pMaterial(nullptr)
	, m_reloadListenerId(0)
	, m_reloadPending(false)
{
	m_skyName[0] = 0;
}

void Sky::Cleanup()
{
	m_pModelData = nullptr;
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

	char* pFileData;
	uint fileSize;
	if (!AssetLib::g_skyDef.LoadAsset(skyName, &pFileData, &fileSize))
	{
		assert(false);
		return;
	}

	AssetLib::Sky* pSky = AssetLib::Sky::FromMem(pFileData);
	m_pModelData = ModelData::LoadFromFile(pSky->model);
	m_pMaterial = RdrMaterial::LoadFromFile(pSky->material);

	if (!s_hSkyInputLayout)
	{
		s_hSkyInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_skyVertexDesc, ARRAY_SIZE(s_skyVertexDesc));
	}

	// Listen for changes of the sky file.
	char filePattern[AssetLib::AssetDef::kMaxNameLen];
	AssetLib::g_skyDef.GetFilePattern(filePattern, ARRAY_SIZE(filePattern));
	m_reloadListenerId = FileWatcher::AddListener(filePattern, handleSkyFileChanged, this);

	delete pFileData;
}

void Sky::Update(float dt)
{
	if (m_reloadPending)
	{
		char skyName[AssetLib::AssetDef::kMaxNameLen];
		strcpy_s(skyName, m_skyName);

		Cleanup();
		Load(skyName);
		m_reloadPending = false;
	}
}

void Sky::PrepareDraw()
{
	if (!m_hVsPerObjectConstantBuffer)
	{
		Matrix44 mtxWorld = Matrix44Translation(Vec3::kZero);
		uint constantsSize = sizeof(Vec4) * 4;
		Vec4* pConstants = (Vec4*)RdrScratchMem::AllocAligned(constantsSize, 16);
		*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);
		m_hVsPerObjectConstantBuffer = RdrResourceSystem::CreateConstantBuffer(pConstants, constantsSize, RdrCpuAccessFlags::None, RdrResourceUsage::Default);
	}


	uint numSubObjects = m_pModelData->GetNumSubObjects();
	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = m_pModelData->GetSubObject(i);

		if (!m_pSubObjectDrawOps[i])
		{
			RdrDrawOp* pDrawOp = m_pSubObjectDrawOps[i] = RdrDrawOp::Allocate();
			pDrawOp->eType = RdrDrawOpType::Graphics;

			pDrawOp->graphics.hVsConstants = m_hVsPerObjectConstantBuffer;

			pDrawOp->graphics.pMaterial = m_pMaterial;

			pDrawOp->graphics.hInputLayout = s_hSkyInputLayout;
			pDrawOp->graphics.vertexShader = kVertexShader;
			if (rSubObject.pMaterial->bAlphaCutout)
			{
				pDrawOp->graphics.vertexShader.flags |= RdrShaderFlags::AlphaCutout;
			}

			pDrawOp->graphics.hGeo = rSubObject.hGeo;
			pDrawOp->graphics.bHasAlpha = false;
			pDrawOp->graphics.bIsSky = true;
		}
	}
}