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
			RdrResource buffer;
			char* pData; // Original non-aligned pointer.
			uint* ids;
		} instanceIds[8];
		uint currentInstanceIds;

	} s_actionSharedData;

	enum class RdrPsSamplerSlots
	{
		Clamp = 14,
		ShadowMap = 15,
	};

	// Pass to bucket mappings
	RdrBucketType s_passBuckets[] = {
		RdrBucketType::ZPrepass,	  // RdrPass::ZPrepass
		RdrBucketType::LightCulling,  // RdrPass::LightCulling
		RdrBucketType::VolumetricFog, // RdrPass::VolumetricFog
		RdrBucketType::Opaque,	      // RdrPass::Opaque
		RdrBucketType::Decal,	      // RdrPass::Decal
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
		L"Decal",			// RdrPass::Decal
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
		RdrProfileSection::Decal,			// RdrPass::Decal
		RdrProfileSection::Sky,				// RdrPass::Sky
		RdrProfileSection::Alpha,			// RdrPass::Alpha
		RdrProfileSection::Editor,			// RdrPass::Editor
		RdrProfileSection::Wireframe,		// RdrPass::Wireframe
		RdrProfileSection::UI,				// RdrPass::UI
	};
	static_assert(sizeof(s_passProfileSections) / sizeof(s_passProfileSections[0]) == (int)RdrPass::Count, "Missing RdrPass profile sections!");



	RdrActionSurfaces* createActionSurfaces(const RdrAction* pAction, bool isPrimaryAction, uint width, uint height, int msaaLevel)
	{
		RdrResourceCommandList& rResCommands = g_pRenderer->GetResourceCommandList();

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
				rResCommands.ReleaseDepthStencilView(pSurfaces->hDepthStencilView, CREATE_BACKPOINTER(pAction));
			if (pSurfaces->hDepthBuffer)
				rResCommands.ReleaseResource(pSurfaces->hDepthBuffer, CREATE_BACKPOINTER(pAction));

			rResCommands.ReleaseRenderTarget2d(pSurfaces->colorBuffer, CREATE_BACKPOINTER(pAction));
			rResCommands.ReleaseRenderTarget2d(pSurfaces->albedoBuffer, CREATE_BACKPOINTER(pAction));
			rResCommands.ReleaseRenderTarget2d(pSurfaces->normalBuffer, CREATE_BACKPOINTER(pAction));

			// Create resized buffers
			pSurfaces->colorBuffer = RdrResourceSystem::InitRenderTarget2d(width, height, RdrResourceFormat::R16G16B16A16_FLOAT, msaaLevel, CREATE_BACKPOINTER(pAction));
			pSurfaces->albedoBuffer = RdrResourceSystem::InitRenderTarget2d(width, height, RdrResourceFormat::B8G8R8A8_UNORM, msaaLevel, CREATE_BACKPOINTER(pAction));
			pSurfaces->normalBuffer = RdrResourceSystem::InitRenderTarget2d(width, height, RdrResourceFormat::B8G8R8A8_UNORM, msaaLevel, CREATE_BACKPOINTER(pAction));
			// Depth Buffer
			pSurfaces->hDepthBuffer = RdrResourceSystem::CreateTexture2DMS(width, height, kDefaultDepthFormat,
				g_debugState.msaaLevel, RdrResourceAccessFlags::CpuRO_GpuRO_RenderTarget, CREATE_BACKPOINTER(pAction));
			pSurfaces->hDepthStencilView = RdrResourceSystem::CreateDepthStencilView(pSurfaces->hDepthBuffer, CREATE_BACKPOINTER(pAction));

			if (isPrimaryAction)
			{
				// Post-proc resources just size to the largest required viewport and are re-used for each action.
				// TODO: This makes the assumption the primary action is always the largest which may not always be valid.
				s_actionSharedData.postProcess.ResizeResources(width, height);
			}
		}

		return pSurfaces;
	}


	void createPerActionConstants(const RdrAction* pAction, const Camera& rCamera, const Rect& rViewport, RdrGlobalConstants& rConstants)
	{
		Matrix44 mtxView;
		Matrix44 mtxProj;

		rCamera.GetMatrices(mtxView, mtxProj);

		// VS
		uint constantsSize = sizeof(VsPerAction);
		VsPerAction* pVsPerAction = (VsPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxView = mtxView;
		pVsPerAction->mtxView = Matrix44Transpose(pVsPerAction->mtxView);
		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);
		pVsPerAction->cameraPosition = rCamera.GetPosition();

		rConstants.hVsPerAction = RdrResourceSystem::CreateTempConstantBuffer(pVsPerAction, constantsSize, CREATE_BACKPOINTER(pAction));

		// PS
		constantsSize = sizeof(PsPerAction);
		PsPerAction* pPsPerAction = (PsPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);

		pPsPerAction->cameraPos = rCamera.GetPosition();
		pPsPerAction->cameraDir = rCamera.GetDirection();
		pPsPerAction->mtxView = mtxView;
		pPsPerAction->mtxView = Matrix44Transpose(pPsPerAction->mtxView);
		pPsPerAction->mtxInvView = Matrix44Inverse(mtxView);
		pPsPerAction->mtxInvView = Matrix44Transpose(pPsPerAction->mtxInvView);
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

		rConstants.hPsPerAction = RdrResourceSystem::CreateTempConstantBuffer(pPsPerAction, constantsSize, CREATE_BACKPOINTER(pAction));
	}

	void createUiConstants(const RdrAction* pAction, const Rect& rViewport, RdrGlobalConstants& rConstants)
	{
		Matrix44 mtxView = Matrix44::kIdentity;
		Matrix44 mtxProj = DirectX::XMMatrixOrthographicLH((float)rViewport.width, (float)rViewport.height, 0.01f, 1000.f);

		// VS
		uint constantsSize = sizeof(VsPerAction);
		VsPerAction* pVsPerAction = (VsPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxView = mtxView;
		pVsPerAction->mtxView = Matrix44Transpose(pVsPerAction->mtxView);
		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);
		pVsPerAction->cameraPosition = Vec3::kZero;

		rConstants.hVsPerAction = RdrResourceSystem::CreateTempConstantBuffer(pVsPerAction, constantsSize, CREATE_BACKPOINTER(pAction));

		// PS
		constantsSize = sizeof(PsPerAction);
		PsPerAction* pPsPerAction = (PsPerAction*)RdrFrameMem::AllocAligned(constantsSize, 16);

		pPsPerAction->cameraPos = Vec3::kOrigin;
		pPsPerAction->cameraDir = Vec3::kUnitZ;
		pPsPerAction->mtxView = mtxView;
		pPsPerAction->mtxView = Matrix44Transpose(pPsPerAction->mtxView);
		pPsPerAction->mtxInvView = Matrix44Inverse(mtxView);
		pPsPerAction->mtxInvView = Matrix44Transpose(pPsPerAction->mtxInvView);
		pPsPerAction->mtxProj = mtxProj;
		pPsPerAction->mtxProj = Matrix44Transpose(pPsPerAction->mtxProj);
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->mtxInvProj = Matrix44Transpose(pPsPerAction->mtxInvProj);
		pPsPerAction->viewSize.x = (uint)rViewport.width;
		pPsPerAction->viewSize.y = (uint)rViewport.height;

		rConstants.hPsPerAction = RdrResourceSystem::CreateTempConstantBuffer(pPsPerAction, constantsSize, CREATE_BACKPOINTER(pAction));
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
		s_actionSharedData.instanceIds[i].buffer.CreateConstantBuffer(*pContext, kMaxInstancesPerDraw * sizeof(uint), 
			RdrResourceAccessFlags::CpuRW_GpuRO, CREATE_NULL_BACKPOINTER);
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
	InitCommon(L"Primary Action", true, viewport, true, RdrGlobalRenderTargetHandles::kPrimary);

	rCamera.SetAspectRatio(viewport.width / viewport.height);
	rCamera.UpdateFrustum();
	m_camera = rCamera;

	m_passes[(int)RdrPass::ZPrepass].bEnabled = true;
	m_passes[(int)RdrPass::LightCulling].bEnabled = true;
	m_passes[(int)RdrPass::Opaque].bEnabled = true;
	m_passes[(int)RdrPass::Decal].bEnabled = true;
	m_passes[(int)RdrPass::Sky].bEnabled = true;
	m_passes[(int)RdrPass::Alpha].bEnabled = true;
	m_passes[(int)RdrPass::UI].bEnabled = true;
	m_passes[(int)RdrPass::Wireframe].bEnabled = true;
	m_passes[(int)RdrPass::Editor].bEnabled = true;

	createPerActionConstants(this, m_camera, m_primaryViewport, m_constants);
	createUiConstants(this, m_primaryViewport, m_uiConstants);

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
	m_passes[(int)RdrPass::Decal].bEnabled = true;
	m_passes[(int)RdrPass::Sky].bEnabled = true;
	m_passes[(int)RdrPass::Alpha].bEnabled = true;

	createPerActionConstants(this, m_camera, m_primaryViewport, m_constants);
}

