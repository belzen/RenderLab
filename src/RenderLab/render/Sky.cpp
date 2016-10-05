#include "Precompiled.h"
#include "Sky.h"
#include "ModelData.h"
#include "RdrDrawOp.h"
#include "RdrComputeOp.h"
#include "RdrScratchMem.h"
#include "Renderer.h"
#include "AssetLib/SkyAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "../data/shaders/c_constants.h"

#define TRANSMITTANCE_LUT_WIDTH 256
#define TRANSMITTANCE_LUT_HEIGHT 128

#define SCATTERING_LUT_WIDTH 256
#define SCATTERING_LUT_HEIGHT 128
#define SCATTERING_LUT_DEPTH 32

#define IRRADIANCE_LUT_WIDTH 256
#define IRRADIANCE_LUT_HEIGHT 128

#define RADIANCE_LUT_WIDTH 256
#define RADIANCE_LUT_HEIGHT 128
#define RADIANCE_LUT_DEPTH 32

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
		AssetLib::Sky::GetAssetDef().ExtractAssetName(filename, skyName, ARRAY_SIZE(skyName));

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
	, m_pSrcAsset(nullptr)
	, m_reloadListenerId(0)
	, m_reloadPending(false)
	, m_hAtmosphereConstants(0)
	, m_hTransmittanceLut(0)
	, m_hScatteringRayleighDeltaLut(0)
	, m_hScatteringMieDeltaLut(0)
	, m_hScatteringCombinedDeltaLut(0)
	, m_hIrradianceDeltaLut(0)
	, m_hRadianceDeltaLut(0)
	, m_bNeedsTransmittanceLutUpdate(true)
	, m_pendingComputeOps(0)
{
	memset(m_hScatteringSumLuts, 0, sizeof(m_hScatteringSumLuts));
	memset(m_hIrradianceSumLuts, 0, sizeof(m_hIrradianceSumLuts));
}

void Sky::Cleanup()
{
	m_pModelData = nullptr;
	m_pMaterial = nullptr;
	m_pSrcAsset = nullptr;
	FileWatcher::RemoveListener(m_reloadListenerId);
	m_reloadListenerId = 0;
	m_reloadPending = false;
	m_bNeedsTransmittanceLutUpdate = true;
	m_pendingComputeOps = 0;

	RdrResourceSystem::ReleaseConstantBuffer(m_hAtmosphereConstants);
	RdrResourceSystem::ReleaseConstantBuffer(m_hVsPerObjectConstantBuffer);
	RdrResourceSystem::ReleaseResource(m_hTransmittanceLut);
	RdrResourceSystem::ReleaseResource(m_hScatteringRayleighDeltaLut);
	RdrResourceSystem::ReleaseResource(m_hScatteringMieDeltaLut);
	RdrResourceSystem::ReleaseResource(m_hScatteringCombinedDeltaLut);
	RdrResourceSystem::ReleaseResource(m_hIrradianceDeltaLut);
	RdrResourceSystem::ReleaseResource(m_hRadianceDeltaLut);
	RdrResourceSystem::ReleaseResource(m_hScatteringSumLuts[0]);
	RdrResourceSystem::ReleaseResource(m_hScatteringSumLuts[1]);
	RdrResourceSystem::ReleaseResource(m_hIrradianceSumLuts[0]);
	RdrResourceSystem::ReleaseResource(m_hIrradianceSumLuts[1]);
	m_hAtmosphereConstants = 0;
	m_hVsPerObjectConstantBuffer = 0;
	m_hAtmosphereConstants = 0;
	m_hTransmittanceLut = 0;
	m_hScatteringRayleighDeltaLut = 0;
	m_hScatteringMieDeltaLut = 0;
	m_hScatteringCombinedDeltaLut = 0;
	m_hIrradianceDeltaLut = 0;
	m_hRadianceDeltaLut = 0;
	memset(m_hScatteringSumLuts, 0, sizeof(m_hScatteringSumLuts));
	memset(m_hIrradianceSumLuts, 0, sizeof(m_hIrradianceSumLuts));
}

