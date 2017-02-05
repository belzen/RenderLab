#include "Precompiled.h"
#include "RdrAction.h"
#include "Renderer.h"
#include "RdrFrameMem.h"
#include "Scene.h"
#include "components/SkyVolume.h"
#include "RdrComputeOp.h"

namespace
{
	const uint kMaxInstancesPerDraw = 2048;

	struct  
	{
		FreeList<RdrAction, MAX_ACTIONS_PER_FRAME * 2> actions;
		RdrLighting lighting;
		RdrSky sky;
		RdrPostProcess postProcess;

		RdrActionSurfaces primarySurfaces;
		std::vector<RdrActionSurfaces> offscreenSurfaces;

		// Instance object ID data.
		struct
		{
			RdrConstantBuffer buffer;
			char* pData; // Original non-aligned pointer.
			uint* ids;
		} instanceIds[8];
		uint currentInstanceIds;

	} s_actionSharedData;

	// todo - move these to start at slot 0
	enum class RdrPsResourceSlots
	{
		EnvironmentMaps = 11,
		VolumetricFogLut = 12,
		SkyTransmittance = 13,
		ShadowMaps = 14,
		ShadowCubeMaps = 15,
		SpotLightList = 16,
		PointLightList = 17,
		LightIds = 18,
	};

	enum class RdrPsSamplerSlots
	{
		Clamp = 14,
		ShadowMap = 15,
	};

	// Pass to bucket mappings
	RdrBucketType s_passBuckets[] = {
		RdrBucketType::Opaque,	      // RdrPass::ZPrepass
		RdrBucketType::LightCulling,  // RdrPass::LightCulling
		RdrBucketType::VolumetricFog, // RdrPass::VolumetricFog
		RdrBucketType::Opaque,	      // RdrPass::Opaque
		RdrBucketType::Sky,	          // RdrPass::Sky
		RdrBucketType::Alpha,		  // RdrPass::Alpha
		RdrBucketType::Editor,		  // RdrPass::Editor
		RdrBucketType::Wireframe,     // RdrPass::Wireframe
		RdrBucketType::UI,		      // RdrPass::UI
	};
	static_assert(sizeof(s_passBuckets) / sizeof(s_passBuckets[0]) == (int)RdrPass::Count, "Missing pass -> bucket mappings");

	// Event names for passes
	static const wchar_t* s_passNames[] =
	{
		L"Z-Prepass",		// RdrPass::ZPrepass
		L"Light Culling",	// RdrPass::LightCulling
		L"Volumetric Fog",	// RdrPass::VolumetricFog
		L"Opaque",			// RdrPass::Opaque
		L"Sky",				// RdrPass::Sky
		L"Alpha",			// RdrPass::Alpha
		L"Editor",			// RdrPass::Editor
		L"Wireframe",		// RdrPass::Wireframe
		L"UI",				// RdrPass::UI
	};
	static_assert(sizeof(s_passNames) / sizeof(s_passNames[0]) == (int)RdrPass::Count, "Missing RdrPass names!");

	// Profile sections for passes
	static const RdrProfileSection s_passProfileSections[] =
	{
		RdrProfileSection::ZPrepass,		// RdrPass::ZPrepass
		RdrProfileSection::LightCulling,	// RdrPass::LightCulling
		RdrProfileSection::VolumetricFog,	// RdrPass::VolumetricFog
		RdrProfileSection::Opaque,			// RdrPass::Opaque
		RdrProfileSection::Sky,				// RdrPass::Sky
		RdrProfileSection::Alpha,			// RdrPass::Alpha
		RdrProfileSection::Editor,			// RdrPass::Editor
		RdrProfileSection::Wireframe,		// RdrPass::Wireframe
		RdrProfileSection::UI,				// RdrPass::UI
	};
	static_assert(sizeof(s_passProfileSections) / sizeof(s_passProfileSections[0]) == (int)RdrPass::Count, "Missing RdrPass profile sections!");



	RdrActionSurfaces* createActionSurfaces(RdrResourceCommandList& rResCommands, bool isPrimaryAction, uint width, uint height, int msaaLevel)
	{
		Vec2 viewportSize = Vec2((float)width, (float)height);
		RdrActionSurfaces* pSurfaces = nullptr;

		if (isPrimaryAction)
		{
			pSurfaces = &s_actionSharedData.primarySurfaces;
		}
		else
		{
			for (RdrActionSurfaces& rOffscreenSurfaces : s_actionSharedData.offscreenSurfaces)
			{
				if (rOffscreenSurfaces.viewportSize.x >= viewportSize.x && rOffscreenSurfaces.viewportSize.y >= viewportSize.y)
				{
					// The surface is usable if it is >= than the desired viewport size.  Larger surfaces can be scaled down using viewports.
					pSurfaces = &rOffscreenSurfaces;
					break;
				}
			}

			if (!pSurfaces)
			{
				// Couldn't find any usable offscreen buffers.  Create a new set.
				s_actionSharedData.offscreenSurfaces.emplace_back(RdrActionSurfaces());
				pSurfaces = &s_actionSharedData.offscreenSurfaces.back();
			}
		}

		if (pSurfaces->viewportSize.x != viewportSize.x || pSurfaces->viewportSize.y != viewportSize.y)
		{
			pSurfaces->viewportSize = viewportSize;

			// Release existing resources
			if (pSurfaces->hDepthStencilView)
				rResCommands.ReleaseDepthStencilView(pSurfaces->hDepthStencilView);
			if (pSurfaces->hDepthBuffer)
				rResCommands.ReleaseResource(pSurfaces->hDepthBuffer);

			rResCommands.ReleaseRenderTarget2d(pSurfaces->colorBuffer);
			rResCommands.ReleaseRenderTarget2d(pSurfaces->albedoBuffer);
			rResCommands.ReleaseRenderTarget2d(pSurfaces->normalBuffer);

			// Create resized buffers
			pSurfaces->colorBuffer = rResCommands.InitRenderTarget2d(width, height, RdrResourceFormat::R16G16B16A16_FLOAT, msaaLevel);
			pSurfaces->albedoBuffer = rResCommands.InitRenderTarget2d(width, height, RdrResourceFormat::B8G8R8A8_UNORM, msaaLevel);
			pSurfaces->normalBuffer = rResCommands.InitRenderTarget2d(width, height, RdrResourceFormat::B8G8R8A8_UNORM, msaaLevel);
			// Depth Buffer
			pSurfaces->hDepthBuffer = rResCommands.CreateTexture2DMS(width, height, RdrResourceFormat::D24_UNORM_S8_UINT,
				g_debugState.msaaLevel, RdrResourceUsage::Default, RdrResourceBindings::kNone);
			pSurfaces->hDepthStencilView = rResCommands.CreateDepthStencilView(pSurfaces->hDepthBuffer);

			if (isPrimaryAction)
			{
				// Post-proc resources just size to the largest required viewport and are re-used for each action.
				// TODO: This makes the assumption the primary action is always the largest which may not always be valid.
				s_actionSharedData.postProcess.ResizeResources(width, height);
			}
		}

		return pSurfaces;
	}