void RdrAction::InitCommon(const wchar_t* actionName, bool isPrimaryAction, const Rect& viewport, bool enablePostProcessing, RdrRenderTargetViewHandle hOutputTarget)
{
	// Setup surfaces/render targets
	m_surfaces = *createActionSurfaces(this, isPrimaryAction, (uint)viewport.width, (uint)viewport.height, g_debugState.msaaLevel);

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
		pPass->hDepthTarget = hDepthTarget;
		pPass->bClearDepthTarget = true;
		pPass->shaderMode = RdrShaderMode::DepthOnly;
	}

	// Light Culling
	pPass = &m_passes[(int)RdrPass::LightCulling];
	{
		pPass->viewport = viewport;
		pPass->bClearDepthTarget = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Volumetric Fog
	pPass = &m_passes[(int)RdrPass::VolumetricFog];
	{
		pPass->viewport = viewport;
		pPass->bClearDepthTarget = false;
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
		pPass->hDepthTarget = hDepthTarget;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Decal
	pPass = &m_passes[(int)RdrPass::Decal];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hColorTarget;
		pPass->bClearRenderTargets = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Sky
	pPass = &m_passes[(int)RdrPass::Sky];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hColorTarget;
		pPass->hDepthTarget = hDepthTarget;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Alpha
	pPass = &m_passes[(int)RdrPass::Alpha];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hColorTarget;
		pPass->hDepthTarget = hDepthTarget;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Editor
	pPass = &m_passes[(int)RdrPass::Editor];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hOutputTarget;
		pPass->hDepthTarget = hDepthTarget;
		pPass->bClearDepthTarget = true;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Wireframe
	pPass = &m_passes[(int)RdrPass::Wireframe];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hOutputTarget;
		pPass->hDepthTarget = hDepthTarget;
		pPass->shaderMode = RdrShaderMode::Wireframe;
	}

	// UI
	pPass = &m_passes[(int)RdrPass::UI];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hOutputTarget;
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
	memset(&m_lightResources.sharedResources, 0, sizeof(m_lightResources.sharedResources));
	memset(m_shadowPasses, 0, sizeof(m_shadowPasses));

	for (int i = 0; i < (int)RdrPass::Count; ++i)
	{
		RdrPassData& rPass = m_passes[i];
		memset(rPass.ahRenderTargets, 0, sizeof(rPass.ahRenderTargets));
		rPass.hDepthTarget = 0;
		rPass.viewport = Rect(0, 0, 0, 0);
		rPass.shaderMode = RdrShaderMode::Normal;
		rPass.bEnabled = false;
		rPass.bClearRenderTargets = false;
		rPass.bClearDepthTarget = false;
	}

	m_shadowPassCount = 0;

	// Release back to the free list.
	s_actionSharedData.actions.releaseSafe(this);
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
	s_actionSharedData.sky.QueueDraw(this, rSkySettings, &m_lights, &m_constants.hPsAtmosphere);
	m_lightResources.sharedResources.hSkyTransmittanceLut = rSkyRenderer.GetTransmittanceLut();

	// Updating lighting.
	s_actionSharedData.lighting.QueueDraw(this, &m_lights, g_pRenderer->GetLightingMethod(), m_sky.volumetricFog, sceneDepthMin, sceneDepthMax, &m_lightResources);
	
	// Re-assign sky material's volumetric fog LUT.
	s_actionSharedData.sky.AssignExternalResources(m_lightResources.sharedResources.hVolumetricFogLut);

	//
	RdrResourceHandle ahResources[8];
	ahResources[0] = hEnvironmentMapTexArray;
	ahResources[1] = m_lightResources.sharedResources.hVolumetricFogLut;
	ahResources[2] = m_lightResources.sharedResources.hSkyTransmittanceLut;
	ahResources[3] = m_lightResources.sharedResources.hShadowMapTexArray;
	ahResources[4] = m_lightResources.sharedResources.hShadowCubeMapTexArray;
	ahResources[5] = m_lightResources.hSpotLightListRes;
	ahResources[6] = m_lightResources.hPointLightListRes;
	ahResources[7] = m_lightResources.sharedResources.hLightIndicesRes;

	m_lightResources.pResourcesTable = RdrResourceSystem::CreateTempShaderResourceViewTable(ahResources, ARRAYSIZE(ahResources), CREATE_BACKPOINTER(this));
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
		rPassData.shaderMode = RdrShaderMode::ShadowMap;
	}

	createPerActionConstants(this, rCamera, viewport, rShadowPass.constants);
}