void Sky::Reload()
{
	m_reloadPending = true;
}

const char* Sky::GetName() const
{
	return m_pSrcAsset->assetName;
}

void Sky::Load(const char* skyName)
{
	m_pSrcAsset = AssetLibrary<AssetLib::Sky>::LoadAsset(skyName);

	m_pModelData = ModelData::LoadFromFile(m_pSrcAsset->model);
	m_pMaterial = RdrMaterial::LoadFromFile(m_pSrcAsset->material);

	if (!s_hSkyInputLayout)
	{
		s_hSkyInputLayout = RdrShaderSystem::CreateInputLayout(kVertexShader, s_skyVertexDesc, ARRAY_SIZE(s_skyVertexDesc));
	}

	// Listen for changes of the sky file.
	char filePattern[AssetLib::AssetDef::kMaxNameLen];
	AssetLib::Sky::GetAssetDef().GetFilePattern(filePattern, ARRAY_SIZE(filePattern));
	m_reloadListenerId = FileWatcher::AddListener(filePattern, handleSkyFileChanged, this);

	// Transmittance LUTs
	m_hTransmittanceLut = RdrResourceSystem::CreateTexture2D(TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT, 
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
	
	// Scattering LUTs
	m_hScatteringRayleighDeltaLut = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
	m_hScatteringMieDeltaLut = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
	m_hScatteringCombinedDeltaLut = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
	m_hScatteringSumLuts[0] = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
	m_hScatteringSumLuts[1] = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
	
	// Irradiance LUTs
	m_hIrradianceDeltaLut = RdrResourceSystem::CreateTexture2D(IRRADIANCE_LUT_WIDTH, IRRADIANCE_LUT_HEIGHT, 
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
	m_hIrradianceSumLuts[0] = RdrResourceSystem::CreateTexture2D(IRRADIANCE_LUT_WIDTH, IRRADIANCE_LUT_HEIGHT,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
	m_hIrradianceSumLuts[1] = RdrResourceSystem::CreateTexture2D(IRRADIANCE_LUT_WIDTH, IRRADIANCE_LUT_HEIGHT,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);

	// Radiance LUTs
	m_hRadianceDeltaLut = RdrResourceSystem::CreateTexture3D(RADIANCE_LUT_WIDTH, RADIANCE_LUT_HEIGHT, RADIANCE_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
}

void Sky::Update(float dt)
{
	if (m_reloadPending)
	{
		Cleanup();
		Load(m_pSrcAsset->assetName);
		m_reloadPending = false;
	}
}

void Sky::PrepareDraw()
{
	const uint kNumOrders = 4;
	
	static RdrMaterial s_material = { 0 };
	if (!s_material.hPixelShaders[0])
	{
		s_material.bNeedsLighting = false;
		s_material.hPixelShaders[0] = RdrShaderSystem::CreatePixelShaderFromFile("p_sky.hlsl", { 0 }, 0);
		s_material.hTextures[0] = m_hScatteringSumLuts[kNumOrders % 2];
		s_material.hTextures[1] = m_hTransmittanceLut;
		s_material.texCount = 2;
		s_material.samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		s_material.samplers[1] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		s_material.samplers[2] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		s_material.samplers[3] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
	}

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

		if (g_debugState.rebuildDrawOps && m_pSubObjectDrawOps[i])
		{
			RdrDrawOp::QueueRelease(m_pSubObjectDrawOps[i]);
			m_pSubObjectDrawOps[i] = nullptr;
		}

		if (!m_pSubObjectDrawOps[i])
		{
			RdrDrawOp* pDrawOp = m_pSubObjectDrawOps[i] = RdrDrawOp::Allocate();
			pDrawOp->hVsConstants = m_hVsPerObjectConstantBuffer;
			pDrawOp->pMaterial = &s_material;

			pDrawOp->hInputLayout = s_hSkyInputLayout;
			pDrawOp->vertexShader = kVertexShader;

			pDrawOp->hGeo = rSubObject.hGeo;
			pDrawOp->bHasAlpha = false;
			pDrawOp->bIsSky = true;
		}
	}

	// Update atmosphere constants
	{
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(AtmosphereParams));
		AtmosphereParams* pParams = (AtmosphereParams*)RdrScratchMem::AllocAligned(constantsSize, 16);
		pParams->planetRadius = 6360.f;
		pParams->atmosphereHeight = 60.f;
		pParams->atmosphereRadius = pParams->planetRadius + pParams->atmosphereHeight;
		pParams->mieScatteringCoeff = float3(5e-3f, 5e-3f, 5e-3f);
		pParams->mieG = 0.9f;
		pParams->mieAltitudeScale = 1.2f;
		pParams->rayleighAltitudeScale = 8.f;
		pParams->rayleighScatteringCoeff = float3(5.8e-3f, 1.35e-2f, 3.31e-2f);
		pParams->ozoneExtinctionCoeff = float3(0.f, 0.f, 0.f);
		pParams->averageGroundReflectance = 0.1f;
		pParams->sunDirection = GetSunDirection();
		pParams->sunColor = m_pSrcAsset->sun.color * m_pSrcAsset->sun.intensity;

		if (!m_hAtmosphereConstants)
			m_hAtmosphereConstants = RdrResourceSystem::CreateConstantBuffer(pParams, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
		else
			RdrResourceSystem::UpdateConstantBuffer(m_hAtmosphereConstants, pParams);
	}

	m_pendingComputeOps = 0;
	if (m_bNeedsTransmittanceLutUpdate)
	{
		// Transmittance
		{
			RdrComputeOp* pOp = RdrComputeOp::Allocate();
			pOp->shader = RdrComputeShader::AtmosphereTransmittanceLut;
			pOp->hWritableResources[0] = m_hTransmittanceLut;
			pOp->writableResourceCount = 1;
			pOp->resourceCount = 0;
			pOp->hCsConstants = m_hAtmosphereConstants;
			pOp->threads[0] = (TRANSMITTANCE_LUT_WIDTH + (LUT_THREADS_X - 1)) / LUT_THREADS_X;
			pOp->threads[1] = (TRANSMITTANCE_LUT_HEIGHT + (LUT_THREADS_Y - 1)) / LUT_THREADS_Y;
			pOp->threads[2] = 1;

			m_pComputeOps[m_pendingComputeOps++] = pOp;
		}


		// Initial delta S (single scattering)
		{
			RdrComputeOp* pOp = RdrComputeOp::Allocate();
			pOp->shader = RdrComputeShader::AtmosphereScatterLut_Single;
			pOp->hResources[0] = m_hTransmittanceLut;
			pOp->resourceCount = 1;
			pOp->samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
			pOp->samplerCount = 1;
			pOp->hWritableResources[0] = m_hScatteringRayleighDeltaLut;
			pOp->hWritableResources[1] = m_hScatteringMieDeltaLut;
			pOp->hWritableResources[2] = m_hScatteringSumLuts[0];
			pOp->writableResourceCount = 3;
			pOp->hCsConstants = m_hAtmosphereConstants;
			pOp->threads[0] = (SCATTERING_LUT_WIDTH + (LUT_THREADS_X - 1)) / LUT_THREADS_X;
			pOp->threads[1] = (SCATTERING_LUT_HEIGHT + (LUT_THREADS_Y - 1)) / LUT_THREADS_Y;
			pOp->threads[2] = SCATTERING_LUT_DEPTH;

			m_pComputeOps[m_pendingComputeOps++] = pOp;
		}

		// Initial delta E (irradiance)
		{
			RdrComputeOp* pOp = RdrComputeOp::Allocate();
			pOp->shader = RdrComputeShader::AtmosphereIrradianceLut_Initial;
			pOp->samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
			pOp->samplerCount = 1;
			pOp->hResources[0] = m_hTransmittanceLut;
			pOp->resourceCount = 1;
			pOp->hWritableResources[0] = m_hIrradianceDeltaLut;
			pOp->writableResourceCount = 1;
			pOp->hCsConstants = m_hAtmosphereConstants;
			pOp->threads[0] = (IRRADIANCE_LUT_WIDTH + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
			pOp->threads[1] = (IRRADIANCE_LUT_HEIGHT + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
			pOp->threads[2] = 1;

			m_pComputeOps[m_pendingComputeOps++] = pOp;
		}

		// Clear E (irradiance)
		{
			RdrComputeOp* pOp = RdrComputeOp::Allocate();
			pOp->shader = RdrComputeShader::Clear2d;
			pOp->samplerCount = 0;
			pOp->resourceCount = 0;
			pOp->hWritableResources[0] = m_hIrradianceSumLuts[0];
			pOp->writableResourceCount = 1;
			pOp->hCsConstants = m_hAtmosphereConstants;
			pOp->threads[0] = (IRRADIANCE_LUT_WIDTH + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
			pOp->threads[1] = (IRRADIANCE_LUT_HEIGHT + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
			pOp->threads[2] = 1;

			m_pComputeOps[m_pendingComputeOps++] = pOp;
		}

		uint currScatterIndex = 1;
		uint currIrradianceIndex = 1;
		for (uint i = 0; i < kNumOrders; ++i)
		{
			// Delta J (radiance)
			{
				RdrComputeOp* pOp = RdrComputeOp::Allocate();
				pOp->samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
				pOp->samplerCount = 1;
				pOp->hResources[0] = m_hTransmittanceLut;
				pOp->hResources[1] = m_hIrradianceDeltaLut;
				if (i == 0)
				{
					pOp->shader = RdrComputeShader::AtmosphereRadianceLut;
					pOp->hResources[2] = m_hScatteringRayleighDeltaLut;
					pOp->hResources[3] = m_hScatteringMieDeltaLut;
					pOp->resourceCount = 4;
				}
				else
				{
					pOp->shader = RdrComputeShader::AtmosphereRadianceLut_CombinedScatter;
					pOp->hResources[2] = m_hScatteringCombinedDeltaLut;
					pOp->resourceCount = 3;
				}
				pOp->hWritableResources[0] = m_hRadianceDeltaLut;
				pOp->writableResourceCount = 1;
				pOp->hCsConstants = m_hAtmosphereConstants;
				pOp->threads[0] = (RADIANCE_LUT_WIDTH + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
				pOp->threads[1] = (RADIANCE_LUT_HEIGHT + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
				pOp->threads[2] = RADIANCE_LUT_DEPTH;

				m_pComputeOps[m_pendingComputeOps++] = pOp;
			}

			// Delta E (irradiance)
			{
				RdrComputeOp* pOp = RdrComputeOp::Allocate();
				pOp->samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
				pOp->samplerCount = 1;
				if (i == 0)
				{
					pOp->shader = RdrComputeShader::AtmosphereIrradianceLut_N;
					pOp->hResources[0] = m_hScatteringRayleighDeltaLut;
					pOp->hResources[1] = m_hScatteringMieDeltaLut;
					pOp->resourceCount = 2;
				}
				else
				{
					pOp->shader = RdrComputeShader::AtmosphereIrradianceLut_N_CombinedScatter;
					pOp->hResources[0] = m_hScatteringCombinedDeltaLut;
					pOp->resourceCount = 1;
				}
				pOp->hWritableResources[0] = m_hIrradianceDeltaLut;
				pOp->writableResourceCount = 1;
				pOp->hCsConstants = m_hAtmosphereConstants;
				pOp->threads[0] = (IRRADIANCE_LUT_WIDTH + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
				pOp->threads[1] = (IRRADIANCE_LUT_HEIGHT + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
				pOp->threads[2] = 1;

				m_pComputeOps[m_pendingComputeOps++] = pOp;
			}

			// Delta S (scatter)
			{
				RdrComputeOp* pOp = RdrComputeOp::Allocate();
				pOp->shader = RdrComputeShader::AtmosphereScatterLut_N;
				pOp->samplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
				pOp->samplerCount = 1;
				pOp->hResources[0] = m_hTransmittanceLut;
				pOp->hResources[1] = m_hRadianceDeltaLut;
				pOp->resourceCount = 2;
				pOp->hWritableResources[0] = m_hScatteringCombinedDeltaLut;
				pOp->writableResourceCount = 1;
				pOp->hCsConstants = m_hAtmosphereConstants;
				pOp->threads[0] = (SCATTERING_LUT_WIDTH + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
				pOp->threads[1] = (SCATTERING_LUT_HEIGHT + (LUT_THREADS_X - 1)) / LUT_THREADS_Y;
				pOp->threads[2] = SCATTERING_LUT_DEPTH;

				m_pComputeOps[m_pendingComputeOps++] = pOp;
			}

			// Sum E (irradiance)
			{
				RdrComputeOp* pOp = RdrComputeOp::Allocate();
				pOp->shader = RdrComputeShader::Add2d;
				pOp->samplerCount = 0;
				pOp->hResources[0] = m_hIrradianceDeltaLut;
				pOp->hResources[1] = m_hIrradianceSumLuts[!currIrradianceIndex];
				pOp->resourceCount = 2;
				pOp->hWritableResources[0] = m_hIrradianceSumLuts[currIrradianceIndex];
				pOp->writableResourceCount = 1;
				pOp->hCsConstants = m_hAtmosphereConstants;
				pOp->threads[0] = (IRRADIANCE_LUT_WIDTH + (ADD_THREADS_X - 1)) / ADD_THREADS_X;
				pOp->threads[1] = (IRRADIANCE_LUT_HEIGHT + (ADD_THREADS_Y - 1)) / ADD_THREADS_Y;
				pOp->threads[2] = 1;

				m_pComputeOps[m_pendingComputeOps++] = pOp;
			}

			// Sum S (scatter)
			{
				RdrComputeOp* pOp = RdrComputeOp::Allocate();
				pOp->shader = RdrComputeShader::Add3d;
				pOp->samplerCount = 0;
				pOp->hResources[0] = m_hScatteringCombinedDeltaLut;
				pOp->hResources[1] = m_hScatteringSumLuts[!currScatterIndex];
				pOp->resourceCount = 2;
				pOp->hWritableResources[0] = m_hScatteringSumLuts[currScatterIndex];
				pOp->writableResourceCount = 1;
				pOp->hCsConstants = m_hAtmosphereConstants;
				pOp->threads[0] = (SCATTERING_LUT_WIDTH + (ADD_THREADS_X - 1)) / ADD_THREADS_X;
				pOp->threads[1] = (SCATTERING_LUT_HEIGHT + (ADD_THREADS_Y - 1)) / ADD_THREADS_Y;
				pOp->threads[2] = SCATTERING_LUT_DEPTH;

				m_pComputeOps[m_pendingComputeOps++] = pOp;
			}

			currIrradianceIndex = !currIrradianceIndex;
			currScatterIndex = !currScatterIndex;
		}

		m_bNeedsTransmittanceLutUpdate = false;
	}
}

Vec3 Sky::GetSunDirection() const
{
	float xAxis = Maths::DegToRad(m_pSrcAsset->sun.angleXAxis);
	float zAxis = Maths::DegToRad(m_pSrcAsset->sun.angleZAxis);
	return -Vec3(cosf(xAxis), sinf(xAxis), 0.f);
}

Light Sky::GetSunLight() const
{
	Light light;
	light.type = LightType::Directional;
	light.castsShadows = true;
	light.color = m_pSrcAsset->sun.color * m_pSrcAsset->sun.intensity;
	light.direction = GetSunDirection();
	light.shadowMapIndex = -1;

	return light;
}