#include "Precompiled.h"
#include "Renderer.h"
#include "Camera.h"
#include "Entity.h"
#include "Font.h"
#include <DXGIFormat.h>
#include "debug\DebugConsole.h"
#include "RdrContextD3D11.h"
#include "RdrShaderConstants.h"
#include "RdrFrameMem.h"
#include "RdrDrawOp.h"
#include "RdrComputeOp.h"
#include "RdrInstancedObjectDataBuffer.h"
#include "Scene.h"
#include "components/Renderable.h"
#include "components/Light.h"
#include "components/SkyVolume.h"
#include "components/PostProcessVolume.h"
#include "components/ModelInstance.h"
#include "AssetLib/SceneAsset.h"

Renderer* g_pRenderer = nullptr;

namespace
{
	const uint kMaxInstancesPerDraw = 2048;

	void cmdSetLightingMethod(DebugCommandArg *args, int numArgs)
	{
		g_pRenderer->SetLightingMethod((RdrLightingMethod)args[0].val.inum);
	}

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

	void cullSceneToCameraForShadows(const Camera& rCamera, RdrDrawBuckets* pBuckets)
	{
		for (ModelInstance& rModel : Scene::GetComponentAllocator()->GetModelInstanceFreeList())
		{
			Entity* pEntity = rModel.GetEntity();
			if (rCamera.CanSee(pEntity->GetPosition(), rModel.GetRadius()))
			{
				// todo: Flag to ignore non-opaque objects
				rModel.QueueDraw(pBuckets);
			}
		}
	}

	AssetLib::SkySettings blendSkyForCamera(const Camera& rCamera)
	{
		AssetLib::SkySettings sky;
		for (SkyVolume& rSkyVolume : Scene::GetComponentAllocator()->GetSkyVolumeFreeList())
		{
			sky = rSkyVolume.GetSkySettings();
			// For now, only support one sky object.
			// TODO: Support blending of skies via volume changes and such.
			//		Will probably need to trim the SkyComponent down to mostly data and
			//		create a function/class that blends them together and maintains the render resources.	
			break;
		}

		return sky;
	}

