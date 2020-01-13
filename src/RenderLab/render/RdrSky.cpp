#include "Precompiled.h"
#include "RdrSky.h"
#include "Renderer.h"
#include "ModelData.h"
#include "RdrFrameMem.h"
#include "RdrComputeOp.h"
#include "Assetlib/SceneAsset.h"

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
	static constexpr uint kNumScatteringOrders = 4;

	static constexpr RdrVertexShader kVertexShader = { RdrVertexShaderType::Sky, RdrShaderFlags::None };
}

RdrSky::RdrSky()
	: m_hVsPerObjectConstantBuffer(0)
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

void RdrSky::LazyInit()
{
	if (m_pSkyDomeModel)
		return;

	m_pSkyDomeModel = ModelData::LoadFromFile("skydome");
	
	// Transmittance LUTs
	m_hTransmittanceLut = RdrResourceSystem::CreateTexture2D(TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));

	// Scattering LUTs
	m_hScatteringRayleighDeltaLut = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
	m_hScatteringMieDeltaLut = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
	m_hScatteringCombinedDeltaLut = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
	m_hScatteringSumLuts[0] = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
	m_hScatteringSumLuts[1] = RdrResourceSystem::CreateTexture3D(SCATTERING_LUT_WIDTH, SCATTERING_LUT_HEIGHT, SCATTERING_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));

	// Irradiance LUTs
	m_hIrradianceDeltaLut = RdrResourceSystem::CreateTexture2D(IRRADIANCE_LUT_WIDTH, IRRADIANCE_LUT_HEIGHT,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
	m_hIrradianceSumLuts[0] = RdrResourceSystem::CreateTexture2D(IRRADIANCE_LUT_WIDTH, IRRADIANCE_LUT_HEIGHT,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
	m_hIrradianceSumLuts[1] = RdrResourceSystem::CreateTexture2D(IRRADIANCE_LUT_WIDTH, IRRADIANCE_LUT_HEIGHT,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));

	// Radiance LUTs
	m_hRadianceDeltaLut = RdrResourceSystem::CreateTexture3D(RADIANCE_LUT_WIDTH, RADIANCE_LUT_HEIGHT, RADIANCE_LUT_DEPTH,
		RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));

	// Create the material
	const RdrResourceFormat* pRtvFormats;
	uint nNumRtvFormats;
	Renderer::GetStageRenderTargetFormats(RdrRenderStage::kScene, &pRtvFormats, &nNumRtvFormats);

	const RdrShader* pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_sky.hlsl", { 0 }, 0);
	
	RdrRasterState rasterState;
	rasterState.bWireframe = false;
	rasterState.bDoubleSided = false;
	rasterState.bEnableMSAA = (g_debugState.msaaLevel > 1);
	rasterState.bUseSlopeScaledDepthBias = false;
	rasterState.bEnableScissor = false;

	m_material.Init("Sky", RdrMaterialFlags::NeedsLighting);
	m_material.CreatePipelineState(RdrShaderMode::Normal,
		kVertexShader, pPixelShader,
		m_pSkyDomeModel->GetVertexElements(0), m_pSkyDomeModel->GetNumVertexElements(0), 
		pRtvFormats, nNumRtvFormats,
		RdrBlendMode::kOpaque,
		rasterState,
		RdrDepthStencilState(true, false, RdrComparisonFunc::Equal));

	RdrResourceHandle ahResourceHandles[2];
	ahResourceHandles[0] = m_hScatteringSumLuts[kNumScatteringOrders % 2];
	ahResourceHandles[1] = m_hTransmittanceLut;
	m_material.SetTextures(0, 2, ahResourceHandles);

	RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
	m_material.SetSamplers(0, 1, &sampler);
}

void RdrSky::AssignExternalResources(RdrResourceHandle hVolumetricFogLut)
{
	m_material.SetTextures(2, 1, &hVolumetricFogLut); // todo: this isn't thread-safe
}