//////////////////////////////////////////////////////////////////////////
// Drawing
//////////////////////////////////////////////////////////////////////////
void RdrAction::DrawIdle(RdrContext* pContext)
{
}

void RdrAction::Draw(RdrContext* pContext, RdrDrawState* pDrawState, RdrProfiler* pProfiler)
{
	// Cache render objects
	m_pContext = pContext;
	m_pDrawState = pDrawState;
	m_pProfiler = pProfiler;

	// Update global resources
	RdrGlobalResources globalResources;
	globalResources.renderTargets[(int)RdrGlobalRenderTargetHandles::kPrimary] = pContext->GetPrimaryRenderTarget();
	RdrResourceSystem::SetActiveGlobalResources(globalResources);

	// Setup global descriptor tables
	RdrConstantBufferHandle ahConstantBuffers[3];
	ahConstantBuffers[0] = m_constants.hPsPerAction;
	ahConstantBuffers[1] = m_lightResources.hGlobalLightsCb;
	ahConstantBuffers[2] = m_constants.hPsAtmosphere;
	m_pDrawState->pPsGlobalConstantBufferTable = RdrResourceSystem::CreateTempConstantBufferTable(ahConstantBuffers, ARRAYSIZE(ahConstantBuffers), CREATE_BACKPOINTER(this));

	m_pDrawState->pPsGlobalShaderResourceViewTable = m_lightResources.pResourcesTable;
	m_pDrawState->pPsScreenShaderResourceViewTable = nullptr;

	m_pContext->BeginEvent(m_name);

	//////////////////////////////////////////////////////////////////////////
	// Shadow passes

	m_pProfiler->BeginSection(RdrProfileSection::Shadows);
	for (int iShadow = 0; iShadow < m_shadowPassCount; ++iShadow)
	{
		DrawShadowPass(iShadow);
	}
	m_pProfiler->EndSection();

	//////////////////////////////////////////////////////////////////////////
	// Normal draw passes
	DrawPass(RdrPass::ZPrepass);
	DrawPass(RdrPass::LightCulling);
	DrawPass(RdrPass::VolumetricFog);
	DrawPass(RdrPass::Opaque);
	DrawPass(RdrPass::Decal);
	DrawPass(RdrPass::Sky);
	DrawPass(RdrPass::Alpha);

	// Resolve multi-sampled color buffer.
	if (g_debugState.msaaLevel > 1)
	{
		const RdrResource* pColorBufferMultisampled = RdrResourceSystem::GetResource(m_surfaces.colorBuffer.hTextureMultisampled);
		const RdrResource* pColorBuffer = RdrResourceSystem::GetResource(m_surfaces.colorBuffer.hTexture);
		pColorBufferMultisampled->ResolveResource(*pContext, *pColorBuffer);
	}

	if (m_bEnablePostProcessing)
	{
		m_pProfiler->BeginSection(RdrProfileSection::PostProcessing);
		s_actionSharedData.postProcess.DoPostProcessing(m_pContext, m_pDrawState, m_surfaces, m_postProcessEffects, m_constants);
		m_pProfiler->EndSection();
	}

	// Wireframe
	DrawPass(RdrPass::Wireframe);

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

void RdrAction::DrawBucket(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants)
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
					DrawGeo(rPass, rGlobalConstants, pPendingEntry->pDrawOp, instanceCount);
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
			DrawGeo(rPass, rGlobalConstants, pPendingEntry->pDrawOp, instanceCount);
		}
	}
	else
	{
		for (const RdrDrawBucketEntry& rEntry : rBucket)
		{
			DrawGeo(rPass, rGlobalConstants, rEntry.pDrawOp, 1);
		}
	}
}