	void createPerActionConstants(RdrResourceCommandList& rResCommandList, const Camera& rCamera, const Rect& rViewport, RdrGlobalConstants& rConstants)
	{
		Matrix44 mtxView;
		Matrix44 mtxProj;

		rCamera.GetMatrices(mtxView, mtxProj);

		// VS
		uint constantsSize = sizeof(VsPerAction);
		VsPerAction* pVsPerAction = (VsPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);
		pVsPerAction->cameraPosition = rCamera.GetPosition();

		rConstants.hVsPerAction = rResCommandList.CreateTempConstantBuffer(pVsPerAction, constantsSize);

		// PS
		constantsSize = sizeof(PsPerAction);
		PsPerAction* pPsPerAction = (PsPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);

		pPsPerAction->cameraPos = rCamera.GetPosition();
		pPsPerAction->cameraDir = rCamera.GetDirection();
		pPsPerAction->mtxView = mtxView;
		pPsPerAction->mtxView = Matrix44Transpose(pPsPerAction->mtxView);
		pPsPerAction->mtxProj = mtxProj;
		pPsPerAction->mtxProj = Matrix44Transpose(pPsPerAction->mtxProj);
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->mtxInvProj = Matrix44Transpose(pPsPerAction->mtxInvProj);
		pPsPerAction->viewSize.x = (uint)rViewport.width;
		pPsPerAction->viewSize.y = (uint)rViewport.height;
		pPsPerAction->cameraNearDist = rCamera.GetNearDist();
		pPsPerAction->cameraFarDist = rCamera.GetFarDist();
		pPsPerAction->cameraFovY = rCamera.GetFieldOfViewY();
		pPsPerAction->aspectRatio = rCamera.GetAspectRatio();

		rConstants.hPsPerAction = rResCommandList.CreateTempConstantBuffer(pPsPerAction, constantsSize);
	}

	void createUiConstants(RdrResourceCommandList& rResCommandList, const Rect& rViewport, RdrGlobalConstants& rConstants)
	{
		Matrix44 mtxView = Matrix44::kIdentity;
		Matrix44 mtxProj = DirectX::XMMatrixOrthographicLH((float)rViewport.width, (float)rViewport.height, 0.01f, 1000.f);

		// VS
		uint constantsSize = sizeof(VsPerAction);
		VsPerAction* pVsPerAction = (VsPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);
		pVsPerAction->cameraPosition = Vec3::kZero;

		rConstants.hVsPerAction = rResCommandList.CreateTempConstantBuffer(pVsPerAction, constantsSize);

		// PS
		constantsSize = sizeof(PsPerAction);
		PsPerAction* pPsPerAction = (PsPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);

		pPsPerAction->cameraPos = Vec3::kOrigin;
		pPsPerAction->cameraDir = Vec3::kUnitZ;
		pPsPerAction->mtxView = mtxView;
		pPsPerAction->mtxView = Matrix44Transpose(pPsPerAction->mtxView);
		pPsPerAction->mtxProj = mtxProj;
		pPsPerAction->mtxProj = Matrix44Transpose(pPsPerAction->mtxProj);
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->mtxInvProj = Matrix44Transpose(pPsPerAction->mtxInvProj);
		pPsPerAction->viewSize.x = (uint)rViewport.width;
		pPsPerAction->viewSize.y = (uint)rViewport.height;

		rConstants.hPsPerAction = rResCommandList.CreateTempConstantBuffer(pPsPerAction, constantsSize);
	}

	RdrConstantBufferHandle createCubemapCaptureConstants(RdrResourceCommandList& rResCommandList, const Vec3& position, const float nearDist, const float farDist)
	{
		Camera cam;
		Matrix44 mtxView, mtxProj;

		uint constantsSize = sizeof(GsCubemapPerAction);
		GsCubemapPerAction* pGsConstants = (GsCubemapPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);
		for (uint f = 0; f < (uint)CubemapFace::Count; ++f)
		{
			cam.SetAsCubemapFace(position, (CubemapFace)f, nearDist, farDist);
			cam.GetMatrices(mtxView, mtxProj);
			pGsConstants->mtxViewProj[f] = Matrix44Multiply(mtxView, mtxProj);
			pGsConstants->mtxViewProj[f] = Matrix44Transpose(pGsConstants->mtxViewProj[f]);
		}

		return rResCommandList.CreateTempConstantBuffer(pGsConstants, constantsSize);
	}
}

