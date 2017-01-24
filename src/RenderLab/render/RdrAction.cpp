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
	} s_actionSharedData;

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
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
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
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
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
	bool enablePostprocessing, const Rect& viewport, const RdrSurface& outputSurface)
{
	RdrAction* pAction = s_actionSharedData.actions.allocSafe();
	pAction->InitAsOffscreen(actionName, rCamera, enablePostprocessing, viewport, outputSurface);
	return pAction;
}

void RdrAction::InitAsPrimary(Camera& rCamera)
{
	Rect viewport = Rect(0.f, 0.f, (float)g_pRenderer->GetViewportWidth(), (float)g_pRenderer->GetViewportHeight());

	RdrSurface surface;
	surface.hRenderTarget = RdrResourceSystem::kPrimaryRenderTargetHandle;
	surface.hDepthTarget = g_pRenderer->GetPrimaryDepthStencilView();

	InitCommon(L"Primary Action", viewport, true, surface);

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
}

void RdrAction::InitAsOffscreen(const wchar_t* actionName, Camera& rCamera,
	bool enablePostprocessing, const Rect& viewport, const RdrSurface& outputSurface)
{
	InitCommon(actionName, viewport, enablePostprocessing, outputSurface);

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

void RdrAction::InitCommon(const wchar_t* actionName, const Rect& viewport, bool enablePostProcessing, const RdrSurface& outputSurface)
{
	RdrRenderTargetViewHandle hColorTarget = enablePostProcessing ? g_pRenderer->GetColorBufferRenderTarget() : outputSurface.hRenderTarget;
	RdrDepthStencilViewHandle hDepthTarget = outputSurface.hDepthTarget;

	// Setup default action and pass states
	m_name = actionName;
	m_primaryViewport = viewport;
	m_bEnablePostProcessing = enablePostProcessing;

	// Z Prepass
	RdrPassData* pPass = &m_passes[(int)RdrPass::ZPrepass];
	{
		pPass->viewport = viewport;
		pPass->bAlphaBlend = false;
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
		pPass->bAlphaBlend = false;
		pPass->bClearDepthTarget = false;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Volumetric Fog
	pPass = &m_passes[(int)RdrPass::VolumetricFog];
	{
		pPass->viewport = viewport;
		pPass->bAlphaBlend = false;
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
		pPass->bClearRenderTargets = true;
		pPass->bAlphaBlend = false;
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
		pPass->ahRenderTargets[0] = outputSurface.hRenderTarget;
		pPass->bAlphaBlend = true;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Wireframe
	pPass = &m_passes[(int)RdrPass::Wireframe];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = outputSurface.hRenderTarget;
		pPass->bAlphaBlend = true;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// UI
	pPass = &m_passes[(int)RdrPass::UI];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = outputSurface.hRenderTarget;
		pPass->bAlphaBlend = true;
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
		rPass.bAlphaBlend = false;
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
		rPassData.bAlphaBlend = false;
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
		rPassData.bAlphaBlend = false;
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