void RdrAction::DrawGeo(const RdrPassData& rPass, const RdrGlobalConstants& rGlobalConstants,
	const RdrDrawOp* pDrawOp, uint instanceCount)
{
	bool bDepthOnly = (rPass.shaderMode == RdrShaderMode::DepthOnly || rPass.shaderMode == RdrShaderMode::ShadowMap);
	const RdrGeometry* pGeo = RdrResourceSystem::GetGeo(pDrawOp->hGeo);

	bool instanced = (instanceCount > 1);

	const RdrMaterial* pMaterial = pDrawOp->pMaterial;
	m_pDrawState->shaderStages = pMaterial->GetActiveShaderStages(rPass.shaderMode);
	m_pDrawState->pPipelineState = pMaterial->GetPipelineState(rPass.shaderMode);

	if (!m_pDrawState->pPipelineState)
		return;

	RdrConstantBufferHandle hPerActionVs = rGlobalConstants.hVsPerAction;

	m_pDrawState->pVsPerObjectConstantBuffer = RdrResourceSystem::GetConstantBuffer(pDrawOp->hVsConstants)->GetCBV().pDesc;

	if (instanced)
	{
		RdrResource& rBuffer = s_actionSharedData.instanceIds[s_actionSharedData.currentInstanceIds].buffer;
		rBuffer.UpdateResource(*m_pContext, s_actionSharedData.instanceIds[s_actionSharedData.currentInstanceIds].ids, rBuffer.GetSize());
		m_pDrawState->pVsGlobalConstantBufferTable = rBuffer.GetCBV().pDesc;

		m_pDrawState->pVsShaderResourceViewTable = RdrResourceSystem::GetResource(RdrInstancedObjectDataBuffer::GetResourceHandle())->GetSRV().pDesc;
	}
	else
	{
		m_pDrawState->pVsGlobalConstantBufferTable = RdrResourceSystem::GetConstantBuffer(hPerActionVs)->GetCBV().pDesc;
	}

	// Tessellation material
	if (pMaterial->IsShaderStageActive(rPass.shaderMode, RdrShaderStageFlags::Domain))
	{
		const RdrTessellationMaterialData& tessellation = pMaterial->GetTessellationData();
		m_pDrawState->pDsPerObjectConstantBuffer = RdrResourceSystem::GetConstantBuffer(tessellation.hDsConstants)->GetCBV().pDesc;
		m_pDrawState->pDsSamplerTable = tessellation.pSamplerDescriptorTable;
		m_pDrawState->pDsShaderResourceViewTable = tessellation.pResourceDescriptorTable;

		// Copy per-action constant buffer to domain shader
		m_pDrawState->pDsGlobalConstantBufferTable = m_pDrawState->pVsGlobalConstantBufferTable;
	}

	// Pixel shader
	if (pMaterial)
	{
		if (pMaterial->IsShaderStageActive(rPass.shaderMode, RdrShaderStageFlags::Pixel))
		{
			m_pDrawState->pPsMaterialShaderResourceViewTable = pMaterial->GetResourceDescriptorTable();
			m_pDrawState->pPsMaterialSamplerTable = pMaterial->GetSamplerDescriptorTable();

			const RdrConstantBufferHandle& hConstants = pMaterial->GetConstantBufferHandle();
			if (hConstants != 0)
			{
				m_pDrawState->pPsMaterialConstantBufferTable = RdrResourceSystem::GetConstantBuffer(hConstants)->GetCBV().pDesc;
			}
			else
			{
				m_pDrawState->pPsMaterialConstantBufferTable = m_pContext->GetNullConstantBufferView().pDesc;
			}

			if (pMaterial->NeedsScreenDepth())
			{
				m_pDrawState->pPsScreenShaderResourceViewTable = RdrResourceSystem::GetResource(m_surfaces.hDepthBuffer)->GetSRV().pDesc;
			}
			else
			{
				m_pDrawState->pPsScreenShaderResourceViewTable = nullptr;
			}
		}
	}

	m_pDrawState->pVertexBuffers[0] = pGeo->pVertexBuffer;
	m_pDrawState->vertexStrides[0] = pGeo->geoInfo.vertStride;
	m_pDrawState->vertexOffsets[0] = 0;
	m_pDrawState->vertexBufferCount = 1;
	m_pDrawState->vertexCount = pGeo->geoInfo.numVerts;

	m_pDrawState->eTopology = pGeo->geoInfo.eTopology;

	if (pDrawOp->hCustomInstanceBuffer)
	{
		const RdrResource* pInstanceData = RdrResourceSystem::GetResource(pDrawOp->hCustomInstanceBuffer);
		m_pDrawState->pVertexBuffers[1] = pInstanceData;
		m_pDrawState->vertexStrides[1] = pInstanceData->GetBufferInfo().elementSize;
		m_pDrawState->vertexOffsets[1] = 0;
		m_pDrawState->vertexBufferCount = 2;

		assert(instanceCount == 1);
		instanceCount = pDrawOp->instanceCount;
	}

	if (pGeo->pIndexBuffer && pGeo->pIndexBuffer->IsValid())
	{
		m_pDrawState->pIndexBuffer = pGeo->pIndexBuffer;
		m_pDrawState->indexCount = pGeo->geoInfo.numIndices;
		m_pProfiler->AddCounter(RdrProfileCounter::Triangles, instanceCount * m_pDrawState->indexCount / 3);
	}
	else
	{
		m_pDrawState->pIndexBuffer = nullptr;
		m_pProfiler->AddCounter(RdrProfileCounter::Triangles, instanceCount * m_pDrawState->vertexCount / 3);
	}

	// Done
	m_pContext->Draw(*m_pDrawState, instanceCount);
	m_pProfiler->IncrementCounter(RdrProfileCounter::DrawCall);
}