	void createPerActionConstants(RdrResourceCommandList& rResCommandList, const Camera& rCamera, const Rect& rViewport, const AssetLib::VolumetricFogSettings& rFog, RdrGlobalConstants& rConstants)
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
		pPsPerAction->volumetricFogFarDepth = rFog.farDepth;

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

//////////////////////////////////////////////////////

bool Renderer::Init(HWND hWnd, int width, int height, InputManager* pInputManager)
{
	assert(g_pRenderer == nullptr);
	g_pRenderer = this;

	DebugConsole::RegisterCommand("lightingMethod", cmdSetLightingMethod, DebugCommandArgType::Integer);

	m_pContext = new RdrContextD3D11(m_profiler);
	if (!m_pContext->Init(hWnd, width, height))
		return false;

	m_profiler.Init(m_pContext);
	m_pInputManager = pInputManager;

	RdrResourceSystem::Init(*this);

	// Set default lighting method before initialized shaders so global defines can be applied first.
	m_eLightingMethod = RdrLightingMethod::Clustered;
	SetLightingMethod(m_eLightingMethod);

	RdrShaderSystem::Init(m_pContext);
	m_lighting.Init();
	m_sky.Init();

	Resize(width, height);
	ApplyDeviceChanges();

	Font::Init();
	m_postProcess.Init();

	// Create instance ids buffers
	m_currentInstanceIds = 0;
	for (int i = 0; i < ARRAY_SIZE(m_instanceIds); ++i)
	{
		size_t temp;
		void* pAlignedData;

		uint dataSize = kMaxInstancesPerDraw * sizeof(uint) + 16;
		m_instanceIds[i].pData = new char[dataSize];
		pAlignedData = (void*)m_instanceIds[i].pData;
		std::align(16, dataSize, pAlignedData, temp);

		m_instanceIds[i].ids = (uint*)pAlignedData;
		m_instanceIds[i].buffer.size = kMaxInstancesPerDraw * sizeof(uint);
		m_instanceIds[i].buffer.bufferObj = m_pContext->CreateConstantBuffer(nullptr, m_instanceIds[i].buffer.size, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}

	return true;
}

void Renderer::Cleanup()
{
	// todo: flip frame to finish resource frees.
	m_profiler.Cleanup();
	m_pContext->Release();
	delete m_pContext;
}

void Renderer::Resize(int width, int height)
{
	if (width == 0 || height == 0)
		return;

	m_pendingViewWidth = width;
	m_pendingViewHeight = height;
}

void Renderer::ApplyDeviceChanges()
{
	if (m_pendingViewWidth != m_viewWidth || m_pendingViewHeight != m_viewHeight)
	{
		RdrResourceCommandList& rResCommandList = GetPreFrameCommandList();
		m_pContext->Resize(m_pendingViewWidth, m_pendingViewHeight);

		// Release existing resources
		if (m_hPrimaryDepthStencilView)
			rResCommandList.ReleaseDepthStencilView(m_hPrimaryDepthStencilView);
		if (m_hPrimaryDepthBuffer)
			rResCommandList.ReleaseResource(m_hPrimaryDepthBuffer);
		if (m_hColorBuffer)
			rResCommandList.ReleaseResource(m_hColorBuffer);
		if (m_hColorBufferMultisampled)
			rResCommandList.ReleaseResource(m_hColorBufferMultisampled);
		if (m_hColorBufferRenderTarget)
			rResCommandList.ReleaseRenderTargetView(m_hColorBufferRenderTarget);

		// FP16 color
		m_hColorBuffer = rResCommandList.CreateTexture2D(m_pendingViewWidth, m_pendingViewHeight, RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default, nullptr);
		if (g_debugState.msaaLevel > 1)
		{
			m_hColorBufferMultisampled = rResCommandList.CreateTexture2DMS(m_pendingViewWidth, m_pendingViewHeight, RdrResourceFormat::R16G16B16A16_FLOAT, g_debugState.msaaLevel);
			m_hColorBufferRenderTarget = rResCommandList.CreateRenderTargetView(m_hColorBufferMultisampled);
		}
		else
		{
			m_hColorBufferMultisampled = 0;
			m_hColorBufferRenderTarget = rResCommandList.CreateRenderTargetView(m_hColorBuffer);
		}

		// Depth Buffer
		m_hPrimaryDepthBuffer = rResCommandList.CreateTexture2DMS(m_pendingViewWidth, m_pendingViewHeight, RdrResourceFormat::D24_UNORM_S8_UINT, g_debugState.msaaLevel);
		m_hPrimaryDepthStencilView = rResCommandList.CreateDepthStencilView(m_hPrimaryDepthBuffer);

		m_viewWidth = m_pendingViewWidth;
		m_viewHeight = m_pendingViewHeight;

		m_postProcess.HandleResize(m_viewWidth, m_viewHeight);
	}
}

const Camera& Renderer::GetCurrentCamera() const
{
	return m_pCurrentAction->camera;
}

RdrAction* Renderer::GetNextAction(const wchar_t* actionName, const Rect& viewport, bool enablePostProcessing, const RdrSurface& outputSurface)
{
	RdrRenderTargetViewHandle hColorTarget = enablePostProcessing ? m_hColorBufferRenderTarget : outputSurface.hRenderTarget;
	RdrDepthStencilViewHandle hDepthTarget = outputSurface.hDepthTarget;

	RdrFrameState& state = GetQueueState();
	assert(state.numActions < MAX_ACTIONS_PER_FRAME);
	RdrAction* pAction = &state.actions[state.numActions++];

	// Setup default action and pass states
	pAction->name = actionName;
	pAction->primaryViewport = viewport;
	pAction->bEnablePostProcessing = enablePostProcessing;

	// Z Prepass
	RdrPassData* pPass = &pAction->passes[(int)RdrPass::ZPrepass];
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
	pPass = &pAction->passes[(int)RdrPass::LightCulling];
	{
		pPass->viewport = viewport;
		pPass->bAlphaBlend = false;
		pPass->bClearDepthTarget = false;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Volumetric Fog
	pPass = &pAction->passes[(int)RdrPass::VolumetricFog];
	{
		pPass->viewport = viewport;
		pPass->bAlphaBlend = false;
		pPass->bClearDepthTarget = false;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Opaque
	pPass = &pAction->passes[(int)RdrPass::Opaque];
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
	pPass = &pAction->passes[(int)RdrPass::Sky];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = hColorTarget;
		pPass->hDepthTarget = hDepthTarget;
		pPass->depthTestMode = RdrDepthTestMode::Equal;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Editor
	pPass = &pAction->passes[(int)RdrPass::Editor];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = outputSurface.hRenderTarget;
		pPass->bAlphaBlend = true;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Wireframe
	pPass = &pAction->passes[(int)RdrPass::Wireframe];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = outputSurface.hRenderTarget;
		pPass->bAlphaBlend = true;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// UI
	pPass = &pAction->passes[(int)RdrPass::UI];
	{
		pPass->viewport = viewport;
		pPass->ahRenderTargets[0] = outputSurface.hRenderTarget;
		pPass->bAlphaBlend = true;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->bDepthWriteEnabled = false;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	return pAction;
}

void Renderer::SetLightingMethod(RdrLightingMethod eLightingMethod)
{
	m_ePendingLightingMethod = eLightingMethod;
	RdrShaderSystem::SetGlobalShaderDefine("CLUSTERED_LIGHTING", (eLightingMethod == RdrLightingMethod::Clustered));
}

void Renderer::QueueScene()
{
	const Camera& rCamera = m_pCurrentAction->camera;
	RdrLightList* pLightList = &m_pCurrentAction->lights;
	RdrDrawBuckets* pDrawBuckets = &m_pCurrentAction->opBuckets;
	DefaultComponentAllocator* pComponents = Scene::GetComponentAllocator();

	Vec3 camDir = rCamera.GetDirection();
	Vec3 camPos = rCamera.GetPosition();
	float depthMin = FLT_MAX;
	float depthMax = 0.f;


	// Models
	for (ModelInstance& rModel : pComponents->GetModelInstanceFreeList())
	{
		Entity* pEntity = rModel.GetEntity();
		float radius = rModel.GetRadius();
		if (!rCamera.CanSee(pEntity->GetPosition(), radius))
		{
			// Can't see the model.
			continue;
		}

		rModel.QueueDraw(pDrawBuckets);

		Vec3 diff = pEntity->GetPosition() - camPos;
		float distSqr = Vec3Dot(camDir, diff);
		float dist = sqrtf(max(0.f, distSqr));

		if (dist - radius < depthMin)
			depthMin = dist - radius;

		if (dist + radius > depthMax)
			depthMax = dist + radius;
	}

	// Terrain
	Scene::GetTerrain().QueueDraw(pDrawBuckets, rCamera);

	// Depth min/max.
	depthMin = max(rCamera.GetNearDist(), depthMin);
	depthMax = min(rCamera.GetFarDist(), depthMax);

	// Lighting
	for (Light& rLight : pComponents->GetLightFreeList())
	{
		pLightList->AddLight(&rLight);
	}

	m_pCurrentAction->lightParams.hEnvironmentMapTexArray = Scene::GetEnvironmentMapTexArray();
	m_lighting.QueueDraw(&m_pCurrentAction->lights, *this, m_pCurrentAction->camera, depthMin, depthMax,
		m_eLightingMethod, &m_pCurrentAction->lightParams);

	m_sky.QueueDraw(m_pCurrentAction, m_pCurrentAction->sky, pLightList);
	m_pCurrentAction->constants.hPsAtmosphere = m_sky.GetAtmosphereConstantBuffer();

	// Post-processing
	if (m_pCurrentAction->bEnablePostProcessing)
	{
		for (PostProcessVolume& rPostProcess : pComponents->GetPostProcessVolumeFreeList())
		{
			m_pCurrentAction->postProcessEffects = rPostProcess.GetEffects();
			// TODO: Add post-process effects blending.
			break;
		}

		m_postProcess.QueueDraw(m_pCurrentAction->postProcessEffects);
	}
}

void Renderer::QueueShadowMapPass(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport)
{
	assert(m_pCurrentAction);
	assert(m_pCurrentAction->shadowPassCount + 1 < MAX_SHADOW_MAPS_PER_FRAME);

	RdrShadowPass& rShadowPass = m_pCurrentAction->shadowPasses[m_pCurrentAction->shadowPassCount];
	m_pCurrentAction->shadowPassCount++;

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

	cullSceneToCameraForShadows(rCamera, &rShadowPass.buckets);
	rShadowPass.buckets.SortDrawOps(RdrBucketType::Opaque);

	createPerActionConstants(m_pCurrentAction->resourceCommands, rCamera, viewport, m_pCurrentAction->sky.volumetricFog, rShadowPass.constants);
	rShadowPass.constants.hPsAtmosphere = 0;
}

void Renderer::QueueShadowCubeMapPass(const PointLight& rLight, RdrDepthStencilViewHandle hDepthView, Rect& viewport)
{
	assert(m_pCurrentAction);
	assert(m_pCurrentAction->shadowPassCount + 1 < MAX_SHADOW_MAPS_PER_FRAME);

	RdrShadowPass& rShadowPass = m_pCurrentAction->shadowPasses[m_pCurrentAction->shadowPassCount];
	m_pCurrentAction->shadowPassCount++;

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

	cullSceneToCameraForShadows(rShadowPass.camera, &rShadowPass.buckets);
	rShadowPass.buckets.SortDrawOps(RdrBucketType::Opaque);

	createPerActionConstants(m_pCurrentAction->resourceCommands, rShadowPass.camera, viewport, m_pCurrentAction->sky.volumetricFog, rShadowPass.constants);
	rShadowPass.constants.hPsAtmosphere = 0;
	rShadowPass.constants.hGsCubeMap = createCubemapCaptureConstants(m_pCurrentAction->resourceCommands, rLight.position, 0.1f, rLight.radius * 2.f);
}

void Renderer::BeginPrimaryAction(Camera& rCamera)
{
	assert(m_pCurrentAction == nullptr);

	Rect viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);

	RdrSurface surface;
	surface.hRenderTarget = RdrResourceSystem::kPrimaryRenderTargetHandle;
	surface.hDepthTarget = m_hPrimaryDepthStencilView;

	m_pCurrentAction = GetNextAction(L"Primary Action", viewport, true, surface);

	rCamera.SetAspectRatio(m_viewWidth / (float)m_viewHeight);
	rCamera.UpdateFrustum();
	m_pCurrentAction->camera = rCamera;

	m_pCurrentAction->passes[(int)RdrPass::ZPrepass].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::LightCulling].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::Opaque].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::Sky].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::Alpha].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::UI].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::Wireframe].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::Editor].bEnabled = true;

	m_pCurrentAction->sky = blendSkyForCamera(m_pCurrentAction->camera);
	createPerActionConstants(m_pCurrentAction->resourceCommands, m_pCurrentAction->camera, viewport, m_pCurrentAction->sky.volumetricFog, m_pCurrentAction->constants);
	createUiConstants(m_pCurrentAction->resourceCommands, viewport, m_pCurrentAction->uiConstants);

	QueueScene();
}