void RdrSky::QueueDraw(RdrAction* pAction, const AssetLib::SkySettings& rSkySettings, const RdrLightList* pLightList, RdrConstantBufferHandle* phOutAtmosphereCb)
{
	LazyInit();

	//////////////////////////////////////////////////////////////////////////
	// Sky / atmosphere
	if (!m_hVsPerObjectConstantBuffer)
	{
		Matrix44 mtxWorld = Matrix44Translation(Vec3::kZero);
		uint constantsSize = sizeof(Vec4) * 4;
		Vec4* pConstants = (Vec4*)RdrFrameMem::AllocAligned(constantsSize, 16);
		*((Matrix44*)pConstants) = Matrix44Transpose(mtxWorld);
		m_hVsPerObjectConstantBuffer = RdrResourceSystem::CreateConstantBuffer(pConstants, constantsSize,
			RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_BACKPOINTER(this));
	}

	uint numSubObjects = m_pSkyDomeModel->GetNumSubObjects();
	for (uint i = 0; i < numSubObjects; ++i)
	{
		const ModelData::SubObject& rSubObject = m_pSkyDomeModel->GetSubObject(i);

		RdrDrawOp* pDrawOp = RdrFrameMem::AllocDrawOp(CREATE_BACKPOINTER(this));
		pDrawOp->hVsConstants = m_hVsPerObjectConstantBuffer;
		pDrawOp->pMaterial = &m_material;

		pDrawOp->hGeo = rSubObject.hGeo;
		pDrawOp->bHasAlpha = false;

		pAction->AddDrawOp(pDrawOp, RdrBucketType::Sky);
	}

	// Update atmosphere constants
	RdrConstantBufferHandle hAtmosphereConstants = 0;
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

		// For now, just pick the first directional light.
		// TODO: Support multiple directional lights?
		if (pLightList->m_directionalLights.size() != 0)
		{
			const DirectionalLight& rSunLight = pLightList->m_directionalLights.get(0);
			pParams->sunDirection = rSunLight.direction;
			pParams->sunColor = rSunLight.color;
		}
		else
		{
			pParams->sunDirection = float3(0.f, -1.f, 0.f);
			pParams->sunColor = float3(0.f, 0.f, 0.f);
		}

		hAtmosphereConstants = RdrResourceSystem::CreateTempConstantBuffer(pParams, constantsSize, CREATE_BACKPOINTER(this));
	}

	m_pendingComputeOps = 0;
	if (m_bNeedsTransmittanceLutUpdate)
	{
		// Transmittance
		{
			RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
			pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::AtmosphereTransmittanceLut);
			pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hTransmittanceLut, 1, CREATE_BACKPOINTER(this));
			pOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hAtmosphereConstants, 1, CREATE_BACKPOINTER(this));
			pOp->threads[0] = RdrComputeOp::getThreadGroupCount(TRANSMITTANCE_LUT_WIDTH, ATM_LUT_THREADS_X);
			pOp->threads[1] = RdrComputeOp::getThreadGroupCount(TRANSMITTANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
			pOp->threads[2] = 1;

			pAction->AddComputeOp(pOp, RdrPass::Sky);
		}


		// Initial delta S (single scattering)
		{
			RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
			pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::AtmosphereScatterLut_Single);
			pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(&m_hTransmittanceLut, 1, CREATE_BACKPOINTER(this));
			
			RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
			pOp->pSamplerDescriptorTable = RdrResourceSystem::CreateTempSamplerTable(&sampler, 1, CREATE_BACKPOINTER(this));

			RdrResourceHandle hWritableResources[3] = { m_hScatteringRayleighDeltaLut, m_hScatteringMieDeltaLut, m_hScatteringSumLuts[0] };
			pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(hWritableResources, ARRAYSIZE(hWritableResources), CREATE_BACKPOINTER(this));

			pOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hAtmosphereConstants, 1, CREATE_BACKPOINTER(this));
			pOp->threads[0] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_WIDTH, ATM_LUT_THREADS_X);
			pOp->threads[1] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_HEIGHT, ATM_LUT_THREADS_Y);
			pOp->threads[2] = SCATTERING_LUT_DEPTH;

			pAction->AddComputeOp(pOp, RdrPass::Sky);
		}

		// Initial delta E (irradiance)
		{
			RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
			pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::AtmosphereIrradianceLut_Initial);

			RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
			pOp->pSamplerDescriptorTable = RdrResourceSystem::CreateTempSamplerTable(&sampler, 1, CREATE_BACKPOINTER(this));
			pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(&m_hTransmittanceLut, 1, CREATE_BACKPOINTER(this));
			pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hIrradianceDeltaLut, 1, CREATE_BACKPOINTER(this));
			pOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hAtmosphereConstants, 1, CREATE_BACKPOINTER(this));

			pOp->threads[0] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_WIDTH, ATM_LUT_THREADS_X);
			pOp->threads[1] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
			pOp->threads[2] = 1;

			pAction->AddComputeOp(pOp, RdrPass::Sky);
		}

		// Clear E (irradiance)
		{
			RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
			pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::Clear2d);
			pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hIrradianceSumLuts[0], 1, CREATE_BACKPOINTER(this));
			pOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hAtmosphereConstants, 1, CREATE_BACKPOINTER(this));
			pOp->threads[0] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_WIDTH, ATM_LUT_THREADS_Y);
			pOp->threads[1] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
			pOp->threads[2] = 1;

			pAction->AddComputeOp(pOp, RdrPass::Sky);
		}

		uint currScatterIndex = 1;
		uint currIrradianceIndex = 1;
		for (uint i = 0; i < kNumScatteringOrders; ++i)
		{
			// Delta J (radiance)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));

				RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
				pOp->pSamplerDescriptorTable = RdrResourceSystem::CreateTempSamplerTable(&sampler, 1, CREATE_BACKPOINTER(this));

				if (i == 0)
				{
					pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::AtmosphereRadianceLut);

					RdrResourceHandle ahResources[] = { m_hTransmittanceLut, m_hIrradianceDeltaLut, m_hScatteringRayleighDeltaLut, m_hScatteringMieDeltaLut };
					pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));
				}
				else
				{
					pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::AtmosphereRadianceLut_CombinedScatter);

					RdrResourceHandle ahResources[] = { m_hTransmittanceLut, m_hIrradianceDeltaLut, m_hScatteringCombinedDeltaLut };
					pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));
				}

				pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hRadianceDeltaLut, 1, CREATE_BACKPOINTER(this));
				pOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hAtmosphereConstants, 1, CREATE_BACKPOINTER(this));

				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(RADIANCE_LUT_WIDTH, ATM_LUT_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(RADIANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
				pOp->threads[2] = RADIANCE_LUT_DEPTH;

				pAction->AddComputeOp(pOp, RdrPass::Sky);
			}

			// Delta E (irradiance)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));

				RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
				pOp->pSamplerDescriptorTable = RdrResourceSystem::CreateTempSamplerTable(&sampler, 1, CREATE_BACKPOINTER(this));

				if (i == 0)
				{
					pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::AtmosphereIrradianceLut_N);

					RdrResourceHandle ahResources[] = { m_hScatteringRayleighDeltaLut, m_hScatteringMieDeltaLut };
					pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));
				}
				else
				{
					pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::AtmosphereIrradianceLut_N_CombinedScatter);
					pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(&m_hScatteringCombinedDeltaLut, 1, CREATE_BACKPOINTER(this));
				}

				pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hIrradianceDeltaLut, 1, CREATE_BACKPOINTER(this));
				pOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hAtmosphereConstants, 1, CREATE_BACKPOINTER(this));

				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_WIDTH, ATM_LUT_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_HEIGHT, ATM_LUT_THREADS_Y);
				pOp->threads[2] = 1;

				pAction->AddComputeOp(pOp, RdrPass::Sky);
			}

			// Delta S (scatter)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
				pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::AtmosphereScatterLut_N);

				RdrSamplerState sampler(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
				pOp->pSamplerDescriptorTable = RdrResourceSystem::CreateTempSamplerTable(&sampler, 1, CREATE_BACKPOINTER(this));

				RdrResourceHandle ahResources[] = { m_hTransmittanceLut, m_hRadianceDeltaLut };
				pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));
				pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hScatteringCombinedDeltaLut, 1, CREATE_BACKPOINTER(this));
				pOp->pConstantsDescriptorTable = RdrResourceSystem::CreateTempConstantBufferTable(&hAtmosphereConstants, 1, CREATE_BACKPOINTER(this));

				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_WIDTH, ATM_LUT_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_HEIGHT, ATM_LUT_THREADS_Y);
				pOp->threads[2] = SCATTERING_LUT_DEPTH;

				pAction->AddComputeOp(pOp, RdrPass::Sky);
			}

			// Sum E (irradiance)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
				pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::Add2d);

				RdrResourceHandle ahResources[] = { m_hIrradianceDeltaLut, m_hIrradianceSumLuts[!currIrradianceIndex] };
				pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));

				pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hIrradianceSumLuts[currIrradianceIndex], 1, CREATE_BACKPOINTER(this));

				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_WIDTH, ADD_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(IRRADIANCE_LUT_HEIGHT, ADD_THREADS_Y);
				pOp->threads[2] = 1;

				pAction->AddComputeOp(pOp, RdrPass::Sky);
			}

			// Sum S (scatter)
			{
				RdrComputeOp* pOp = RdrFrameMem::AllocComputeOp(CREATE_BACKPOINTER(this));
				pOp->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::Add3d);

				RdrResourceHandle ahResources[] = { m_hScatteringCombinedDeltaLut, m_hScatteringSumLuts[!currScatterIndex] };
				pOp->pResourceDescriptorTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));

				pOp->pUnorderedAccessDescriptorTable = RdrResourceSystem::CreateTempUnorderedAccessViewTable(&m_hScatteringSumLuts[currScatterIndex], 1, CREATE_BACKPOINTER(this));

				pOp->threads[0] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_WIDTH, ADD_THREADS_X);
				pOp->threads[1] = RdrComputeOp::getThreadGroupCount(SCATTERING_LUT_HEIGHT, ADD_THREADS_Y);
				pOp->threads[2] = SCATTERING_LUT_DEPTH;

				pAction->AddComputeOp(pOp, RdrPass::Sky);
			}

			currIrradianceIndex = !currIrradianceIndex;
			currScatterIndex = !currScatterIndex;
		}

		m_bNeedsTransmittanceLutUpdate = false;
	}

	*phOutAtmosphereCb = hAtmosphereConstants;
}
