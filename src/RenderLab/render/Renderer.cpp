#include "Precompiled.h"
#include "Renderer.h"
#include "Camera.h"
#include "WorldObject.h"
#include "ModelInstance.h"
#include "Light.h"
#include "Font.h"
#include <DXGIFormat.h>
#include "debug\DebugConsole.h"
#include "RdrContextD3D11.h"
#include "RdrPostProcessEffects.h"
#include "RdrShaderConstants.h"
#include "RdrScratchMem.h"
#include "RdrDrawOp.h"
#include "Scene.h"

#define ENABLE_DRAWOP_VALIDATION 1

namespace
{
	static bool s_wireframe = 0;
	static int s_msaaLevel = 2;

	void setWireframeEnabled(DebugCommandArg *args, int numArgs)
	{
		s_wireframe = (args[0].val.num != 0.f);
	}

	enum class RdrPsResourceSlots
	{
		ShadowMaps = 14,
		ShadowCubeMaps = 15,
		LightList = 16,
		TileLightIds = 17,
		ShadowMapData = 18,
	};

	enum class RdrPsSamplerSlots
	{
		ShadowMap = 15,
	};

	// Pass to bucket mappings
	RdrBucketType s_passBuckets[] = {
		RdrBucketType::Opaque,	     // RdrPass::ZPrepass
		RdrBucketType::LightCulling, // RdrPass::LightCulling
		RdrBucketType::Opaque,	     // RdrPass::Opaque
		RdrBucketType::Sky,	         // RdrPass::Sky
		RdrBucketType::Alpha,		 // RdrPass::Alpha
		RdrBucketType::UI,		     // RdrPass::UI
	};
	static_assert(sizeof(s_passBuckets) / sizeof(s_passBuckets[0]) == (int)RdrPass::Count, "Missing pass -> bucket mappings");

	// Event names for passes
	static const wchar_t* s_passNames[] =
	{
		L"Z-Prepass",		// RdrPass::ZPrepass
		L"Light Culling",	// RdrPass::LightCulling
		L"Opaque",			// RdrPass::Opaque
		L"Sky",				// RdrPass::Sky
		L"Alpha",			// RdrPass::Alpha
		L"UI",				// RdrPass::UI
	};
	static_assert(sizeof(s_passNames) / sizeof(s_passNames[0]) == (int)RdrPass::Count, "Missing RdrPass names!");


	void validateDrawOp(RdrDrawOp* pDrawOp)
	{
		if (pDrawOp->eType == RdrDrawOpType::Graphics)
		{
			assert(pDrawOp->graphics.hGeo);
		}
	}