void Renderer::BeginOffscreenAction(const wchar_t* actionName, Camera& rCamera,
	bool enablePostprocessing, const Rect& viewport, const RdrSurface& outputSurface)
{
	assert(m_pCurrentAction == nullptr);

	m_pCurrentAction = GetNextAction(actionName, viewport, enablePostprocessing, outputSurface);

	rCamera.SetAspectRatio(viewport.width / viewport.height);
	rCamera.UpdateFrustum();
	m_pCurrentAction->camera = rCamera;

	m_pCurrentAction->passes[(int)RdrPass::ZPrepass].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::LightCulling].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::Opaque].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::Sky].bEnabled = true;
	m_pCurrentAction->passes[(int)RdrPass::Alpha].bEnabled = true;

	m_pCurrentAction->sky = blendSkyForCamera(m_pCurrentAction->camera);
	createPerActionConstants(m_pCurrentAction->resourceCommands, m_pCurrentAction->camera, viewport, m_pCurrentAction->sky.volumetricFog, m_pCurrentAction->constants);

	QueueScene();
}

void Renderer::EndAction()
{
	m_pCurrentAction = nullptr;
}

RdrResourceCommandList& Renderer::GetPreFrameCommandList()
{
	return GetQueueState().preFrameResourceCommands;
}