void RdrAction::InitSharedData(RdrContext* pContext, const InputManager* pInputManager)
{
	s_actionSharedData.postProcess.Init(pContext, pInputManager);

	// Create instance ids buffers
	s_actionSharedData.currentInstanceIds = 0;
	for (int i = 0; i < ARRAY_SIZE(s_actionSharedData.instanceIds); ++i)
	{
		size_t temp;
		void* pAlignedData;

		uint dataSize = kMaxInstancesPerDraw * sizeof(uint) + 16;
		s_actionSharedData.instanceIds[i].pData = new char[dataSize];
		pAlignedData = (void*)s_actionSharedData.instanceIds[i].pData;
		std::align(16, dataSize, pAlignedData, temp);

		s_actionSharedData.instanceIds[i].ids = (uint*)pAlignedData;
		s_actionSharedData.instanceIds[i].buffer.size = kMaxInstancesPerDraw * sizeof(uint);
		s_actionSharedData.instanceIds[i].buffer.bufferObj = pContext->CreateConstantBuffer(nullptr, s_actionSharedData.instanceIds[i].buffer.size, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}
}

RdrAction* RdrAction::CreatePrimary(Camera& rCamera)
{
	RdrAction* pAction = s_actionSharedData.actions.allocSafe();
	pAction->InitAsPrimary(rCamera);
	return pAction;
}

RdrAction* RdrAction::CreateOffscreen(const wchar_t* actionName, Camera& rCamera,
	bool enablePostprocessing, const Rect& viewport, RdrRenderTargetViewHandle hOutputTarget)
{
	RdrAction* pAction = s_actionSharedData.actions.allocSafe();
	pAction->InitAsOffscreen(actionName, rCamera, enablePostprocessing, viewport, hOutputTarget);
	return pAction;
}

void RdrAction::InitAsPrimary(Camera& rCamera)
{
	Rect viewport = Rect(0.f, 0.f, (float)g_pRenderer->GetViewportWidth(), (float)g_pRenderer->GetViewportHeight());
	InitCommon(L"Primary Action", true, viewport, true, RdrResourceSystem::kPrimaryRenderTargetHandle);

	rCamera.SetAspectRatio(viewport.width / viewport.height);
	rCamera.UpdateFrustum();
	m_camera = rCamera;

	m_passes[(int)RdrPass::ZPrepass].bEnabled = true;
	m_passes[(int)RdrPass::LightCulling].bEnabled = true;
	m_passes[(int)RdrPass::Opaque].bEnabled = true;
	m_passes[(int)RdrPass::Sky].bEnabled = true;
	m_passes[(int)RdrPass::Alpha].bEnabled = true;
	m_passes[(int)RdrPass::UI].bEnabled = true;
	m_passes[(int)RdrPass::Wireframe].bEnabled = true;
	m_passes[(int)RdrPass::Editor].bEnabled = true;

	createPerActionConstants(m_resourceCommands, m_camera, m_primaryViewport, m_constants);
	createUiConstants(m_resourceCommands, m_primaryViewport, m_uiConstants);

	switch (g_debugState.visMode)
	{
	case DebugVisMode::kSsao:
		m_hDebugCopyTexture = s_actionSharedData.postProcess.GetSsaoBlurredBuffer();
		break;
	default:
		m_hDebugCopyTexture = 0;
		break;
	}
}

void RdrAction::InitAsOffscreen(const wchar_t* actionName, Camera& rCamera,
	bool enablePostprocessing, const Rect& viewport, RdrRenderTargetViewHandle hOutputTarget)
{
	InitCommon(actionName, false, viewport, enablePostprocessing, hOutputTarget);

	rCamera.SetAspectRatio(viewport.width / viewport.height);
	rCamera.UpdateFrustum();
	m_camera = rCamera;

	m_passes[(int)RdrPass::ZPrepass].bEnabled = true;
	m_passes[(int)RdrPass::LightCulling].bEnabled = true;
	m_passes[(int)RdrPass::Opaque].bEnabled = true;
	m_passes[(int)RdrPass::Sky].bEnabled = true;
	m_passes[(int)RdrPass::Alpha].bEnabled = true;

	createPerActionConstants(m_resourceCommands, m_camera, m_primaryViewport, m_constants);
}

void RdrAction::InitCommon(const wchar_t* actionName, bool isPrimaryAction, const Rect& viewport, bool enablePostProcessing, RdrRenderTargetViewHandle hOutputTarget)
{
	// Setup surfaces/render targets
	m_surfaces = *createActionSurfaces(m_resourceCommands, isPrimaryAction, (uint)viewport.width, (uint)viewport.height, g_debugState.msaaLevel);

	RdrRenderTargetViewHandle hColorTarget = enablePostProcessing ? m_surfaces.colorBuffer.hRenderTarget : hOutputTarget;
	RdrRenderTargetViewHandle hAlbedoTarget = m_surfaces.albedoBuffer.hRenderTarget;
	RdrRenderTargetViewHandle hNormalTarget = m_surfaces.normalBuffer.hRenderTarget;
	RdrDepthStencilViewHandle hDepthTarget = m_surfaces.hDepthStencilView;

	// Setup default action and pass states
	m_hOutputTarget = hOutputTarget;
	m_name = actionName;
	m_primaryViewport = viewport;
	m_bEnablePostProcessing = enablePostProcessing;

	// Z Prepass
	RdrPassData* pPass = &m_passes[(int)RdrPass::ZPrepass];
	{
		pPass->viewport = viewport;
		pPass->blendMode = RdrBlendMode::kOpaque;
		pPass->hDepthTarget = hDepthTarget;
		pPass->bClearDepthTarget = true;
		pPass->depthTestMode = RdrDepthTestMode::Less;
		pPass->bDepthWriteEnabled = true;
		pPass->shaderMode = RdrShaderMode::DepthOnly;
	}

	// Light Culling
	pPass = &m_passes[(int)RdrPass::LightCulling];
	{
		pPass->viewport = viewport;
		pPass->blendMode = RdrBlendMode::kOpaque;
		pPass->bClearDepthTarget = false;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Volumetric Fog
	pPass = &m_passes[(int)RdrPass::VolumetricFog];
	{
		pPass->viewport = viewport;
		pPass->blendMode = RdrBlendMode::kOpaque;
		pPass->bClearDepthTarget = false;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Opaque
	pPass = &m_passes[(int)RdrPass::Opaque];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hColorTarget;
		pPass->ahRenderTargets[1] = hAlbedoTarget;
		pPass->ahRenderTargets[2] = hNormalTarget;
		pPass->bClearRenderTargets = true;
		pPass->blendMode = RdrBlendMode::kOpaque;
		pPass->hDepthTarget = hDepthTarget;
		pPass->depthTestMode = RdrDepthTestMode::Equal;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Sky
	pPass = &m_passes[(int)RdrPass::Sky];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hColorTarget;
		pPass->hDepthTarget = hDepthTarget;
		pPass->depthTestMode = RdrDepthTestMode::Equal;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Alpha
	pPass = &m_passes[(int)RdrPass::Alpha];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hColorTarget;
		pPass->blendMode = RdrBlendMode::kAlpha;
		pPass->hDepthTarget = hDepthTarget;
		pPass->depthTestMode = RdrDepthTestMode::Less;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Editor
	pPass = &m_passes[(int)RdrPass::Editor];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hOutputTarget;
		pPass->hDepthTarget = hDepthTarget;
		pPass->blendMode = RdrBlendMode::kAlpha;
		pPass->depthTestMode = RdrDepthTestMode::Less;
		pPass->bDepthWriteEnabled = true;
		pPass->bClearDepthTarget = true;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Wireframe
	pPass = &m_passes[(int)RdrPass::Wireframe];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hOutputTarget;
		pPass->hDepthTarget = hDepthTarget;
		pPass->blendMode = RdrBlendMode::kOpaque;
		pPass->depthTestMode = RdrDepthTestMode::Less;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
		pPass->pOverridePixelShader = RdrShaderSystem::GetWireframePixelShader();
	}

	// UI
	pPass = &m_passes[(int)RdrPass::UI];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hOutputTarget;
		pPass->blendMode = RdrBlendMode::kAlpha;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}
}

void RdrAction::Release()
{
	for (RdrDrawOpBucket& rBucket : m_drawOpBuckets)
	{
		rBucket.clear();
	}

	for (RdrComputeOpBucket& rBucket : m_computeOpBuckets)
	{
		rBucket.clear();
	}

	m_lights.Clear();
	memset(&m_lightResources, 0, sizeof(m_lightResources));
	memset(m_shadowPasses, 0, sizeof(m_shadowPasses));

	for (int i = 0; i < (int)RdrPass::Count; ++i)
	{
		RdrPassData& rPass = m_passes[i];
		memset(rPass.ahRenderTargets, 0, sizeof(rPass.ahRenderTargets));
		rPass.hDepthTarget = 0;
		rPass.viewport = Rect(0, 0, 0, 0);
		rPass.shaderMode = RdrShaderMode::Normal;
		rPass.depthTestMode = RdrDepthTestMode::None;
		rPass.blendMode = RdrBlendMode::kOpaque;
		rPass.bEnabled = false;
		rPass.bClearRenderTargets = false;
		rPass.bClearDepthTarget = false;
		rPass.bIsCubeMapCapture = false;
	}

	m_shadowPassCount = 0;

	// Release back to the free list.
	s_actionSharedData.actions.releaseSafe(this);
}

RdrResourceCommandList& RdrAction::GetResCommandList()
{
	return m_resourceCommands;
}

void RdrAction::QueueSkyAndLighting(const AssetLib::SkySettings& rSkySettings, RdrResourceHandle hEnvironmentMapTexArray, float sceneDepthMin, float sceneDepthMax)
{
	m_sky = rSkySettings;
	m_passes[(int)RdrPass::VolumetricFog].bEnabled = rSkySettings.volumetricFog.enabled;

	// There are some unfortunate circular dependencies here:
	//	* Volumetric fog depends on transmittance LUT and atmosphere constants from the sky.
	//	* Sky dome models depend on volumetric fog LUT.
	// So, the sky is queued prior to lighting (which includes volumetric fog), and then we send the fog LUT back to the sky.
	// TODO: This isn't great... Sky dome models could instead use global resources like other models.
	RdrSky& rSkyRenderer = s_actionSharedData.sky;
	s_actionSharedData.sky.QueueDraw(this, rSkySettings, &m_lights);
	m_lightResources.hSkyTransmittanceLut = rSkyRenderer.GetTransmittanceLut();
	m_constants.hPsAtmosphere = rSkyRenderer.GetAtmosphereConstantBuffer();

	// Updating lighting.
	s_actionSharedData.lighting.QueueDraw(this, &m_lights, g_pRenderer->GetLightingMethod(), m_sky.volumetricFog, sceneDepthMin, sceneDepthMax, &m_lightResources);
	m_lightResources.hEnvironmentMapTexArray = hEnvironmentMapTexArray;

	// Re-assign sky material's volumetric fog LUT.
	s_actionSharedData.sky.AssignExternalResources(m_lightResources.hVolumetricFogLut);
}

void RdrAction::SetPostProcessingEffects(const AssetLib::PostProcessEffects& postProcFx)
{
	m_postProcessEffects = postProcFx;
}

void RdrAction::SortDrawOps(RdrBucketType eBucketType)
{
	RdrDrawOpBucket& rBucket = m_drawOpBuckets[(int)eBucketType];
	std::sort(rBucket.begin(), rBucket.end(), RdrDrawBucketEntry::SortCompare);
}

void RdrAction::QueueShadowMapPass(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport)
{
	assert(m_shadowPassCount + 1 < MAX_SHADOW_MAPS_PER_FRAME);

	int shadowPassIndex = m_shadowPassCount;
	RdrShadowPass& rShadowPass = m_shadowPasses[shadowPassIndex];
	m_shadowPassCount++;

	rShadowPass.camera = rCamera;
	rShadowPass.camera.UpdateFrustum();

	// Setup action passes
	RdrPassData& rPassData = rShadowPass.passData;
	{
		rPassData.viewport = viewport;
		rPassData.bEnabled = true;
		rPassData.hDepthTarget = hDepthView;
		rPassData.bClearDepthTarget = true;
		rPassData.depthTestMode = RdrDepthTestMode::Less;
		rPassData.bDepthWriteEnabled = true;
		rPassData.blendMode = RdrBlendMode::kOpaque;
		rPassData.shaderMode = RdrShaderMode::DepthOnly;
	}

	createPerActionConstants(m_resourceCommands, rCamera, viewport, rShadowPass.constants);
}

void RdrAction::QueueShadowCubeMapPass(const PointLight& rLight, RdrDepthStencilViewHandle hDepthView, Rect& viewport)
{
	assert(m_shadowPassCount + 1 < MAX_SHADOW_MAPS_PER_FRAME);

	int shadowPassIndex = m_shadowPassCount;
	RdrShadowPass& rShadowPass = m_shadowPasses[shadowPassIndex];
	m_shadowPassCount++;

	float viewDist = rLight.radius * 2.f;
	rShadowPass.camera.SetAsSphere(rLight.position, 0.1f, viewDist);
	rShadowPass.camera.UpdateFrustum();

	// Setup action passes
	RdrPassData& rPassData = rShadowPass.passData;
	{
		rPassData.viewport = viewport;
		rPassData.bEnabled = true;
		rPassData.blendMode = RdrBlendMode::kOpaque;
		rPassData.shaderMode = RdrShaderMode::DepthOnly;
		rPassData.depthTestMode = RdrDepthTestMode::Less;
		rPassData.bDepthWriteEnabled = true;
		rPassData.bClearDepthTarget = true;
		rPassData.hDepthTarget = hDepthView;
		rPassData.bIsCubeMapCapture = true;
	}

	createPerActionConstants(m_resourceCommands, rShadowPass.camera, viewport, rShadowPass.constants);
	rShadowPass.constants.hGsCubeMap = createCubemapCaptureConstants(m_resourceCommands, rLight.position, 0.1f, rLight.radius * 2.f);
}

//////////////////////////////////////////////////////////////////////////
// Drawing
//////////////////////////////////////////////////////////////////////////
void RdrAction::DrawIdle(RdrContext* pContext)
{
	m_resourceCommands.ProcessCommands(pContext);
}

void RdrAction::Draw(RdrContext* pContext, RdrDrawState* pDrawState, RdrProfiler* pProfiler)
{
	// Cache render objects
	m_pContext = pContext;
	m_pDrawState = pDrawState;
	m_pProfiler = pProfiler;

	m_pContext->BeginEvent(m_name);

	// Process resource system commands for this action
	m_resourceCommands.ProcessCommands(m_pContext);

	//////////////////////////////////////////////////////////////////////////
	// Shadow passes

	// Setup shadow raster state with depth bias.
	RdrRasterState rasterState;
	rasterState.bEnableMSAA = false;
	rasterState.bEnableScissor = false;
	rasterState.bWireframe = false;
	rasterState.bUseSlopeScaledDepthBias = 1;
	m_pContext->SetRasterState(rasterState);

	m_pProfiler->BeginSection(RdrProfileSection::Shadows);
	for (int iShadow = 0; iShadow < m_shadowPassCount; ++iShadow)
	{
		DrawShadowPass(iShadow);
	}
	m_pProfiler->EndSection();

	//////////////////////////////////////////////////////////////////////////
	// Normal draw passes
	rasterState.bEnableMSAA = (g_debugState.msaaLevel > 1);
	rasterState.bEnableScissor = false;
	rasterState.bWireframe = g_debugState.wireframe;
	rasterState.bUseSlopeScaledDepthBias = 0;
	m_pContext->SetRasterState(rasterState);

	DrawPass(RdrPass::ZPrepass);
	DrawPass(RdrPass::LightCulling);
	DrawPass(RdrPass::VolumetricFog);
	DrawPass(RdrPass::Opaque);
	DrawPass(RdrPass::Sky);
	DrawPass(RdrPass::Alpha);

	if (g_debugState.wireframe)
	{
		rasterState.bWireframe = false;
		m_pContext->SetRasterState(rasterState);
	}

	// Resolve multi-sampled color buffer.
	if (g_debugState.msaaLevel > 1)
	{
		const RdrResource* pColorBufferMultisampled = RdrResourceSystem::GetResource(m_surfaces.colorBuffer.hTextureMultisampled);
		const RdrResource* pColorBuffer = RdrResourceSystem::GetResource(m_surfaces.colorBuffer.hTexture);
		m_pContext->ResolveResource(*pColorBufferMultisampled, *pColorBuffer);

		rasterState.bEnableMSAA = false;
		m_pContext->SetRasterState(rasterState);
	}

	if (m_bEnablePostProcessing)
	{
		m_pProfiler->BeginSection(RdrProfileSection::PostProcessing);
		s_actionSharedData.postProcess.DoPostProcessing(m_pContext, m_pDrawState, m_surfaces, m_postProcessEffects, m_constants);
		m_pProfiler->EndSection();
	}

	// Wireframe
	{
		rasterState.bWireframe = true;
		m_pContext->SetRasterState(rasterState);

		DrawPass(RdrPass::Wireframe);

		rasterState.bWireframe = false;
		m_pContext->SetRasterState(rasterState);
	}

	DrawPass(RdrPass::Editor);
	DrawPass(RdrPass::UI);

	if (m_hDebugCopyTexture)
	{
		s_actionSharedData.postProcess.CopyToTarget(m_pContext, m_pDrawState, m_hDebugCopyTexture, m_hOutputTarget);
	}

	m_pContext->EndEvent();

	// Reset render objects
	m_pContext = nullptr;
	m_pDrawState = nullptr;
	m_pProfiler = nullptr;
}

void RdrAction::DrawBucket(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants, const RdrLightResources& rLightParams)
{
	if (g_debugState.enableInstancing & 1)
	{
		const RdrDrawBucketEntry* pPendingEntry = nullptr;
		uint instanceCount = 0;
		uint* pCurrInstanceIds = nullptr;

		for (const RdrDrawBucketEntry& rEntry : rBucket)
		{
			if (pPendingEntry)
			{
				if (pPendingEntry->pDrawOp->instanceDataId != 0 && pPendingEntry->sortKey == rEntry.sortKey && instanceCount < kMaxInstancesPerDraw)
				{
					pCurrInstanceIds[instanceCount] = rEntry.pDrawOp->instanceDataId;
					++instanceCount;
				}
				else
				{
					// Draw the pending entry
					DrawGeo(rPass, rGlobalConstants, pPendingEntry->pDrawOp, rLightParams, instanceCount);
					pPendingEntry = nullptr;
				}
			}

			// Update pending entry if it was not set or we just drew.
			if (!pPendingEntry)
			{
				s_actionSharedData.currentInstanceIds++;
				if (s_actionSharedData.currentInstanceIds >= 8)
					s_actionSharedData.currentInstanceIds = 0;

				pCurrInstanceIds = s_actionSharedData.instanceIds[s_actionSharedData.currentInstanceIds].ids;
				pCurrInstanceIds[0] = rEntry.pDrawOp->instanceDataId;

				pPendingEntry = &rEntry;
				instanceCount = 1;
			}
		}

		if (pPendingEntry)
		{
			DrawGeo(rPass, rGlobalConstants, pPendingEntry->pDrawOp, rLightParams, instanceCount);
		}
	}
	else
	{
		for (const RdrDrawBucketEntry& rEntry : rBucket)
		{
			DrawGeo(rPass, rGlobalConstants, rEntry.pDrawOp, rLightParams, 1);
		}
	}
}

void RdrAction::DrawGeo(const RdrPassData& rPass, const RdrGlobalConstants& rGlobalConstants,
	const RdrDrawOp* pDrawOp, const RdrLightResources& rLightParams, uint instanceCount)
{
	bool bDepthOnly = (rPass.shaderMode == RdrShaderMode::DepthOnly);
	const RdrGeometry* pGeo = RdrResourceSystem::GetGeo(pDrawOp->hGeo);

	// Vertex & tessellation shaders
	RdrVertexShader vertexShader = pDrawOp->vertexShader;
	if (bDepthOnly)
	{
		vertexShader.flags |= RdrShaderFlags::DepthOnly;
	}

	bool instanced = false;
	if (instanceCount > 1)
	{
		vertexShader.flags |= RdrShaderFlags::IsInstanced;
		instanced = true;
	}

	m_pDrawState->pVertexShader = RdrShaderSystem::GetVertexShader(vertexShader);

	RdrConstantBufferHandle hPerActionVs = rGlobalConstants.hVsPerAction;
	m_pDrawState->vsConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(hPerActionVs)->bufferObj;
	m_pDrawState->vsConstantBufferCount = 1;

	m_pDrawState->dsConstantBuffers[0] = m_pDrawState->vsConstantBuffers[0];
	m_pDrawState->dsConstantBufferCount = 1;

	if (instanced)
	{
		const RdrConstantBuffer& rBuffer = s_actionSharedData.instanceIds[s_actionSharedData.currentInstanceIds].buffer;
		m_pContext->UpdateConstantBuffer(rBuffer.bufferObj, s_actionSharedData.instanceIds[s_actionSharedData.currentInstanceIds].ids, rBuffer.size);
		m_pDrawState->vsConstantBuffers[1] = rBuffer.bufferObj;
		m_pDrawState->vsConstantBufferCount = 2;

		m_pDrawState->vsResources[0] = RdrResourceSystem::GetResource(RdrInstancedObjectDataBuffer::GetResourceHandle())->resourceView;
		m_pDrawState->vsResourceCount = 1;
	}
	else if (pDrawOp->hVsConstants)
	{
		m_pDrawState->vsConstantBuffers[1] = RdrResourceSystem::GetConstantBuffer(pDrawOp->hVsConstants)->bufferObj;
		m_pDrawState->vsConstantBufferCount = 2;
	}

	// Geom shader
	if (rPass.bIsCubeMapCapture)
	{
		RdrGeometryShader geomShader = { RdrGeometryShaderType::Model_CubemapCapture, vertexShader.flags };
		m_pDrawState->pGeometryShader = RdrShaderSystem::GetGeometryShader(geomShader);
		m_pDrawState->gsConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hGsCubeMap)->bufferObj;
		m_pDrawState->gsConstantBufferCount = 1;
	}

	// Tessellation material
	if (pDrawOp->pTessellationMaterial)
	{
		const RdrTessellationMaterial* pTessMaterial = pDrawOp->pTessellationMaterial;
		RdrTessellationShader tessShader = pTessMaterial->shader;
		if (bDepthOnly)
		{
			tessShader.flags |= RdrShaderFlags::DepthOnly;
		}
		m_pDrawState->pHullShader = RdrShaderSystem::GetHullShader(tessShader);
		m_pDrawState->pDomainShader = RdrShaderSystem::GetDomainShader(tessShader);

		m_pDrawState->dsResourceCount = pTessMaterial->ahResources.size();
		for (uint i = 0; i < m_pDrawState->dsResourceCount; ++i)
		{
			m_pDrawState->dsResources[i] = RdrResourceSystem::GetResource(pTessMaterial->ahResources.get(i))->resourceView;
		}

		m_pDrawState->dsSamplerCount = pTessMaterial->aSamplers.size();
		for (uint i = 0; i < m_pDrawState->dsSamplerCount; ++i)
		{
			m_pDrawState->dsSamplers[i] = pTessMaterial->aSamplers.get(i);
		}

		if (pTessMaterial->hDsConstants)
		{
			m_pDrawState->dsConstantBuffers[1] = RdrResourceSystem::GetConstantBuffer(pTessMaterial->hDsConstants)->bufferObj;
			m_pDrawState->dsConstantBufferCount = 2;
		}
	}

	// Pixel shader
	const RdrMaterial* pMaterial = pDrawOp->pMaterial;
	if (pMaterial)
	{
		if (rPass.pOverridePixelShader)
		{
			m_pDrawState->pPixelShader = rPass.pOverridePixelShader;
		}
		else
		{
			m_pDrawState->pPixelShader = pMaterial->hPixelShaders[(int)rPass.shaderMode] ?
				RdrShaderSystem::GetPixelShader(pMaterial->hPixelShaders[(int)rPass.shaderMode]) :
				nullptr;
		}

		if (m_pDrawState->pPixelShader)
		{
			m_pDrawState->psResourceCount = pMaterial->ahTextures.size();
			for (uint i = 0; i < m_pDrawState->psResourceCount; ++i)
			{
				m_pDrawState->psResources[i] = RdrResourceSystem::GetResource(pMaterial->ahTextures.get(i))->resourceView;
			}

			m_pDrawState->psSamplerCount = pMaterial->aSamplers.size();
			for (uint i = 0; i < m_pDrawState->psSamplerCount; ++i)
			{
				m_pDrawState->psSamplers[i] = pMaterial->aSamplers.get(i);
			}

			m_pDrawState->psConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hPsPerAction)->bufferObj;
			m_pDrawState->psConstantBufferCount = 1;

			if (pMaterial->hConstants)
			{
				m_pDrawState->psConstantBuffers[1] = RdrResourceSystem::GetConstantBuffer(pMaterial->hConstants)->bufferObj;
				m_pDrawState->psConstantBufferCount = 2;
			}

			if (pMaterial->bNeedsLighting && !bDepthOnly)
			{
				m_pDrawState->psConstantBuffers[2] = RdrResourceSystem::GetConstantBuffer(rLightParams.hGlobalLightsCb)->bufferObj;
				m_pDrawState->psConstantBuffers[3] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hPsAtmosphere)->bufferObj;
				m_pDrawState->psConstantBufferCount = 4;

				m_pDrawState->psResources[(int)RdrPsResourceSlots::SpotLightList] = RdrResourceSystem::GetResource(rLightParams.hSpotLightListRes)->resourceView;
				m_pDrawState->psResources[(int)RdrPsResourceSlots::PointLightList] = RdrResourceSystem::GetResource(rLightParams.hPointLightListRes)->resourceView;

				m_pDrawState->psResources[(int)RdrPsResourceSlots::EnvironmentMaps] = RdrResourceSystem::GetResource(rLightParams.hEnvironmentMapTexArray)->resourceView;
				m_pDrawState->psResources[(int)RdrPsResourceSlots::VolumetricFogLut] = RdrResourceSystem::GetResource(rLightParams.hVolumetricFogLut)->resourceView;
				m_pDrawState->psResources[(int)RdrPsResourceSlots::SkyTransmittance] = RdrResourceSystem::GetResource(rLightParams.hSkyTransmittanceLut)->resourceView;
				m_pDrawState->psResources[(int)RdrPsResourceSlots::LightIds] = RdrResourceSystem::GetResource(rLightParams.hLightIndicesRes)->resourceView;
				m_pDrawState->psResources[(int)RdrPsResourceSlots::ShadowMaps] = RdrResourceSystem::GetResource(rLightParams.hShadowMapTexArray)->resourceView;
				m_pDrawState->psResources[(int)RdrPsResourceSlots::ShadowCubeMaps] = RdrResourceSystem::GetResource(rLightParams.hShadowCubeMapTexArray)->resourceView;
				m_pDrawState->psResourceCount = max((uint)RdrPsResourceSlots::LightIds + 1, m_pDrawState->psResourceCount);

				m_pDrawState->psSamplers[(int)RdrPsSamplerSlots::Clamp] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
				m_pDrawState->psSamplers[(int)RdrPsSamplerSlots::ShadowMap] = RdrSamplerState(RdrComparisonFunc::LessEqual, RdrTexCoordMode::Clamp, false);
				m_pDrawState->psSamplerCount = max((uint)RdrPsSamplerSlots::ShadowMap + 1, m_pDrawState->psSamplerCount);
			}
		}
	}

	// Input assembly
	m_pDrawState->inputLayout = *RdrShaderSystem::GetInputLayout(pDrawOp->hInputLayout); // todo: layouts per flags
	m_pDrawState->eTopology = pGeo->geoInfo.eTopology;

	m_pDrawState->vertexBuffers[0] = pGeo->vertexBuffer;
	m_pDrawState->vertexStrides[0] = pGeo->geoInfo.vertStride;
	m_pDrawState->vertexOffsets[0] = 0;
	m_pDrawState->vertexBufferCount = 1;
	m_pDrawState->vertexCount = pGeo->geoInfo.numVerts;

	if (pDrawOp->hCustomInstanceBuffer)
	{
		const RdrResource* pInstanceData = RdrResourceSystem::GetResource(pDrawOp->hCustomInstanceBuffer);
		m_pDrawState->vertexBuffers[1].pBuffer = pInstanceData->pBuffer;
		m_pDrawState->vertexStrides[1] = pInstanceData->bufferInfo.elementSize;
		m_pDrawState->vertexOffsets[1] = 0;
		m_pDrawState->vertexBufferCount = 2;

		assert(instanceCount == 1);
		instanceCount = pDrawOp->instanceCount;
	}

	if (pGeo->indexBuffer.pBuffer)
	{
		m_pDrawState->indexBuffer = pGeo->indexBuffer;
		m_pDrawState->indexCount = pGeo->geoInfo.numIndices;
		m_pProfiler->AddCounter(RdrProfileCounter::Triangles, instanceCount * m_pDrawState->indexCount / 3);
	}
	else
	{
		m_pDrawState->indexBuffer.pBuffer = nullptr;
		m_pProfiler->AddCounter(RdrProfileCounter::Triangles, instanceCount * m_pDrawState->vertexCount / 3);
	}

	// Done
	m_pContext->Draw(*m_pDrawState, instanceCount);
	m_pProfiler->IncrementCounter(RdrProfileCounter::DrawCall);

	m_pDrawState->Reset();
}