	void cullSceneToCamera(const Camera& rCamera, const Scene& rScene, RdrDrawOpBucket& rOpaqueBucket, RdrDrawOpBucket& rAlphaBucket, RdrDrawOpBucket& rSkyBucket)
	{
		rOpaqueBucket.clear();
		rAlphaBucket.clear();
		rSkyBucket.clear();

		// World Objects
		const WorldObjectList& objects = rScene.GetWorldObjects();
		for (int i = (int)objects.size() - 1; i >= 0; --i)
		{
			const WorldObject* pObj = objects[i];
			if (!rCamera.CanSee(pObj->GetPosition(), pObj->GetRadius()))
				continue;
			
			const ModelInstance* pModel = pObj->GetModel();
			const RdrDrawOp** apDrawOps = pModel->GetDrawOps();
			uint numDrawOps = pModel->GetNumDrawOps();

			for (uint k = 0; k < numDrawOps; ++k)
			{
				const RdrDrawOp* pDrawOp = apDrawOps[k];
				if (pDrawOp)
				{
					if (pDrawOp->graphics.bHasAlpha)
					{
						rAlphaBucket.push_back(pDrawOp);
					}
					else if (pDrawOp->graphics.bIsSky)
					{
						rSkyBucket.push_back(pDrawOp);
					}
					else
					{
						rOpaqueBucket.push_back(pDrawOp);
					}
				}
			}
		}

		// Sky
		{
			const RdrDrawOp** apDrawOps = rScene.GetSky().GetDrawOps();
			uint numDrawOps = rScene.GetSky().GetNumDrawOps();

			for (uint k = 0; k < numDrawOps; ++k)
			{
				const RdrDrawOp* pDrawOp = apDrawOps[k];
				if (pDrawOp)
				{
					if (pDrawOp->graphics.bHasAlpha)
					{
						rAlphaBucket.push_back(pDrawOp);
					}
					else if (pDrawOp->graphics.bIsSky)
					{
						rSkyBucket.push_back(pDrawOp);
					}
					else
					{
						rOpaqueBucket.push_back(pDrawOp);
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////

bool Renderer::Init(HWND hWnd, int width, int height)
{
	DebugConsole::RegisterCommand("wireframe", setWireframeEnabled, DebugCommandArgType::Number);

	m_pContext = new RdrContextD3D11();
	if (!m_pContext->Init(hWnd, width, height))
		return false;

	RdrResourceSystem::Init();
	RdrShaderSystem::Init(m_pContext);

	Resize(width, height);
	ApplyDeviceChanges();

	Font::Init();
	m_postProcess.Init();

	return true;
}

void Renderer::Cleanup()
{
	// todo: flip frame to finish resource frees.

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
		m_pContext->Resize(m_pendingViewWidth, m_pendingViewHeight);

		// Release existing resources
		if (m_hPrimaryDepthStencilView)
			RdrResourceSystem::ReleaseDepthStencilView(m_hPrimaryDepthStencilView);
		if (m_hPrimaryDepthBuffer)
			RdrResourceSystem::ReleaseResource(m_hPrimaryDepthBuffer);
		if (m_hColorBuffer)
			RdrResourceSystem::ReleaseResource(m_hColorBuffer);
		if (m_hColorBufferMultisampled)
			RdrResourceSystem::ReleaseResource(m_hColorBufferMultisampled);
		if (m_hColorBufferRenderTarget)
			RdrResourceSystem::ReleaseRenderTargetView(m_hColorBufferRenderTarget);

		// FP16 color
		m_hColorBuffer = RdrResourceSystem::CreateTexture2D(m_pendingViewWidth, m_pendingViewHeight, RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
		if (s_msaaLevel > 1)
		{
			m_hColorBufferMultisampled = RdrResourceSystem::CreateTexture2DMS(m_pendingViewWidth, m_pendingViewHeight, RdrResourceFormat::R16G16B16A16_FLOAT, s_msaaLevel);
			m_hColorBufferRenderTarget = RdrResourceSystem::CreateRenderTargetView(m_hColorBufferMultisampled);
		}
		else
		{
			m_hColorBufferMultisampled = 0;
			m_hColorBufferRenderTarget = RdrResourceSystem::CreateRenderTargetView(m_hColorBuffer);
		}

		// Depth Buffer
		m_hPrimaryDepthBuffer = RdrResourceSystem::CreateTexture2DMS(m_pendingViewWidth, m_pendingViewHeight, RdrResourceFormat::D24_UNORM_S8_UINT, s_msaaLevel);
		m_hPrimaryDepthStencilView = RdrResourceSystem::CreateDepthStencilView(m_hPrimaryDepthBuffer);

		m_viewWidth = m_pendingViewWidth;
		m_viewHeight = m_pendingViewHeight;

		m_postProcess.HandleResize(m_viewWidth, m_viewHeight);
	}
}

const Camera& Renderer::GetCurrentCamera(void) const
{
	return m_pCurrentAction->camera;
}

RdrAction* Renderer::GetNextAction()
{
	RdrFrameState& state = GetQueueState();
	assert(state.numActions < MAX_ACTIONS_PER_FRAME);
	return &state.actions[state.numActions++];
}

void Renderer::QueueLightCulling()
{
	const Camera& rCamera = m_pCurrentAction->camera;

	const int kTilePixelSize = 16;
	int tileCountX = (m_viewWidth + (kTilePixelSize-1)) / kTilePixelSize;
	int tileCountY = (m_viewHeight + (kTilePixelSize - 1)) / kTilePixelSize;
	bool tileCountChanged = false;

	if (m_tileCountX != tileCountX || m_tileCountY != tileCountY)
	{
		m_tileCountX = tileCountX;
		m_tileCountY = tileCountY;
		tileCountChanged = true;
	}

	//////////////////////////////////////
	// Depth min max
	if ( tileCountChanged )
	{
		if (m_hDepthMinMaxTex)
			RdrResourceSystem::ReleaseResource(m_hDepthMinMaxTex);
		m_hDepthMinMaxTex = RdrResourceSystem::CreateTexture2D(tileCountX, tileCountY, RdrResourceFormat::R16G16_FLOAT, RdrResourceUsage::Default);
	}

	RdrDrawOp* pDepthOp = RdrDrawOp::Allocate();
	pDepthOp->eType = RdrDrawOpType::Compute;
	pDepthOp->compute.shader = RdrComputeShader::TiledDepthMinMax;
	pDepthOp->compute.threads[0] = tileCountX;
	pDepthOp->compute.threads[1] = tileCountY;
	pDepthOp->compute.threads[2] = 1;
	pDepthOp->compute.hViews[0] = m_hDepthMinMaxTex;
	pDepthOp->compute.viewCount = 1;
	pDepthOp->compute.hTextures[0] = m_hPrimaryDepthBuffer;
	pDepthOp->compute.texCount = 1;

	Matrix44 viewMtx;
	Matrix44 invProjMtx;
	rCamera.GetMatrices(viewMtx, invProjMtx);
	invProjMtx = Matrix44Inverse(invProjMtx);
	invProjMtx = Matrix44Transpose(invProjMtx);

	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrScratchMem::AllocAligned(constantsSize, 16);
	for (int i = 0; i < 4; ++i)
	{
		pConstants[i].x = invProjMtx.m[i][0];
		pConstants[i].y = invProjMtx.m[i][1];
		pConstants[i].z = invProjMtx.m[i][2];
		pConstants[i].w = invProjMtx.m[i][3];
	}
	if (m_hDepthMinMaxConstants)
	{
		RdrResourceSystem::UpdateConstantBuffer(m_hDepthMinMaxConstants, pConstants);
	}
	else
	{
		m_hDepthMinMaxConstants = RdrResourceSystem::CreateConstantBuffer(pConstants, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}
	pDepthOp->compute.hCsConstants = m_hDepthMinMaxConstants;

	AddToBucket(pDepthOp, RdrBucketType::LightCulling);

	//////////////////////////////////////
	// Light culling
	if ( tileCountChanged )
	{
		const uint kMaxLightsPerTile = 128; // Sync with MAX_LIGHTS_PER_TILE in "light_inc.hlsli"
		if (m_hTileLightIndices)
			RdrResourceSystem::ReleaseResource(m_hTileLightIndices);
		m_hTileLightIndices = RdrResourceSystem::CreateStructuredBuffer(nullptr, tileCountX * tileCountY * kMaxLightsPerTile, sizeof(uint), RdrResourceUsage::Default);
	}

	RdrDrawOp* pCullOp = RdrDrawOp::Allocate();
	pCullOp->eType = RdrDrawOpType::Compute;
	pCullOp->compute.shader = RdrComputeShader::TiledLightCull;
	pCullOp->compute.threads[0] = tileCountX;
	pCullOp->compute.threads[1] = tileCountY;
	pCullOp->compute.threads[2] = 1;
	pCullOp->compute.hViews[0] = m_hTileLightIndices;
	pCullOp->compute.viewCount = 1;
	pCullOp->compute.hTextures[0] = m_pCurrentAction->lightParams.hLightListRes;
	pCullOp->compute.hTextures[1] = m_hDepthMinMaxTex;
	pCullOp->compute.texCount = 2;


	struct CullingParams // Sync with c_tiledlight_cull.hlsl
	{
		Vec3 cameraPos;
		float fovY;

		Vec3 cameraDir;
		float aspectRatio;

		float cameraNearDist;
		float cameraFarDist;
		Vec2 screenSize;

		uint lightCount;
		uint tileCountX;
		uint tileCountY;
	};

	constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(CullingParams));
	CullingParams* pParams = (CullingParams*)RdrScratchMem::AllocAligned(constantsSize, 16);
	pParams->cameraPos = rCamera.GetPosition();
	pParams->cameraDir = rCamera.GetDirection();
	pParams->cameraNearDist = rCamera.GetNearDist();
	pParams->cameraFarDist = rCamera.GetFarDist();
	pParams->screenSize = GetViewportSize();
	pParams->fovY = rCamera.GetFieldOfViewY();
	pParams->aspectRatio = rCamera.GetAspectRatio();
	pParams->lightCount = m_pCurrentAction->lightParams.lightCount;
	pParams->tileCountX = tileCountX;
	pParams->tileCountY = tileCountY;

	if (m_hTileCullConstants)
	{
		RdrResourceSystem::UpdateConstantBuffer(m_hTileCullConstants, pParams);
	}
	else
	{
		m_hTileCullConstants = RdrResourceSystem::CreateConstantBuffer(pParams, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}
	pCullOp->compute.hCsConstants = m_hTileCullConstants;

	AddToBucket(pCullOp, RdrBucketType::LightCulling);
}

void Renderer::BeginShadowMapAction(const Camera& rCamera, const Scene& rScene, RdrDepthStencilViewHandle hDepthView, Rect& viewport)
{
	assert(m_pCurrentAction == nullptr);

	m_pCurrentAction = GetNextAction();
	m_pCurrentAction->name = L"Shadow Map";
	m_pCurrentAction->primaryViewport = viewport;
	m_pCurrentAction->pScene = &rScene;

	m_pCurrentAction->camera = rCamera;
	m_pCurrentAction->camera.UpdateFrustum();

	// Setup action passes
	RdrPassData& rPass = m_pCurrentAction->passes[(int)RdrPass::ZPrepass];
	{
		rPass.viewport = viewport;
		rPass.bEnabled = true;
		rPass.hDepthTarget = hDepthView;
		rPass.bClearDepthTarget = true;
		rPass.depthTestMode = RdrDepthTestMode::Less;
		rPass.bAlphaBlend = false;
		rPass.shaderMode = RdrShaderMode::DepthOnly;
	}

	cullSceneToCamera(m_pCurrentAction->camera, rScene,
		m_pCurrentAction->buckets[(int)RdrBucketType::Opaque],
		m_pCurrentAction->buckets[(int)RdrBucketType::Alpha],
		m_pCurrentAction->buckets[(int)RdrBucketType::Sky]);

	CreatePerActionConstants();
}

void Renderer::BeginShadowCubeMapAction(const Light* pLight, const Scene& rScene, RdrDepthStencilViewHandle hDepthView, Rect& viewport)
{
	assert(m_pCurrentAction == nullptr);

	float viewDist = pLight->radius * 2.f;

	m_pCurrentAction = GetNextAction();
	m_pCurrentAction->name = L"Shadow Cube Map";
	m_pCurrentAction->bIsCubemapCapture = true;
	m_pCurrentAction->primaryViewport = viewport;
	m_pCurrentAction->pScene = &rScene;

	m_pCurrentAction->camera.SetAsSphere(pLight->position, 0.1f, viewDist);
	m_pCurrentAction->camera.UpdateFrustum();

	// Setup action passes
	RdrPassData& rPass = m_pCurrentAction->passes[(int)RdrPass::ZPrepass];
	{
		rPass.viewport = viewport;
		rPass.bEnabled = true;
		rPass.bAlphaBlend = false;
		rPass.shaderMode = RdrShaderMode::DepthOnly;
		rPass.depthTestMode = RdrDepthTestMode::Less;
		rPass.bClearDepthTarget = true;
		rPass.hDepthTarget = hDepthView;
	}

	cullSceneToCamera(m_pCurrentAction->camera, rScene,
		m_pCurrentAction->buckets[(int)RdrBucketType::Opaque],
		m_pCurrentAction->buckets[(int)RdrBucketType::Alpha],
		m_pCurrentAction->buckets[(int)RdrBucketType::Sky]);

	CreatePerActionConstants();
	CreateCubemapCaptureConstants(pLight->position, 0.1f, pLight->radius * 2.f);
}

void Renderer::BeginPrimaryAction(const Camera& rCamera, const Scene& rScene)
{
	assert(m_pCurrentAction == nullptr);

	Rect viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);;

	const LightList* pLights = rScene.GetLightList();

	m_pCurrentAction = GetNextAction();
	m_pCurrentAction->name = L"Primary Action";
	m_pCurrentAction->lightParams.hLightListRes = pLights->GetLightListRes();
	m_pCurrentAction->lightParams.hShadowMapDataRes = pLights->GetShadowMapDataRes();
	m_pCurrentAction->lightParams.hShadowCubeMapTexArray = pLights->GetShadowCubeMapTexArray();
	m_pCurrentAction->lightParams.hShadowMapTexArray = pLights->GetShadowMapTexArray();
	m_pCurrentAction->lightParams.lightCount = pLights->GetLightCount();

	m_pCurrentAction->pScene = &rScene;

	m_pCurrentAction->camera = rCamera;
	m_pCurrentAction->camera.SetAspectRatio(m_viewWidth / (float)m_viewHeight);
	m_pCurrentAction->camera.UpdateFrustum();

	m_pCurrentAction->primaryViewport = viewport;

	m_pCurrentAction->pPostProcEffects = rScene.GetPostProcEffects();

	// Z Prepass
	RdrPassData* pPass = &m_pCurrentAction->passes[(int)RdrPass::ZPrepass];
	{
		pPass->viewport = viewport;
		pPass->bEnabled = true;
		pPass->bAlphaBlend = false;
		pPass->hDepthTarget = m_hPrimaryDepthStencilView;
		pPass->bClearDepthTarget = true;
		pPass->depthTestMode = RdrDepthTestMode::Less;
		pPass->shaderMode = RdrShaderMode::DepthOnly;
	}

	// Light Culling
	pPass = &m_pCurrentAction->passes[(int)RdrPass::LightCulling];
	{
		pPass->viewport = viewport;
		pPass->bEnabled = true;
		pPass->bAlphaBlend = false;
		pPass->bClearDepthTarget = false;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Opaque
	pPass = &m_pCurrentAction->passes[(int)RdrPass::Opaque];
	{
		pPass->viewport = viewport;
		pPass->bEnabled = true;
		pPass->ahRenderTargets[0] = m_hColorBufferRenderTarget;
		pPass->bClearRenderTargets = true;
		pPass->bAlphaBlend = false;
		pPass->hDepthTarget = m_hPrimaryDepthStencilView;
		pPass->depthTestMode = RdrDepthTestMode::Equal;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Sky
	pPass = &m_pCurrentAction->passes[(int)RdrPass::Sky];
	{
		pPass->viewport = viewport;
		pPass->bEnabled = true;
		pPass->ahRenderTargets[0] = m_hColorBufferRenderTarget;
		pPass->hDepthTarget = m_hPrimaryDepthStencilView;
		pPass->depthTestMode = RdrDepthTestMode::Equal;
		pPass->shaderMode = RdrShaderMode::Normal;
	}

	// Alpha
	pPass = &m_pCurrentAction->passes[(int)RdrPass::Alpha];
	{
		pPass->viewport = viewport;
		pPass->bEnabled = true;
		pPass->ahRenderTargets[0] = m_hColorBufferRenderTarget;
		pPass->hDepthTarget = m_hPrimaryDepthStencilView;
		pPass->depthTestMode = RdrDepthTestMode::Equal;
		pPass->shaderMode = RdrShaderMode::Normal;
		pPass->bAlphaBlend = true;
	}

	m_pCurrentAction->bEnablePostProcessing = true;

	// UI
	pPass = &m_pCurrentAction->passes[(int)RdrPass::UI];
	{
		pPass->viewport = viewport;
		pPass->bEnabled = true;
		pPass->ahRenderTargets[0] = RdrResourceSystem::kPrimaryRenderTargetHandle;
		pPass->bAlphaBlend = true;
		pPass->depthTestMode = RdrDepthTestMode::None;
		pPass->shaderMode = RdrShaderMode::Normal;
	}
	
	cullSceneToCamera(m_pCurrentAction->camera, rScene, 
		m_pCurrentAction->buckets[(int)RdrBucketType::Opaque], 
		m_pCurrentAction->buckets[(int)RdrBucketType::Alpha], 
		m_pCurrentAction->buckets[(int)RdrBucketType::Sky]);

	CreatePerActionConstants();
	QueueLightCulling();
}

void Renderer::EndAction()
{
	m_pCurrentAction = nullptr;
}

void Renderer::AddToBucket(RdrDrawOp* pDrawOp, RdrBucketType eBucket)
{
#if ENABLE_DRAWOP_VALIDATION
	validateDrawOp(pDrawOp);
#endif
	m_pCurrentAction->buckets[(int)eBucket].push_back(pDrawOp);
}

void Renderer::CreateCubemapCaptureConstants(const Vec3& position, const float nearDist, const float farDist)
{
	Camera cam;
	Matrix44 mtxView, mtxProj;

	uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(GsCubemapPerAction));
	GsCubemapPerAction* pGsConstants = (GsCubemapPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);
	for (uint f = 0; f < (uint)CubemapFace::Count; ++f)
	{
		cam.SetAsCubemapFace(position, (CubemapFace)f, nearDist, farDist);
		cam.GetMatrices(mtxView, mtxProj);
		pGsConstants->mtxViewProj[f] = Matrix44Multiply(mtxView, mtxProj);
		pGsConstants->mtxViewProj[f] = Matrix44Transpose(pGsConstants->mtxViewProj[f]);
	}

	m_pCurrentAction->hPerActionCubemapGs = RdrResourceSystem::CreateTempConstantBuffer(pGsConstants, constantsSize);
}

void Renderer::CreatePerActionConstants()
{
	const Camera& rCamera = m_pCurrentAction->camera;

	Matrix44 mtxView;
	Matrix44 mtxProj;

	// Per-action
	{
		rCamera.GetMatrices(mtxView, mtxProj);

		// VS
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(VsPerAction));
		VsPerAction* pVsPerAction = (VsPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);
		pVsPerAction->cameraPosition = rCamera.GetPosition();

		m_pCurrentAction->hPerActionVs = RdrResourceSystem::CreateTempConstantBuffer(pVsPerAction, constantsSize);

		// PS
		constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(PsPerAction));
		PsPerAction* pPsPerAction = (PsPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);

		pPsPerAction->cameraPos = rCamera.GetPosition();
		pPsPerAction->cameraDir = rCamera.GetDirection();
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->viewWidth = m_viewWidth;

		m_pCurrentAction->hPerActionPs = RdrResourceSystem::CreateTempConstantBuffer(pPsPerAction, constantsSize);
	}

	// Ui per-action
	if ( m_pCurrentAction->passes[(int)RdrPass::UI].bEnabled)
	{
		mtxView = Matrix44::kIdentity;
		mtxProj = DirectX::XMMatrixOrthographicLH((float)m_viewWidth, (float)m_viewHeight, 0.01f, 1000.f);

		// VS
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(VsPerAction));
		VsPerAction* pVsPerAction = (VsPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);
		pVsPerAction->cameraPosition = Vec3::kZero;

		m_pCurrentAction->hUiPerActionVs = RdrResourceSystem::CreateTempConstantBuffer(pVsPerAction, constantsSize);
	
		// PS
		constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(PsPerAction));
		PsPerAction* pPsPerAction = (PsPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);
		
		pPsPerAction->cameraPos = Vec3::kOrigin;
		pPsPerAction->cameraDir = Vec3::kUnitZ;
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->viewWidth = m_viewWidth;

		m_pCurrentAction->hUiPerActionPs = RdrResourceSystem::CreateTempConstantBuffer(pPsPerAction, constantsSize);
	}
}

void Renderer::DrawPass(const RdrAction& rAction, RdrPass ePass)
{
	const RdrPassData& rPass = rAction.passes[(int)ePass];

	if (!rPass.bEnabled)
		return;

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
	m_pContext->SetDepthStencilState(rPass.depthTestMode);
	m_pContext->SetBlendState(rPass.bAlphaBlend);

	m_pContext->SetViewport(rPass.viewport);

	RdrDrawOpBucket::const_iterator opIter = rAction.buckets[(int)s_passBuckets[(int)ePass] ].begin();
	RdrDrawOpBucket::const_iterator opEndIter = rAction.buckets[(int)s_passBuckets[(int)ePass] ].end();
	for ( ; opIter != opEndIter; ++opIter )
	{
		const RdrDrawOp* pDrawOp = *opIter;
		if (pDrawOp->eType == RdrDrawOpType::Compute)
		{
			DispatchCompute(pDrawOp);
		}
		else
		{
			DrawGeo(rAction, ePass, pDrawOp, rAction.lightParams, m_hTileLightIndices);
		}
	}

	m_pContext->EndEvent();
}

void Renderer::PostFrameSync()
{
	RdrFrameState& rActiveState = GetActiveState();
	assert(!m_pCurrentAction);

	// Return old frame actions to the pool.
	for (uint iAction = 0; iAction < rActiveState.numActions; ++iAction)
	{
		RdrAction& rAction = rActiveState.actions[iAction];

		// Free draw ops.
		for (uint iBucket = 0; iBucket < (int)RdrBucketType::Count; ++iBucket)
		{
			uint numOps = (uint)rAction.buckets[iBucket].size();
			RdrDrawOpBucket::iterator iter = rAction.buckets[iBucket].begin();
			RdrDrawOpBucket::iterator endIter = rAction.buckets[iBucket].end();
			for (; iter != endIter; ++iter)
			{
				const RdrDrawOp* pDrawOp = *iter;
				if (pDrawOp->eType == RdrDrawOpType::Graphics && pDrawOp->graphics.bFreeGeo)
				{
					RdrGeoSystem::ReleaseGeo(pDrawOp->graphics.hGeo);
				}
				
				if (pDrawOp->eType == RdrDrawOpType::Compute || pDrawOp->graphics.bTempDrawOp)
				{
					RdrDrawOp::QueueRelease(pDrawOp);
				}
			}
			rAction.buckets[iBucket].clear();
		}

		rAction.Reset();
	}

	// Clear remaining state data.
	rActiveState.numActions = 0;

	// Swap state
	m_queueState = !m_queueState;
	RdrResourceSystem::FlipState(m_pContext);
	RdrGeoSystem::FlipState();
	RdrShaderSystem::FlipState();
	RdrScratchMem::FlipState();
	RdrDrawOp::ProcessReleases();
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

	RdrShaderSystem::ProcessCommands(m_pContext);
	RdrResourceSystem::ProcessCommands(m_pContext);
	RdrGeoSystem::ProcessCommands(m_pContext);

	ProcessReadbackRequests();

	if (!m_pContext->IsIdle()) // If the device is idle (probably minimized), don't bother rendering anything.
	{
		for (uint iAction = 0; iAction < rFrameState.numActions; ++iAction)
		{
			RdrAction& rAction = rFrameState.actions[iAction];
			m_pContext->BeginEvent(rAction.name);

			RdrRasterState rasterState;
			rasterState.bEnableMSAA = (s_msaaLevel > 1);
			rasterState.bEnableScissor = false;
			rasterState.bWireframe = s_wireframe;
			m_pContext->SetRasterState(rasterState);

			// todo: sort buckets 
			//std::sort(pAction->buckets[RdrBucketType::Opaque].begin(), pAction->buckets[RdrBucketType::Opaque].end(), );

			DrawPass(rAction, RdrPass::ZPrepass);
			DrawPass(rAction, RdrPass::LightCulling);
			DrawPass(rAction, RdrPass::Opaque);
			DrawPass(rAction, RdrPass::Sky);
			DrawPass(rAction, RdrPass::Alpha);

			if (s_wireframe)
			{
				rasterState.bWireframe = false;
				m_pContext->SetRasterState(rasterState);
			}

			// Resolve multisampled color buffer.
			const RdrResource* pColorBuffer = RdrResourceSystem::GetResource(m_hColorBuffer);
			if (s_msaaLevel > 1)
			{
				const RdrResource* pColorBufferMultisampled = RdrResourceSystem::GetResource(m_hColorBufferMultisampled);
				m_pContext->ResolveSurface(*pColorBufferMultisampled, *pColorBuffer);

				rasterState.bEnableMSAA = false;
				m_pContext->SetRasterState(rasterState);
			}

			if (rAction.bEnablePostProcessing)
				m_postProcess.DoPostProcessing(m_pContext, m_drawState, pColorBuffer, *rAction.pPostProcEffects);

			DrawPass(rAction, RdrPass::UI);

			m_pContext->EndEvent();
		}
	}

	m_pContext->Present();
}

void Renderer::DrawGeo(const RdrAction& rAction, const RdrPass ePass, const RdrDrawOp* pDrawOp, const RdrLightParams& rLightParams, const RdrResourceHandle hTileLightIndices)
{
	const RdrPassData& rPass = rAction.passes[(int)ePass];
	bool bDepthOnly = (rPass.shaderMode == RdrShaderMode::DepthOnly);
	const RdrGeometry* pGeo = RdrGeoSystem::GetGeo(pDrawOp->graphics.hGeo);
	RdrShaderFlags shaderFlags = RdrShaderFlags::None;

	// Vertex shader
	RdrVertexShader vertexShader = pDrawOp->graphics.vertexShader;
	if (bDepthOnly)
	{
		shaderFlags |= RdrShaderFlags::DepthOnly;
	}
	vertexShader.flags |= shaderFlags;

	m_drawState.pVertexShader = RdrShaderSystem::GetVertexShader(vertexShader);

	RdrConstantBufferHandle hPerActionVs = (ePass == RdrPass::UI) ? rAction.hUiPerActionVs : rAction.hPerActionVs;
	m_drawState.vsConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(hPerActionVs)->bufferObj;
	if (pDrawOp->graphics.hVsConstants)
	{
		m_drawState.vsConstantBuffers[1] = RdrResourceSystem::GetConstantBuffer(pDrawOp->graphics.hVsConstants)->bufferObj;
		m_drawState.vsConstantBufferCount = 2;
	}
	else
	{
		m_drawState.vsConstantBufferCount = 1;
	}

	// Geom shader
	if (rAction.bIsCubemapCapture)
	{
		RdrGeometryShader geomShader = { RdrGeometryShaderType::Model_CubemapCapture, shaderFlags };
		m_drawState.pGeometryShader = RdrShaderSystem::GetGeometryShader(geomShader);
		m_drawState.gsConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rAction.hPerActionCubemapGs)->bufferObj;
		m_drawState.gsConstantBufferCount = 1;
	}

	// Pixel shader
	const RdrMaterial* pMaterial = pDrawOp->graphics.pMaterial;
	if (pMaterial)
	{
		m_drawState.pPixelShader = pMaterial->hPixelShaders[(int)rPass.shaderMode] ?
			RdrShaderSystem::GetPixelShader(pMaterial->hPixelShaders[(int)rPass.shaderMode]) :
			nullptr;
		if (m_drawState.pPixelShader)
		{
			for (uint i = 0; i < pMaterial->texCount; ++i)
			{
				m_drawState.psResources[i] = RdrResourceSystem::GetResource(pMaterial->hTextures[i])->resourceView;
				m_drawState.psSamplers[i] = pMaterial->samplers[i];
			}

			if (pMaterial->bNeedsLighting && !bDepthOnly)
			{
				m_drawState.psResources[(int)RdrPsResourceSlots::LightList] = RdrResourceSystem::GetResource(rLightParams.hLightListRes)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::TileLightIds] = RdrResourceSystem::GetResource(hTileLightIndices)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::ShadowMaps] = RdrResourceSystem::GetResource(rLightParams.hShadowMapTexArray)->resourceView;
				m_drawState.psResources[(int)RdrPsResourceSlots::ShadowCubeMaps] = RdrResourceSystem::GetResource(rLightParams.hShadowCubeMapTexArray)->resourceView;
				m_drawState.psSamplers[(int)RdrPsSamplerSlots::ShadowMap] = RdrSamplerState(RdrComparisonFunc::LessEqual, RdrTexCoordMode::Clamp, false);
				m_drawState.psResources[(int)RdrPsResourceSlots::ShadowMapData] = RdrResourceSystem::GetResource(rLightParams.hShadowMapDataRes)->resourceView;
			}

			RdrConstantBufferHandle hPerActionPs = (ePass == RdrPass::UI) ? rAction.hUiPerActionPs : rAction.hPerActionPs;
			m_drawState.psConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rAction.hPerActionPs)->bufferObj;
			m_drawState.psConstantBufferCount = 1;
		}
	}

	// Input assembly
	m_drawState.inputLayout = *RdrShaderSystem::GetInputLayout(pDrawOp->graphics.hInputLayout); // todo: layouts per flags
	m_drawState.eTopology = RdrTopology::TriangleList;

	m_drawState.vertexBuffers[0] = pGeo->vertexBuffer;
	m_drawState.vertexStride = pGeo->geoInfo.vertStride;
	m_drawState.vertexOffset = 0;
	m_drawState.vertexCount = pGeo->geoInfo.numVerts;

	if (pGeo->indexBuffer.pBuffer)
	{
		m_drawState.indexBuffer = pGeo->indexBuffer;
		m_drawState.indexCount = pGeo->geoInfo.numIndices;
	}
	else
	{
		m_drawState.indexBuffer.pBuffer = nullptr;
	}

	// Done
	m_pContext->Draw(m_drawState);

	m_drawState.Reset();
}