RdrResourceCommandList& Renderer::GetPostFrameCommandList()
{
	return GetQueueState().postFrameResourceCommands;
}

RdrResourceCommandList* Renderer::GetActionCommandList()
{
	return m_pCurrentAction ? &m_pCurrentAction->resourceCommands : nullptr;
}

void Renderer::AddDrawOpToBucket(const RdrDrawOp* pDrawOp, RdrBucketType eBucket)
{
	m_pCurrentAction->opBuckets.AddDrawOp(pDrawOp, eBucket);
}

void Renderer::AddComputeOpToPass(const RdrComputeOp* pComputeOp, RdrPass ePass)
{
	m_pCurrentAction->opBuckets.AddComputeOp(pComputeOp, ePass);
}

void Renderer::DrawPass(const RdrAction& rAction, RdrPass ePass)
{
	const RdrPassData& rPass = rAction.passes[(int)ePass];

	if (!rPass.bEnabled)
		return;

	m_profiler.BeginSection(s_passProfileSections[(int)ePass]);
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
	m_pContext->SetBlendState(rPass.bAlphaBlend);

	m_pContext->SetViewport(rPass.viewport);

	// Compute ops
	{
		const RdrComputeOpBucket& rComputeBucket = rAction.opBuckets.GetComputeOpBucket(ePass);
		for (const RdrComputeOp* pComputeOp : rComputeBucket)
		{
			DispatchCompute(pComputeOp);
		}
	}

	// Draw ops
	const RdrDrawOpBucket& rBucket = rAction.opBuckets.GetDrawOpBucket(s_passBuckets[(int)ePass]);
	const RdrGlobalConstants& rGlobalConstants = (ePass == RdrPass::UI) ? rAction.uiConstants : rAction.constants;
	DrawBucket(rPass, rBucket, rGlobalConstants, rAction.lightParams);

	m_pContext->EndEvent();
	m_profiler.EndSection();
}

void Renderer::DrawShadowPass(const RdrShadowPass& rShadowPass)
{
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
	m_pContext->SetBlendState(rPassData.bAlphaBlend);

	m_pContext->SetViewport(rPassData.viewport);

	RdrLightResources lightParams;
	const RdrDrawOpBucket& rBucket = rShadowPass.buckets.GetDrawOpBucket(RdrBucketType::Opaque);
	DrawBucket(rPassData, rBucket, rShadowPass.constants, lightParams);

	m_pContext->EndEvent();
}