void RdrAction::DispatchCompute(const RdrComputeOp* pComputeOp)
{
	m_pDrawState->pPipelineState = pComputeOp->pipelineState;

	m_pDrawState->pCsConstantBufferTable = pComputeOp->pConstantsDescriptorTable ? pComputeOp->pConstantsDescriptorTable : m_pContext->GetNullConstantBufferView().pDesc;
	m_pDrawState->pCsSamplerTable = pComputeOp->pSamplerDescriptorTable ? pComputeOp->pSamplerDescriptorTable : m_pContext->GetSampler(RdrSamplerState()).pDesc;
	m_pDrawState->pCsShaderResourceViewTable = pComputeOp->pResourceDescriptorTable ? pComputeOp->pResourceDescriptorTable : m_pContext->GetNullShaderResourceView().pDesc;
	m_pDrawState->pCsUnorderedAccessViewTable = pComputeOp->pUnorderedAccessDescriptorTable ? pComputeOp->pUnorderedAccessDescriptorTable : m_pContext->GetNullUnorderedAccessView().pDesc;

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
			renderTargets[i] = m_pContext->GetNullRenderTargetView();
		}
	}

	RdrDepthStencilView depthView = rPass.hDepthTarget
		? RdrResourceSystem::GetDepthStencilView(rPass.hDepthTarget)
		: m_pContext->GetNullDepthStencilView();

	if (rPass.bClearRenderTargets)
	{
		for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
		{
			if (rPass.ahRenderTargets[i]) // Only clear non-null targets
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

	m_pContext->SetRenderTargets(MAX_RENDER_TARGETS, renderTargets, depthView);
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
	DrawBucket(rPass, rBucket, rGlobalConstants);

	m_pContext->EndEvent();
	m_pProfiler->EndSection();
}

void RdrAction::DrawShadowPass(int shadowPassIndex)
{
	const RdrShadowPass& rShadowPass = m_shadowPasses[shadowPassIndex];
	const RdrPassData& rPassData = rShadowPass.passData;
	m_pContext->BeginEvent(L"Shadow Map");

	RdrRenderTargetView renderTargets[MAX_RENDER_TARGETS];
	uint nNumRenderTargets = 0;
	for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
	{
		if (rPassData.ahRenderTargets[i])
		{
			++nNumRenderTargets;
			renderTargets[i] = RdrResourceSystem::GetRenderTargetView(rPassData.ahRenderTargets[i]);
		}
		else
		{
			renderTargets[i] = m_pContext->GetNullRenderTargetView();
		}
	}

	RdrDepthStencilView depthView = rPassData.hDepthTarget 
		? RdrResourceSystem::GetDepthStencilView(rPassData.hDepthTarget) 
		: m_pContext->GetNullDepthStencilView();

	if (rPassData.bClearRenderTargets)
	{
		for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
		{
			if (rPassData.ahRenderTargets[i]) // Only clear non-null targets
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

	m_pContext->SetRenderTargets(nNumRenderTargets, renderTargets, depthView);
	m_pContext->SetViewport(rPassData.viewport);

	const RdrDrawOpBucket& rDrawBucket = GetDrawOpBucket((RdrBucketType)((int)RdrBucketType::ShadowMap0 + shadowPassIndex));
	DrawBucket(rPassData, rDrawBucket, rShadowPass.constants);

	m_pContext->EndEvent();
}