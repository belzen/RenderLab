#include "Precompiled.h"
#include "RdrAction.h"
#include "Renderer.h"
#include "RdrFrameMem.h"
#include "Scene.h"
#include "components/SkyVolume.h"

namespace
{
	struct  
	{
		FreeList<RdrAction, MAX_ACTIONS_PER_FRAME * 2> actions;
		RdrLighting lighting;
		RdrSky sky;

		RdrActionSurfaces primarySurfaces;
		std::vector<RdrActionSurfaces> offscreenSurfaces;
	} s_actionSharedData;

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
		m_hDebugCopyTexture = g_pRenderer->GetPostProcess().GetSsaoBlurredBuffer();
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