void Renderer::PostFrameSync()
{
	//////////////////////////////////////////////////////////////////////////
	// Finalize pending frame data.
	{
		RdrFrameState& rQueueState = GetQueueState();

		// Sort buckets and build object index buffers
		for (uint iAction = 0; iAction < rQueueState.numActions; ++iAction)
		{
			RdrAction& rAction = rQueueState.actions[iAction];
			for (int i = 0; i < (int)RdrBucketType::Count; ++i)
			{
				rAction.opBuckets.SortDrawOps((RdrBucketType)i);
			}
		}

		// Update global data
		RdrInstancedObjectDataBuffer::UpdateBuffer(*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// Clear the data that we just rendered with.
	RdrFrameState& rActiveState = GetActiveState();
	RdrResourceCommandList& rResCommandList = GetPostFrameCommandList();
	assert(!m_pCurrentAction);

	// Return old frame actions to the pool.
	for (uint iAction = 0; iAction < rActiveState.numActions; ++iAction)
	{
		RdrAction& rAction = rActiveState.actions[iAction];
		rAction.Reset();
	}

	// Clear remaining state data.
	rActiveState.numActions = 0;

	// Swap state
	m_queueState = !m_queueState;
	RdrShaderSystem::FlipState();
	RdrFrameMem::FlipState();
	RdrInstancedObjectDataBuffer::FlipState();

	m_eLightingMethod = m_ePendingLightingMethod;
}

void Renderer::ProcessReadbackRequests()
{
	AutoScopedLock lock(m_readbackMutex);

	uint numRequests = (uint)m_pendingReadbackRequests.size();
	for (uint i = 0; i < numRequests; ++i)
	{
		RdrResourceReadbackRequest* pRequest = m_readbackRequests.get(m_pendingReadbackRequests[i]);
		if (pRequest->frameCount == 0)
		{
			const RdrResource* pSrc = RdrResourceSystem::GetResource(pRequest->hSrcResource);
			const RdrResource* pDst = RdrResourceSystem::GetResource(pRequest->hDstResource);
			m_pContext->CopyResourceRegion(*pSrc, pRequest->srcRegion, *pDst, IVec3::kZero);
		}
		else if (pRequest->frameCount == 3)
		{
			// After 2 frames, we can safely read from the resource without stalling.
			const RdrResource* pDst = RdrResourceSystem::GetResource(pRequest->hDstResource);
			m_pContext->ReadResource(*pDst, pRequest->pData, pRequest->dataSize);
			pRequest->bComplete = true;

			// Remove from pending list
			m_pendingReadbackRequests.erase(m_pendingReadbackRequests.begin() + i);
			--i;
		}

		pRequest->frameCount++;
	}
}

void Renderer::DrawFrame()
{
	RdrFrameState& rFrameState = GetActiveState();

	// Process threaded render commands.
	RdrShaderSystem::ProcessCommands(m_pContext);
	rFrameState.preFrameResourceCommands.ProcessCommands(m_pContext);

	ProcessReadbackRequests();

	m_profiler.BeginFrame();

	// Draw the frame
	if (!m_pContext->IsIdle()) // If the device is idle (probably minimized), don't bother rendering anything.
	{
		for (uint iAction = 0; iAction < rFrameState.numActions; ++iAction)
		{
			// Process resource system commands for this action
			rFrameState.actions[iAction].resourceCommands.ProcessCommands(m_pContext);

			const RdrAction& rAction = rFrameState.actions[iAction];
			m_pContext->BeginEvent(rAction.name);

			//////////////////////////////////////////////////////////////////////////
			// Shadow passes

			// Setup shadow raster state with depth bias.
			RdrRasterState rasterState;
			rasterState.bEnableMSAA = false;
			rasterState.bEnableScissor = false;
			rasterState.bWireframe = false;
			rasterState.bUseSlopeScaledDepthBias = 1;
			m_pContext->SetRasterState(rasterState);

			m_profiler.BeginSection(RdrProfileSection::Shadows);
			for (int iShadow = 0; iShadow < rAction.shadowPassCount; ++iShadow)
			{
				DrawShadowPass(rAction.shadowPasses[iShadow]);
			}
			m_profiler.EndSection();

			//////////////////////////////////////////////////////////////////////////
			// Normal draw passes
			rasterState.bEnableMSAA = (g_debugState.msaaLevel > 1);
			rasterState.bEnableScissor = false;
			rasterState.bWireframe = g_debugState.wireframe;
			rasterState.bUseSlopeScaledDepthBias = 0;
			m_pContext->SetRasterState(rasterState);

			DrawPass(rAction, RdrPass::ZPrepass);
			DrawPass(rAction, RdrPass::LightCulling);
			DrawPass(rAction, RdrPass::VolumetricFog);
			DrawPass(rAction, RdrPass::Opaque);
			DrawPass(rAction, RdrPass::Sky);
			DrawPass(rAction, RdrPass::Alpha);

			if (g_debugState.wireframe)
			{
				rasterState.bWireframe = false;
				m_pContext->SetRasterState(rasterState);
			}

			// Resolve multi-sampled color buffer.
			const RdrResource* pColorBuffer = RdrResourceSystem::GetResource(m_hColorBuffer);
			if (g_debugState.msaaLevel > 1)
			{
				const RdrResource* pColorBufferMultisampled = RdrResourceSystem::GetResource(m_hColorBufferMultisampled);
				m_pContext->ResolveSurface(*pColorBufferMultisampled, *pColorBuffer);

				rasterState.bEnableMSAA = false;
				m_pContext->SetRasterState(rasterState);
			}
			else
			{
				RdrRenderTargetView renderTargets[MAX_RENDER_TARGETS] = { 0 };
				RdrDepthStencilView depthView = { 0 };
				m_pContext->SetRenderTargets(MAX_RENDER_TARGETS, renderTargets, depthView);
			}

			if (rAction.bEnablePostProcessing)
			{
				m_profiler.BeginSection(RdrProfileSection::PostProcessing);
				m_postProcess.DoPostProcessing(*m_pInputManager, m_pContext, m_drawState, pColorBuffer, rAction.postProcessEffects);
				m_profiler.EndSection();
			}


			DrawPass(rAction, RdrPass::Editor);

			{
				rasterState.bWireframe = true;
				m_pContext->SetRasterState(rasterState);

				DrawPass(rAction, RdrPass::Wireframe);

				rasterState.bWireframe = false;
				m_pContext->SetRasterState(rasterState);
			}

			DrawPass(rAction, RdrPass::UI);

			m_pContext->EndEvent();
		}
	}
	else
	{
		// Process commands even though we're not rendering.
		for (uint iAction = 0; iAction < rFrameState.numActions; ++iAction)
		{
			rFrameState.actions[iAction].resourceCommands.ProcessCommands(m_pContext);
		}
	}

	m_pContext->Present();
	m_profiler.EndFrame();

	// Process post-frame commands.
	rFrameState.postFrameResourceCommands.ProcessCommands(m_pContext);
}

void Renderer::DrawBucket(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, 
	const RdrGlobalConstants& rGlobalConstants, const RdrLightResources& rLightParams)
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
					DrawGeo(rPass, rBucket, rGlobalConstants, pPendingEntry->pDrawOp, rLightParams, instanceCount);
					pPendingEntry = nullptr;
				}
			}

			// Update pending entry if it was not set or we just drew.
			if (!pPendingEntry)
			{
				m_currentInstanceIds++;
				if (m_currentInstanceIds >= 8)
					m_currentInstanceIds = 0;

				pCurrInstanceIds = m_instanceIds[m_currentInstanceIds].ids;
				pCurrInstanceIds[0] = rEntry.pDrawOp->instanceDataId;

				pPendingEntry = &rEntry;
				instanceCount = 1;
			}
		}

		if (pPendingEntry)
		{
			DrawGeo(rPass, rBucket, rGlobalConstants, pPendingEntry->pDrawOp, rLightParams, instanceCount);
		}
	}
	else
	{
		for (const RdrDrawBucketEntry& rEntry : rBucket)
		{
			DrawGeo(rPass, rBucket, rGlobalConstants, rEntry.pDrawOp, rLightParams, 1);
		}
	}
}

