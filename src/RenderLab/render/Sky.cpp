#include "Precompiled.h"
#include "Sky.h"
#include "ModelData.h"
#include "RdrDrawOp.h"
#include "RdrComputeOp.h"
#include "RdrFrameMem.h"
#include "Renderer.h"
#include "AssetLib/SkyAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "../data/shaders/c_constants.h"

#define TRANSMITTANCE_LUT_WIDTH 256
#define TRANSMITTANCE_LUT_HEIGHT 128

// TODO: Reduce the sizes of these LUTs.  Elek09 uses 32x128x32 which is probably sufficient
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
		const char* assetName = m_pSrcAsset->assetName;
		Cleanup();
		Load(assetName);
		m_reloadPending = false;
	}
}

void Sky::QueueDraw(RdrDrawBuckets* pDrawBuckets, const RdrVolumetricFogData& rFogData)
{
	const uint kNumOrders = 4;
	
	static RdrMaterial s_material = { 0 };
	if (!s_material.hPixelShaders[0])
	{
		s_material.bNeedsLighting = false;
		s_material.hPixelShaders[0] = RdrShaderSystem::CreatePixelShaderFromFile("p_sky.hlsl", { 0 }, 0);
		s_material.ahTextures.assign(0, m_hScatteringSumLuts[kNumOrders % 2]);
		s_material.ahTextures.assign(1, m_hTransmittanceLut);
		s_material.aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false));
	}
	s_material.ahTextures.assign(2, rFogData.hFinalLut); // todo: this isn't thread-safe

	if (!m_hVsPerObjectConstantBuffer)
	{
		Matrix44 mtxWorld = Matrix44Translation(Vec3::kZero);
		uint constantsSize = sizeof(Vec4) * 4;
		Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
		*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);
		m_hVsPerObjectConstantBuffer = RdrResourceSystem::CreateConstantBuffer(pConstants, constantsSize, RdrCpuAccessFlags::None, RdrResourceUsage::Default);
	}


	uint numSubObjects = m_pModelData->GetNumSubObjects();
	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = m_pModelData->GetSubObject(i);

		RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp();
		pDrawOp->hVsConstants = m_hVsPerObjectConstantBuffer;
		pDrawOp->pMaterial = &s_material;

		pDrawOp->hInputLayout = s_hSkyInputLayout;
		pDrawOp->vertexShader = kVertexShader;

		pDrawOp->hGeo = rSubObject.hGeo;
		pDrawOp->bHasAlpha = false;

		pDrawBuckets->AddDrawOp(pDrawOp, RdrBucketType::Sky);
	}

	// Update atmosphere constants
	{
		uint constantsSize = sizeof(AtmosphereParams);
		AtmosphereParams* pParams = (AtmosphereParams*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pParams->planetRadius = 6360.f;
		pParams->atmosphereHeight = 60.f;
		pParams->atmosphereRadius = pParams->planetRadius + pParams->atmosphereHeight;
		pParams->mieScatteringCoeff = float3(5e-3f, 5e-3f, 5e-3f);
		pParams->mieG = 0.9f;
		pParams->mieAltitudeScale = 1.2f;
		pParams->rayleighAltitudeScale = 8.f;
		pParams->rayleighScatteringCoeff = float3(5.8e-3f, 1.35e-2f, 3.31e-2f);
		pParams->ozoneExtinctionCoeff = float3(0.f, 0.f, 0.f);
		pParams->averageGroundReflectance = 0.4f;
		pParams->sunDirection = GetSunDirection();
		pParams->sunColor = m_pSrcAsset->sun.color * m_pSrcAsset->sun.intensity;

		m_hAtmosphereConstants = RdrResourceSystem::CreateUpdateConstantBuffer(m_hAtmosphereConstants,
			pParams, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}

	m_pendingComputeOps = 0;
	if (m_bNeedsTransmittanceLutUpdate)
	{
		// Transmittance
		{
			RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
			pOp->shader = RdrComputeShader::AtmosphereTransmittanceLut;
			pOp->ahWritableResources.assign(0, m_hTransmittanceLut);
			pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
			pOp->threads[0] = RdrComputeOp::getThreadGroupCount(TRANSMITTANCE_LUT_WIDTH, ATM_LUT_THREADS_X);
			pOp->threads[1] = RdrComputeOp::getThreadGroupCount(TRANSMITTANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
			pOp->threads[2] = 1;

			pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
		}


		// Initial delta S (single scattering)
		{
			RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
			pOp->shader = RdrComputeShader::AtmosphereScatterLut_Single;
			pOp->ahResources.assign(0, m_hTransmittanceLut);
			pOp->aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false));
			pOp->ahWritableResources.assign(0, m_hScatteringRayleighDeltaLut);
			pOp->ahWritableResources.assign(1, m_hScatteringMieDeltaLut);
			pOp->ahWritableResources.assign(2, m_hScatteringSumLuts[0]);
			pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
			pOp->threads[0] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_WIDTH, ATM_LUT_THREADS_X);
			pOp->threads[1] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_HEIGHT, ATM_LUT_THREADS_Y);
			pOp->threads[2] = SCATTERING_LUT_DEPTH;

			pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
		}
		
		// Initial delta E (irradiance)
		{
			RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
			pOp->shader = RdrComputeShader::AtmosphereIrradianceLut_Initial;
			pOp->aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false));
			pOp->ahResources.assign(0, m_hTransmittanceLut);
			pOp->ahWritableResources.assign(0, m_hIrradianceDeltaLut);
			pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
			pOp->threads[0] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_WIDTH, ATM_LUT_THREADS_X);
			pOp->threads[1] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
			pOp->threads[2] = 1;

			pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
		}

		// Clear E (irradiance)
		{
			RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
			pOp->shader = RdrComputeShader::Clear2d;
			pOp->ahWritableResources.assign(0, m_hIrradianceSumLuts[0]);
			pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
			pOp->threads[0] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_WIDTH, ATM_LUT_THREADS_Y);
			pOp->threads[1] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
			pOp->threads[2] = 1;

			pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
		}

		uint currScatterIndex = 1;
		uint currIrradianceIndex = 1;
		for (uint i = 0; i < kNumOrders; ++i)
		{
			// Delta J (radiance)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
				pOp->aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false));
				pOp->ahResources.assign(0, m_hTransmittanceLut);
				pOp->ahResources.assign(1, m_hIrradianceDeltaLut);
				if (i == 0)
				{
					pOp->shader = RdrComputeShader::AtmosphereRadianceLut;
					pOp->ahResources.assign(2, m_hScatteringRayleighDeltaLut);
					pOp->ahResources.assign(3, m_hScatteringMieDeltaLut);
				}
				else
				{
					pOp->shader = RdrComputeShader::AtmosphereRadianceLut_CombinedScatter;
					pOp->ahResources.assign(2, m_hScatteringCombinedDeltaLut);
				}
				pOp->ahWritableResources.assign(0, m_hRadianceDeltaLut);
				pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(RADIANCE_LUT_WIDTH, ATM_LUT_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(RADIANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
				pOp->threads[2] = RADIANCE_LUT_DEPTH;

				pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
			}

			// Delta E (irradiance)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
				pOp->aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false));
				if (i == 0)
				{
					pOp->shader = RdrComputeShader::AtmosphereIrradianceLut_N;
					pOp->ahResources.assign(0, m_hScatteringRayleighDeltaLut);
					pOp->ahResources.assign(1, m_hScatteringMieDeltaLut);
				}
				else
				{
					pOp->shader = RdrComputeShader::AtmosphereIrradianceLut_N_CombinedScatter;
					pOp->ahResources.assign(0, m_hScatteringCombinedDeltaLut);
				}
				pOp->ahWritableResources.assign(0, m_hIrradianceDeltaLut);
				pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_WIDTH, ATM_LUT_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
				pOp->threads[2] = 1;

				pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
			}

			// Delta S (scatter)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
				pOp->shader = RdrComputeShader::AtmosphereScatterLut_N;
				pOp->aSamplers.assign(0, RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false));
				pOp->ahResources.assign(0, m_hTransmittanceLut);
				pOp->ahResources.assign(1, m_hRadianceDeltaLut);
				pOp->ahWritableResources.assign(0, m_hScatteringCombinedDeltaLut);
				pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_WIDTH, ATM_LUT_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_HEIGHT, ATM_LUT_THREADS_Y);
				pOp->threads[2] = SCATTERING_LUT_DEPTH;

				pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
			}

			// Sum E (irradiance)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
				pOp->shader = RdrComputeShader::Add2d;
				pOp->ahResources.assign(0, m_hIrradianceDeltaLut);
				pOp->ahResources.assign(1, m_hIrradianceSumLuts[!currIrradianceIndex]);
				pOp->ahWritableResources.assign(0, m_hIrradianceSumLuts[currIrradianceIndex]);
				pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_WIDTH, ADD_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_HEIGHT, ADD_THREADS_Y);
				pOp->threads[2] = 1;

				pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
			}

			// Sum S (scatter)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp();
				pOp->shader = RdrComputeShader::Add3d;
				pOp->ahResources.assign(0, m_hScatteringCombinedDeltaLut);
				pOp->ahResources.assign(1, m_hScatteringSumLuts[!currScatterIndex]);
				pOp->ahWritableResources.assign(0, m_hScatteringSumLuts[currScatterIndex]);
				pOp->ahConstantBuffers.assign(0, m_hAtmosphereConstants);
				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_WIDTH, ADD_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_HEIGHT, ADD_THREADS_Y);
				pOp->threads[2] = SCATTERING_LUT_DEPTH;

				pDrawBuckets->AddComputeOp(pOp, RdrPass::Sky);
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

float Sky::GetPssmLambda() const
{
	return m_pSrcAsset->shadows.pssmLambda;
}

const AssetLib::VolumetricFogSettings& Sky::GetVolFogSettings() const
{
	return m_pSrcAsset->volumetricFog;
}