void Renderer::DispatchCompute(const RdrDrawOp* pDrawOp)
{
	m_drawState.pComputeShader = RdrShaderSystem::GetComputeShader(pDrawOp->compute.shader);

	m_drawState.csConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(pDrawOp->compute.hCsConstants)->bufferObj;
	m_drawState.csConstantBufferCount = 1;

	for (uint i = 0; i < pDrawOp->compute.texCount; ++i)
	{
		if (pDrawOp->compute.hTextures[i])
			m_drawState.csResources[i] = RdrResourceSystem::GetResource(pDrawOp->compute.hTextures[i])->resourceView;
		else
			m_drawState.csResources[i].pTypeless = nullptr;
	}

	for (uint i = 0; i < pDrawOp->compute.viewCount; ++i)
	{
		if (pDrawOp->compute.hViews[i])
			m_drawState.csUavs[i] = RdrResourceSystem::GetResource(pDrawOp->compute.hViews[i])->uav;
		else
			m_drawState.csUavs[i].pTypeless = nullptr;
	}

	m_pContext->DispatchCompute(m_drawState, pDrawOp->compute.threads[0], pDrawOp->compute.threads[1], pDrawOp->compute.threads[2]);

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
	pReq->hDstResource = RdrResourceSystem::CreateTexture2D(1, 1, pSrcResource->texInfo.format, RdrResourceUsage::Staging);
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
	pReq->hDstResource = RdrResourceSystem::CreateStructuredBuffer(nullptr, 1, numBytesToRead, RdrResourceUsage::Staging);
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
	RdrResourceSystem::ReleaseResource(pReq->hDstResource);
	SAFE_DELETE(pReq->pData);

	m_readbackRequests.releaseId(hRequest);
}