void Renderer::DrawGeo(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants,
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

	m_drawState.pVertexShader = RdrShaderSystem::GetVertexShader(vertexShader);

	RdrConstantBufferHandle hPerActionVs = rGlobalConstants.hVsPerAction;
	m_drawState.vsConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(hPerActionVs)->bufferObj;
	m_drawState.vsConstantBufferCount = 1;

	m_drawState.dsConstantBuffers[0] = m_drawState.vsConstantBuffers[0];
	m_drawState.dsConstantBufferCount = 1;

	if (instanced)
	{
		m_pContext->UpdateConstantBuffer(m_instanceIds[m_currentInstanceIds].buffer, m_instanceIds[m_currentInstanceIds].ids);
		m_drawState.vsConstantBuffers[1] = m_instanceIds[m_currentInstanceIds].buffer.bufferObj;
		m_drawState.vsConstantBufferCount = 2;

		m_drawState.vsResources[0] = RdrResourceSystem::GetResource(RdrInstancedObjectDataBuffer::GetResourceHandle())->resourceView;
		m_drawState.vsResourceCount = 1;
	}
	else if (pDrawOp->hVsConstants)
	{
		m_drawState.vsConstantBuffers[1] = RdrResourceSystem::GetConstantBuffer(pDrawOp->hVsConstants)->bufferObj;
		m_drawState.vsConstantBufferCount = 2;
	}

	// Geom shader
	if (rPass.bIsCubeMapCapture)
	{
		RdrGeometryShader geomShader = { RdrGeometryShaderType::Model_CubemapCapture, vertexShader.flags };
		m_drawState.pGeometryShader = RdrShaderSystem::GetGeometryShader(geomShader);
		m_drawState.gsConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hGsCubeMap)->bufferObj;
		m_drawState.gsConstantBufferCount = 1;
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
		m_drawState.pHullShader = RdrShaderSystem::GetHullShader(tessShader);
		m_drawState.pDomainShader = RdrShaderSystem::GetDomainShader(tessShader);

		m_drawState.dsResourceCount = pTessMaterial->ahResources.size();
		for (uint i = 0; i < m_drawState.dsResourceCount; ++i)
		{
			m_drawState.dsResources[i] = RdrResourceSystem::GetResource(pTessMaterial->ahResources.get(i))->resourceView;
		}

		m_drawState.dsSamplerCount = pTessMaterial->aSamplers.size();
		for (uint i = 0; i < m_drawState.dsSamplerCount; ++i)
		{
			m_drawState.dsSamplers[i] = pTessMaterial->aSamplers.get(i);
		}

		if (pTessMaterial->hDsConstants)
		{
			m_drawState.dsConstantBuffers[1] = RdrResourceSystem::GetConstantBuffer(pTessMaterial->hDsConstants)->bufferObj;
			m_drawState.dsConstantBufferCount = 2;
		}
	}

	// Pixel shader
	const RdrMaterial* pMaterial = pDrawOp->pMaterial;
	if (pMaterial)
	{
		m_drawState.pPixelShader = pMaterial->hPixelShaders[(int)rPass.shaderMode] ?
			RdrShaderSystem::GetPixelShader(pMaterial->hPixelShaders[(int)rPass.shaderMode]) :
			nullptr;
		if (m_drawState.pPixelShader)
		{
			m_drawState.psResourceCount = pMaterial->ahTextures.size();
			for (uint i = 0; i < m_drawState.psResourceCount; ++i)
			{
				m_drawState.psResources[i] = RdrResourceSystem::GetResource(pMaterial->ahTextures.get(i))->resourceView;
			}

			m_drawState.psSamplerCount = pMaterial->aSamplers.size();
			for (uint i = 0; i < m_drawState.psSamplerCount; ++i)
			{
				m_drawState.psSamplers[i] = pMaterial->aSamplers.get(i);
			}

			m_drawState.psConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hPsPerAction)->bufferObj;
			m_drawState.psConstantBufferCount++;

			if (pMaterial->bNeedsLighting && !bDepthOnly)
			{
				m_drawState.psConstantBuffers[1] = RdrResourceSystem::GetConstantBuffer(rLightParams.hGlobalLightsCb)->bufferObj;
				m_drawState.psConstantBufferCount++;

				m_drawState.psConstantBuffers[2] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hPsAtmosphere)->bufferObj;
				m_drawState.psConstantBufferCount++;

				m_drawState.psConstantBuffers[3] = RdrResourceSystem::GetConstantBuffer(pMaterial->hConstants)->bufferObj;
				m_drawState.psConstantBufferCount++;

				m_drawState.psResources[(int)RdrPsResourceSlots::SpotLightList] = RdrResourceSystem::GetResource(rLightParams.hSpotLightListRes)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::PointLightList] = RdrResourceSystem::GetResource(rLightParams.hPointLightListRes)->resourceView;

				m_drawState.psResources[(int)RdrPsResourceSlots::EnvironmentMaps] = RdrResourceSystem::GetResource(rLightParams.hEnvironmentMapTexArray)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::VolumetricFogLut] = RdrResourceSystem::GetResource(rLightParams.hVolumetricFogLut)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::SkyTransmittance] = RdrResourceSystem::GetResource(rLightParams.hSkyTransmittanceLut)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::LightIds] = RdrResourceSystem::GetResource(rLightParams.hLightIndicesRes)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::ShadowMaps] = RdrResourceSystem::GetResource(rLightParams.hShadowMapTexArray)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::ShadowCubeMaps] = RdrResourceSystem::GetResource(rLightParams.hShadowCubeMapTexArray)->resourceView;
				m_drawState.psResourceCount = max((uint)RdrPsResourceSlots::LightIds + 1, m_drawState.psResourceCount);

				m_drawState.psSamplers[(int)RdrPsSamplerSlots::Clamp] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
				m_drawState.psSamplers[(int)RdrPsSamplerSlots::ShadowMap] = RdrSamplerState(RdrComparisonFunc::LessEqual, RdrTexCoordMode::Clamp, false);
				m_drawState.psSamplerCount = max((uint)RdrPsSamplerSlots::ShadowMap + 1, m_drawState.psSamplerCount);
			}
		}
	}

	// Input assembly
	m_drawState.inputLayout = *RdrShaderSystem::GetInputLayout(pDrawOp->hInputLayout); // todo: layouts per flags
	m_drawState.eTopology = pGeo->geoInfo.eTopology;

	m_drawState.vertexBuffers[0] = pGeo->vertexBuffer;
	m_drawState.vertexStrides[0] = pGeo->geoInfo.vertStride;
	m_drawState.vertexOffsets[0] = 0;
	m_drawState.vertexBufferCount = 1;
	m_drawState.vertexCount = pGeo->geoInfo.numVerts;

	if (pDrawOp->hCustomInstanceBuffer)
	{
		const RdrResource* pInstanceData = RdrResourceSystem::GetResource(pDrawOp->hCustomInstanceBuffer);
		m_drawState.vertexBuffers[1].pBuffer = pInstanceData->pBuffer;
		m_drawState.vertexStrides[1] = pInstanceData->bufferInfo.elementSize;
		m_drawState.vertexOffsets[1] = 0;
		m_drawState.vertexBufferCount = 2;

		assert(instanceCount == 1);
		instanceCount = pDrawOp->instanceCount;
	}

	if (pGeo->indexBuffer.pBuffer)
	{
		m_drawState.indexBuffer = pGeo->indexBuffer;
		m_drawState.indexCount = pGeo->geoInfo.numIndices;
		m_profiler.AddCounter(RdrProfileCounter::Triangles, instanceCount * m_drawState.indexCount / 3);
	}
	else
	{
		m_drawState.indexBuffer.pBuffer = nullptr;
		m_profiler.AddCounter(RdrProfileCounter::Triangles, instanceCount * m_drawState.vertexCount / 3);
	}

	// Done
	m_pContext->Draw(m_drawState, instanceCount);
	m_profiler.IncrementCounter(RdrProfileCounter::DrawCall);

	m_drawState.Reset();
}