void RdrAction::DispatchCompute(const RdrComputeOp* pComputeOp)
{
	m_pDrawState->pComputeShader = RdrShaderSystem::GetComputeShader(pComputeOp->shader);

	m_pDrawState->csConstantBufferCount = pComputeOp->ahConstantBuffers.size();
	for (uint i = 0; i < m_pDrawState->csConstantBufferCount; ++i)
	{
		m_pDrawState->csConstantBuffers[i] = RdrResourceSystem::GetConstantBuffer(pComputeOp->ahConstantBuffers.get(i))->bufferObj;
	}

	uint count = pComputeOp->ahResources.size();
	for (uint i = 0; i < count; ++i)
	{
		if (pComputeOp->ahResources.get(i))
			m_pDrawState->csResources[i] = RdrResourceSystem::GetResource(pComputeOp->ahResources.get(i))->resourceView;
		else
			m_pDrawState->csResources[i].pTypeless = nullptr;
	}

	count = pComputeOp->aSamplers.size();
	for (uint i = 0; i < count; ++i)
	{
		m_pDrawState->csSamplers[i] = pComputeOp->aSamplers.get(i);
	}

	count = pComputeOp->ahWritableResources.size();
	for (uint i = 0; i < count; ++i)
	{
		if (pComputeOp->ahWritableResources.get(i))
			m_pDrawState->csUavs[i] = RdrResourceSystem::GetResource(pComputeOp->ahWritableResources.get(i))->uav;
		else
			m_pDrawState->csUavs[i].pTypeless = nullptr;
	}

	m_pContext->DispatchCompute(*m_pDrawState, pComputeOp->threads[0], pComputeOp->threads[1], pComputeOp->threads[2]);

	m_pDrawState->Reset();
}