void Renderer::DispatchCompute(const RdrComputeOp* pComputeOp)
{
	m_drawState.pComputeShader = RdrShaderSystem::GetComputeShader(pComputeOp->shader);

	m_drawState.csConstantBufferCount = pComputeOp->ahConstantBuffers.size();
	for (uint i = 0; i < m_drawState.csConstantBufferCount; ++i)
	{
		m_drawState.csConstantBuffers[i] = RdrResourceSystem::GetConstantBuffer(pComputeOp->ahConstantBuffers.get(i))->bufferObj;
	}

	uint count = pComputeOp->ahResources.size();
	for (uint i = 0; i < count; ++i)
	{
		if (pComputeOp->ahResources.get(i))
			m_drawState.csResources[i] = RdrResourceSystem::GetResource(pComputeOp->ahResources.get(i))->resourceView;
		else
			m_drawState.csResources[i].pTypeless = nullptr;
	}

	count = pComputeOp->aSamplers.size();
	for (uint i = 0; i < count; ++i)
	{
		m_drawState.csSamplers[i] = pComputeOp->aSamplers.get(i);
	}

	count = pComputeOp->ahWritableResources.size();
	for (uint i = 0; i < count; ++i)
	{
		if (pComputeOp->ahWritableResources.get(i))
			m_drawState.csUavs[i] = RdrResourceSystem::GetResource(pComputeOp->ahWritableResources.get(i))->uav;
		else
			m_drawState.csUavs[i].pTypeless = nullptr;
	}

	m_pContext->DispatchCompute(m_drawState, pComputeOp->threads[0], pComputeOp->threads[1], pComputeOp->threads[2]);

	m_drawState.Reset();
}