void RdrAction::DrawPass(RdrPass ePass)
{
	const RdrPassData& rPass = m_passes[(int)ePass];

	if (!rPass.bEnabled)
		return;

	m_pProfiler->BeginSection(s_passProfileSections[(int)ePass]);
	m_pContext->BeginEvent(s_passNames[(int)ePass]);

	RdrRenderTargetView renderTargets[MAX_RENDER_TARGETS];
	for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
	{
		if (rPass.ahRenderTargets[i])
		{
			renderTargets[i] = RdrResourceSystem::GetRenderTargetView(rPass.ahRenderTargets[i]);
		}
		else
		{
			renderTargets[i].pView = nullptr;
		}
	}

	RdrDepthStencilView depthView = { 0 };
	if (rPass.hDepthTarget)
	{
		depthView = RdrResourceSystem::GetDepthStencilView(rPass.hDepthTarget);
	}

	if (rPass.bClearRenderTargets)
	{
		for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
		{
			if (renderTargets[i].pView)
			{
				const Color clearColor(0.f, 0.f, 0.f, 0.f);
				m_pContext->ClearRenderTargetView(renderTargets[i], clearColor);
			}
		}
	}

	if (rPass.bClearDepthTarget)
	{
		m_pContext->ClearDepthStencilView(depthView, true, 1.f, true, 0);
	}

	// Clear resource bindings to avoid input/output binding errors
	m_pContext->PSClearResources();

	m_pContext->SetRenderTargets(MAX_RENDER_TARGETS, renderTargets, depthView);
	m_pContext->SetDepthStencilState(rPass.depthTestMode, rPass.bDepthWriteEnabled);
	m_pContext->SetBlendState(rPass.blendMode);

	m_pContext->SetViewport(rPass.viewport);

	// Compute ops
	{
		const RdrComputeOpBucket& rComputeBucket = GetComputeOpBucket(ePass);
		for (const RdrComputeOp* pComputeOp : rComputeBucket)
		{
			DispatchCompute(pComputeOp);
		}
	}

	// Draw ops
	const RdrDrawOpBucket& rBucket = GetDrawOpBucket(s_passBuckets[(int)ePass]);
	const RdrGlobalConstants& rGlobalConstants = (ePass == RdrPass::UI) ? m_uiConstants : m_constants;
	DrawBucket(rPass, rBucket, rGlobalConstants, m_lightResources);

	m_pContext->EndEvent();
	m_pProfiler->EndSection();
}