RdrResourceReadbackRequestHandle Renderer::IssueTextureReadbackRequest(RdrResourceHandle hResource, const IVec3& pixelCoord)
{
	RdrResourceReadbackRequest* pReq = m_readbackRequests.alloc();
	const RdrResource* pSrcResource = RdrResourceSystem::GetResource(hResource);
	pReq->hSrcResource = hResource;
	pReq->frameCount = 0;
	pReq->bComplete = false;
	pReq->srcRegion = RdrBox(pixelCoord.x, pixelCoord.y, 0, 1, 1, 1);
	pReq->hDstResource = GetPreFrameCommandList().CreateTexture2D(1, 1, pSrcResource->texInfo.format, RdrResourceUsage::Staging, nullptr);
	pReq->dataSize = rdrGetTexturePitch(1, pSrcResource->texInfo.format);
	pReq->pData = new char[pReq->dataSize]; // todo: custom heap

	AutoScopedLock lock(m_readbackMutex);
	RdrResourceReadbackRequestHandle handle = m_readbackRequests.getId(pReq);
	m_pendingReadbackRequests.push_back(handle);
	return handle;
}

RdrResourceReadbackRequestHandle Renderer::IssueStructuredBufferReadbackRequest(RdrResourceHandle hResource, uint startByteOffset, uint numBytesToRead)
{
	RdrResourceReadbackRequest* pReq = m_readbackRequests.alloc();
	pReq->hSrcResource = hResource;
	pReq->frameCount = 0;
	pReq->bComplete = false;
	pReq->srcRegion = RdrBox(startByteOffset, 0, 0, numBytesToRead, 1, 1);
	pReq->hDstResource = GetPreFrameCommandList().CreateStructuredBuffer(nullptr, 1, numBytesToRead, RdrResourceUsage::Staging);
	pReq->dataSize = numBytesToRead;
	pReq->pData = new char[numBytesToRead]; // todo: custom heap

	AutoScopedLock lock(m_readbackMutex);
	RdrResourceReadbackRequestHandle handle = m_readbackRequests.getId(pReq);
	m_pendingReadbackRequests.push_back(handle);
	return handle;
}

void Renderer::ReleaseResourceReadbackRequest(RdrResourceReadbackRequestHandle hRequest)
{
	RdrResourceReadbackRequest* pReq = m_readbackRequests.get(hRequest);
	if (!pReq->bComplete)
	{
		AutoScopedLock lock(m_readbackMutex);

		uint numReqs = (uint)m_pendingReadbackRequests.size();
		for (uint i = 0; i < numReqs; ++i)
		{
			if (m_pendingReadbackRequests[i] == hRequest)
			{
				if (i != numReqs - 1)
				{
					// If this isn't the last entry, swap it so that we can just pop the end off the vector.
					m_pendingReadbackRequests[i] = m_pendingReadbackRequests.back();
				}
				m_pendingReadbackRequests.pop_back();
				break;
			}
		}
	}

	// todo: pool/reuse dest resources.
	GetPreFrameCommandList().ReleaseResource(pReq->hDstResource);
	SAFE_DELETE(pReq->pData);

	m_readbackRequests.releaseId(hRequest);
}