void RdrAction::DrawShadowPass(int shadowPassIndex)
{
	const RdrShadowPass& rShadowPass = m_shadowPasses[shadowPassIndex];
	const RdrPassData& rPassData = rShadowPass.passData;
	m_pContext->BeginEvent(rPassData.bIsCubeMapCapture ? L"Shadow Cube Map" : L"Shadow Map");

	RdrRenderTargetView renderTargets[MAX_RENDER_TARGETS];
	for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
	{
		if (rPassData.ahRenderTargets[i])
		{
			renderTargets[i] = RdrResourceSystem::GetRenderTargetView(rPassData.ahRenderTargets[i]);
		}
		else
		{
			renderTargets[i].pView = nullptr;
		}
	}

	RdrDepthStencilView depthView = { 0 };
	if (rPassData.hDepthTarget)
	{
		depthView = RdrResourceSystem::GetDepthStencilView(rPassData.hDepthTarget);
	}

	if (rPassData.bClearRenderTargets)
	{
		for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
		{
			if (renderTargets[i].pView)
			{
				const Color clearColor(0.f, 0.f, 0.f, 0.f);
				m_pContext->ClearRenderTargetView(renderTargets[i], clearColor);
			}
		}
	}

	if (rPassData.bClearDepthTarget)
	{
		m_pContext->ClearDepthStencilView(depthView, true, 1.f, true, 0);
	}

	// Clear resource bindings to avoid input/output binding errors
	m_pContext->PSClearResources();

	m_pContext->SetRenderTargets(MAX_RENDER_TARGETS, renderTargets, depthView);
	m_pContext->SetDepthStencilState(rPassData.depthTestMode, rPassData.bDepthWriteEnabled);
	m_pContext->SetBlendState(rPassData.blendMode);

	m_pContext->SetViewport(rPassData.viewport);

	RdrLightResources lightParams;
	const RdrDrawOpBucket& rDrawBucket = GetDrawOpBucket((RdrBucketType)((int)RdrBucketType::ShadowMap0 + shadowPassIndex));
	DrawBucket(rPassData, rDrawBucket, rShadowPass.constants, lightParams);

	m_pContext->EndEvent();
}