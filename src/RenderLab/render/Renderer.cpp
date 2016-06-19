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
#include "../../data/shaders/light_types.h"

#define ENABLE_DRAWOP_VALIDATION 1

namespace
{
	// Hidden global reference to the Renderer for debug commands.
	// There should only ever be one renderer, but I don't want every system to be
	// able to access it through a singleton or other global pattern.
	// This keeps the global ref containined to only this file.
	static Renderer* g_pRenderer = nullptr;

	static bool s_wireframe = 0;
	static int s_msaaLevel = 2;

	void cmdSetWireframeEnabled(DebugCommandArg *args, int numArgs)
	{
		s_wireframe = (args[0].val.num != 0.f);
	}

	void cmdSetLightingMethod(DebugCommandArg *args, int numArgs)
	{
		g_pRenderer->SetLightingMethod((RdrLightingMethod)(int)args[0].val.num);
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

	// Profile sections for passes
	static const RdrProfileSection s_passProfileSections[] =
	{
		RdrProfileSection::ZPrepass,		// RdrPass::ZPrepass
		RdrProfileSection::LightCulling,	// RdrPass::LightCulling
		RdrProfileSection::Opaque,			// RdrPass::Opaque
		RdrProfileSection::Sky,				// RdrPass::Sky
		RdrProfileSection::Alpha,			// RdrPass::Alpha
		RdrProfileSection::UI,				// RdrPass::UI
	};
	static_assert(sizeof(s_passProfileSections) / sizeof(s_passProfileSections[0]) == (int)RdrPass::Count, "Missing RdrPass profile sections!");

	void validateDrawOp(RdrDrawOp* pDrawOp)
	{
		if (pDrawOp->eType == RdrDrawOpType::Graphics)
		{
			assert(pDrawOp->graphics.hGeo);
		}
	}

	void cullSceneToCameraForShadows(const Camera& rCamera, const Scene& rScene, RdrDrawOpBucket& rOpaqueBucket)
	{
		rOpaqueBucket.clear();

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
					if (!pDrawOp->graphics.bHasAlpha && !pDrawOp->graphics.bIsSky)
					{
						rOpaqueBucket.push_back(pDrawOp);
					}
				}
			}
		}
	}

	void cullSceneToCamera(const Camera& rCamera, const Scene& rScene, 
		RdrDrawOpBucket& rOpaqueBucket, RdrDrawOpBucket& rAlphaBucket, RdrDrawOpBucket& rSkyBucket, 
		float& rOutDepthMin, float& rOutDepthMax)
	{
		rOpaqueBucket.clear();
		rAlphaBucket.clear();
		rSkyBucket.clear();

		Vec3 camDir = rCamera.GetDirection();
		Vec3 camPos = rCamera.GetPosition();
		float depthMin = FLT_MAX;
		float depthMax = 0.f;

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
			bool testDepth = true;

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
						testDepth = false;
					}
					else
					{
						rOpaqueBucket.push_back(pDrawOp);
					}
				}
			}

			if (testDepth)
			{
				Vec3 diff = pObj->GetPosition() - camPos;
				float distSqr = Vec3Dot(camDir, diff);
				float dist = sqrtf(max(0.f, distSqr));
				float radius = pObj->GetRadius();

				if (dist -radius < depthMin)
					depthMin = dist - radius;

				if (dist + radius > depthMax)
					depthMax = dist + radius;
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

		rOutDepthMin = max(rCamera.GetNearDist(), depthMin);
		rOutDepthMax = min(rCamera.GetFarDist(), depthMax);
	}

	void createPerActionConstants(const Camera& rCamera, const Rect& rViewport, RdrGlobalConstants& rConstants)
	{
		Matrix44 mtxView;
		Matrix44 mtxProj;

		rCamera.GetMatrices(mtxView, mtxProj);

		// VS
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(VsPerAction));
		VsPerAction* pVsPerAction = (VsPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);
		pVsPerAction->cameraPosition = rCamera.GetPosition();

		rConstants.hVsPerFrame = RdrResourceSystem::CreateTempConstantBuffer(pVsPerAction, constantsSize);

		// PS
		constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(PsPerAction));
		PsPerAction* pPsPerAction = (PsPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);

		pPsPerAction->cameraPos = rCamera.GetPosition();
		pPsPerAction->cameraDir = rCamera.GetDirection();
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->viewSize.x = (uint)rViewport.width;
		pPsPerAction->viewSize.y = (uint)rViewport.height;

		rConstants.hPsPerFrame = RdrResourceSystem::CreateTempConstantBuffer(pPsPerAction, constantsSize);
	}

	void createUiConstants(const Rect& rViewport, RdrGlobalConstants& rConstants)
	{
		Matrix44 mtxView = Matrix44::kIdentity;
		Matrix44 mtxProj = DirectX::XMMatrixOrthographicLH((float)rViewport.width, (float)rViewport.height, 0.01f, 1000.f);

		// VS
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(VsPerAction));
		VsPerAction* pVsPerAction = (VsPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);
		pVsPerAction->cameraPosition = Vec3::kZero;

		rConstants.hVsPerFrame = RdrResourceSystem::CreateTempConstantBuffer(pVsPerAction, constantsSize);

		// PS
		constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(PsPerAction));
		PsPerAction* pPsPerAction = (PsPerAction*)RdrScratchMem::AllocAligned(constantsSize, 16);

		pPsPerAction->cameraPos = Vec3::kOrigin;
		pPsPerAction->cameraDir = Vec3::kUnitZ;
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->viewSize.x = (uint)rViewport.width;
		pPsPerAction->viewSize.y = (uint)rViewport.height;

		rConstants.hPsPerFrame = RdrResourceSystem::CreateTempConstantBuffer(pPsPerAction, constantsSize);
	}

	RdrConstantBufferHandle createCubemapCaptureConstants(const Vec3& position, const float nearDist, const float farDist)
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

		return RdrResourceSystem::CreateTempConstantBuffer(pGsConstants, constantsSize);
	}
}

//////////////////////////////////////////////////////

bool Renderer::Init(HWND hWnd, int width, int height)
{
	assert(g_pRenderer == nullptr);
	g_pRenderer = this;

	DebugConsole::RegisterCommand("wireframe", cmdSetWireframeEnabled, DebugCommandArgType::Number);
	DebugConsole::RegisterCommand("lightingMethod", cmdSetLightingMethod, DebugCommandArgType::Number);

	m_pContext = new RdrContextD3D11();
	if (!m_pContext->Init(hWnd, width, height))
		return false;

	m_profiler.Init(m_pContext);

	RdrResourceSystem::Init();

	// Set default lighting method before initialized shaders so global defines can be applied first.
	m_eLightingMethod = RdrLightingMethod::Clustered;
	SetLightingMethod(m_eLightingMethod);

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

void Renderer::SetLightingMethod(RdrLightingMethod eLightingMethod)
{
	m_ePendingLightingMethod = eLightingMethod;
	RdrShaderSystem::SetGlobalShaderDefine("CLUSTERED_LIGHTING", (eLightingMethod == RdrLightingMethod::Clustered));
}

void Renderer::QueueClusteredLightCulling()
{
	const Camera& rCamera = m_pCurrentAction->camera;

	int clusterCountX = (m_viewWidth + (CLUSTEREDLIGHTING_TILE_SIZE - 1)) / CLUSTEREDLIGHTING_TILE_SIZE;
	int clusterCountY = (m_viewHeight + (CLUSTEREDLIGHTING_TILE_SIZE - 1)) / CLUSTEREDLIGHTING_TILE_SIZE;
	int clusterCountZ = CLUSTEREDLIGHTING_DEPTH_SLICES;
	bool clusterCountChanged = false;

	if (m_clusteredLightData.clusterCountX != clusterCountX || clusterCountY != clusterCountY)
	{
		m_clusteredLightData.clusterCountX = clusterCountX;
		m_clusteredLightData.clusterCountY = clusterCountY;
		clusterCountChanged = true;
	}

	//////////////////////////////////////
	// Light culling
	if (clusterCountChanged)
	{
		if (m_clusteredLightData.hLightIndices)
			RdrResourceSystem::ReleaseResource(m_clusteredLightData.hLightIndices);
		m_clusteredLightData.hLightIndices = RdrResourceSystem::CreateDataBuffer(nullptr, clusterCountX * clusterCountY * clusterCountZ * CLUSTEREDLIGHTING_MAX_LIGHTS_PER, RdrResourceFormat::R16_UINT, RdrResourceUsage::Default);
	}

	RdrDrawOp* pCullOp = RdrDrawOp::Allocate();
	pCullOp->eType = RdrDrawOpType::Compute;
	pCullOp->compute.shader = RdrComputeShader::ClusteredLightCull;
	pCullOp->compute.threads[0] = clusterCountX;
	pCullOp->compute.threads[1] = clusterCountY;
	pCullOp->compute.threads[2] = clusterCountZ;
	pCullOp->compute.hViews[0] = m_clusteredLightData.hLightIndices;
	pCullOp->compute.viewCount = 1;
	pCullOp->compute.hTextures[0] = m_pCurrentAction->lightParams.hLightListRes;
	pCullOp->compute.hTextures[1] = m_tiledLightData.hDepthMinMaxTex;
	pCullOp->compute.texCount = 2;

	uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(ClusteredLightCullingParams));
	ClusteredLightCullingParams* pParams = (ClusteredLightCullingParams*)RdrScratchMem::AllocAligned(constantsSize, 16);
	rCamera.GetViewMatrix(pParams->mtxView);
	pParams->mtxView = Matrix44Transpose(pParams->mtxView);
	pParams->screenSize = GetViewportSize();
	pParams->fovY = rCamera.GetFieldOfViewY();
	pParams->aspectRatio = rCamera.GetAspectRatio();
	pParams->lightCount = m_pCurrentAction->lightParams.lightCount;
	pParams->clusterCountX = clusterCountX;
	pParams->clusterCountY = clusterCountY;
	pParams->clusterCountZ = clusterCountZ;

	if (m_clusteredLightData.hCullConstants)
	{
		RdrResourceSystem::UpdateConstantBuffer(m_clusteredLightData.hCullConstants, pParams);
	}
	else
	{
		m_clusteredLightData.hCullConstants = RdrResourceSystem::CreateConstantBuffer(pParams, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}
	pCullOp->compute.hCsConstants = m_clusteredLightData.hCullConstants;

	AddToBucket(pCullOp, RdrBucketType::LightCulling);
}

void Renderer::QueueTiledLightCulling()
{
	const Camera& rCamera = m_pCurrentAction->camera;

	int tileCountX = (m_viewWidth + (TILEDLIGHTING_TILE_SIZE - 1)) / TILEDLIGHTING_TILE_SIZE;
	int tileCountY = (m_viewHeight + (TILEDLIGHTING_TILE_SIZE - 1)) / TILEDLIGHTING_TILE_SIZE;
	bool tileCountChanged = false;

	if (m_tiledLightData.tileCountX != tileCountX || m_tiledLightData.tileCountY != tileCountY)
	{
		m_tiledLightData.tileCountX = tileCountX;
		m_tiledLightData.tileCountY = tileCountY;
		tileCountChanged = true;
	}

	//////////////////////////////////////
	// Depth min max
	if ( tileCountChanged )
	{
		if (m_tiledLightData.hDepthMinMaxTex)
			RdrResourceSystem::ReleaseResource(m_tiledLightData.hDepthMinMaxTex);
		m_tiledLightData.hDepthMinMaxTex = RdrResourceSystem::CreateTexture2D(tileCountX, tileCountY, RdrResourceFormat::R16G16_FLOAT, RdrResourceUsage::Default);
	}

	RdrDrawOp* pDepthOp = RdrDrawOp::Allocate();
	pDepthOp->eType = RdrDrawOpType::Compute;
	pDepthOp->compute.shader = RdrComputeShader::TiledDepthMinMax;
	pDepthOp->compute.threads[0] = tileCountX;
	pDepthOp->compute.threads[1] = tileCountY;
	pDepthOp->compute.threads[2] = 1;
	pDepthOp->compute.hViews[0] = m_tiledLightData.hDepthMinMaxTex;
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
	if (m_tiledLightData.hDepthMinMaxConstants)
	{
		RdrResourceSystem::UpdateConstantBuffer(m_tiledLightData.hDepthMinMaxConstants, pConstants);
	}
	else
	{
		m_tiledLightData.hDepthMinMaxConstants = RdrResourceSystem::CreateConstantBuffer(pConstants, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}
	pDepthOp->compute.hCsConstants = m_tiledLightData.hDepthMinMaxConstants;

	AddToBucket(pDepthOp, RdrBucketType::LightCulling);

	//////////////////////////////////////
	// Light culling
	if ( tileCountChanged )
	{
		if (m_tiledLightData.hLightIndices)
			RdrResourceSystem::ReleaseResource(m_tiledLightData.hLightIndices);
		m_tiledLightData.hLightIndices = RdrResourceSystem::CreateDataBuffer(nullptr, tileCountX * tileCountY * TILEDLIGHTING_MAX_LIGHTS_PER, RdrResourceFormat::R16_UINT, RdrResourceUsage::Default);
	}

	RdrDrawOp* pCullOp = RdrDrawOp::Allocate();
	pCullOp->eType = RdrDrawOpType::Compute;
	pCullOp->compute.shader = RdrComputeShader::TiledLightCull;
	pCullOp->compute.threads[0] = tileCountX;
	pCullOp->compute.threads[1] = tileCountY;
	pCullOp->compute.threads[2] = 1;
	pCullOp->compute.hViews[0] = m_tiledLightData.hLightIndices;
	pCullOp->compute.viewCount = 1;
	pCullOp->compute.hTextures[0] = m_pCurrentAction->lightParams.hLightListRes;
	pCullOp->compute.hTextures[1] = m_tiledLightData.hDepthMinMaxTex;
	pCullOp->compute.texCount = 2;

	constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(TiledLightCullingParams));
	TiledLightCullingParams* pParams = (TiledLightCullingParams*)RdrScratchMem::AllocAligned(constantsSize, 16);
	rCamera.GetViewMatrix(pParams->mtxView);
	pParams->mtxView = Matrix44Transpose(pParams->mtxView);
	pParams->cameraNearDist = rCamera.GetNearDist();
	pParams->cameraFarDist = rCamera.GetFarDist();
	pParams->screenSize = GetViewportSize();
	pParams->fovY = rCamera.GetFieldOfViewY();
	pParams->aspectRatio = rCamera.GetAspectRatio();
	pParams->lightCount = m_pCurrentAction->lightParams.lightCount;
	pParams->tileCountX = tileCountX;
	pParams->tileCountY = tileCountY;

	if (m_tiledLightData.hCullConstants)
	{
		RdrResourceSystem::UpdateConstantBuffer(m_tiledLightData.hCullConstants, pParams);
	}
	else
	{
		m_tiledLightData.hCullConstants = RdrResourceSystem::CreateConstantBuffer(pParams, constantsSize, RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	}
	pCullOp->compute.hCsConstants = m_tiledLightData.hCullConstants;

	AddToBucket(pCullOp, RdrBucketType::LightCulling);
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
		rPassData.bAlphaBlend = false;
		rPassData.shaderMode = RdrShaderMode::DepthOnly;
	}

	cullSceneToCameraForShadows(rCamera, *m_pCurrentAction->pScene, rShadowPass.drawOps);

	createPerActionConstants(rCamera, viewport, rShadowPass.constants);
}

void Renderer::QueueShadowCubeMapPass(const Light* pLight, RdrDepthStencilViewHandle hDepthView, Rect& viewport)
{
	assert(m_pCurrentAction);
	assert(m_pCurrentAction->shadowPassCount + 1 < MAX_SHADOW_MAPS_PER_FRAME);

	RdrShadowPass& rShadowPass = m_pCurrentAction->shadowPasses[m_pCurrentAction->shadowPassCount];
	m_pCurrentAction->shadowPassCount++;

	float viewDist = pLight->radius * 2.f;
	rShadowPass.camera.SetAsSphere(pLight->position, 0.1f, viewDist);
	rShadowPass.camera.UpdateFrustum();

	// Setup action passes
	RdrPassData& rPassData = rShadowPass.passData;
	{
		rPassData.viewport = viewport;
		rPassData.bEnabled = true;
		rPassData.bAlphaBlend = false;
		rPassData.shaderMode = RdrShaderMode::DepthOnly;
		rPassData.depthTestMode = RdrDepthTestMode::Less;
		rPassData.bClearDepthTarget = true;
		rPassData.hDepthTarget = hDepthView;
		rPassData.bIsCubeMapCapture = true;
	}

	cullSceneToCameraForShadows(rShadowPass.camera, *m_pCurrentAction->pScene, rShadowPass.drawOps);

	createPerActionConstants(rShadowPass.camera, viewport, rShadowPass.constants);
	rShadowPass.constants.hGsCubeMap = createCubemapCaptureConstants(pLight->position, 0.1f, pLight->radius * 2.f);
}

void Renderer::BeginPrimaryAction(const Camera& rCamera, Scene& rScene)
{
	assert(m_pCurrentAction == nullptr);

	Rect viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);;


	m_pCurrentAction = GetNextAction();
	m_pCurrentAction->name = L"Primary Action";

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

	float sceneDepthMin, sceneDepthMax;
	cullSceneToCamera(m_pCurrentAction->camera, rScene, 
		m_pCurrentAction->buckets[(int)RdrBucketType::Opaque], 
		m_pCurrentAction->buckets[(int)RdrBucketType::Alpha], 
		m_pCurrentAction->buckets[(int)RdrBucketType::Sky], 
		sceneDepthMin, sceneDepthMax);

	createPerActionConstants(m_pCurrentAction->camera, viewport, m_pCurrentAction->constants);
	createUiConstants(viewport, m_pCurrentAction->uiConstants);

	// Lighting
	LightList& rLights = rScene.GetLightList();
	rLights.PrepareDraw(*this, m_pCurrentAction->camera, sceneDepthMin, sceneDepthMax);

	m_pCurrentAction->lightParams.hLightListRes = rLights.GetLightListRes();
	m_pCurrentAction->lightParams.hShadowMapDataRes = rLights.GetShadowMapDataRes();
	m_pCurrentAction->lightParams.hShadowCubeMapTexArray = rLights.GetShadowCubeMapTexArray();
	m_pCurrentAction->lightParams.hShadowMapTexArray = rLights.GetShadowMapTexArray();
	m_pCurrentAction->lightParams.lightCount = rLights.GetLightCount();

	if (m_eLightingMethod == RdrLightingMethod::Clustered)
	{
		QueueClusteredLightCulling();
	}
	else
	{
		QueueTiledLightCulling();
	}
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
	m_pContext->SetDepthStencilState(rPass.depthTestMode);
	m_pContext->SetBlendState(rPass.bAlphaBlend);

	m_pContext->SetViewport(rPass.viewport);

	const RdrResourceHandle hLightIndicesBuffer = (m_eLightingMethod == RdrLightingMethod::Clustered) 
		? m_clusteredLightData.hLightIndices 
		: m_tiledLightData.hLightIndices;

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
			const RdrGlobalConstants& rGlobalConstants = (ePass == RdrPass::UI) ? rAction.uiConstants : rAction.constants;
			DrawGeo(rPass, rGlobalConstants, pDrawOp, rAction.lightParams, hLightIndicesBuffer);
		}
	}

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
	m_pContext->SetDepthStencilState(rPassData.depthTestMode);
	m_pContext->SetBlendState(rPassData.bAlphaBlend);

	m_pContext->SetViewport(rPassData.viewport);

	RdrLightParams lightParams;
	RdrDrawOpBucket::const_iterator opIter = rShadowPass.drawOps.begin();
	RdrDrawOpBucket::const_iterator opEndIter = rShadowPass.drawOps.end();
	for (; opIter != opEndIter; ++opIter)
	{
		const RdrDrawOp* pDrawOp = *opIter;
		if (pDrawOp->eType == RdrDrawOpType::Compute)
		{
			DispatchCompute(pDrawOp);
		}
		else
		{
			DrawGeo(rPassData, rShadowPass.constants, pDrawOp, lightParams, 0);
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

	RdrShaderSystem::ProcessCommands(m_pContext);
	RdrResourceSystem::ProcessCommands(m_pContext);
	RdrGeoSystem::ProcessCommands(m_pContext);

	ProcessReadbackRequests();

	if (!m_pContext->IsIdle()) // If the device is idle (probably minimized), don't bother rendering anything.
	{
		m_profiler.BeginFrame();

		for (uint iAction = 0; iAction < rFrameState.numActions; ++iAction)
		{
			RdrAction& rAction = rFrameState.actions[iAction];
			m_pContext->BeginEvent(rAction.name);

			m_profiler.BeginSection(RdrProfileSection::Shadows);
			for (int iShadow = 0; iShadow < rAction.shadowPassCount; ++iShadow)
			{
				DrawShadowPass(rAction.shadowPasses[iShadow]);
			}
			m_profiler.EndSection();

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
			{
				m_profiler.BeginSection(RdrProfileSection::PostProcessing);
				m_postProcess.DoPostProcessing(m_pContext, m_drawState, pColorBuffer, *rAction.pPostProcEffects);
				m_profiler.EndSection();
			}

			DrawPass(rAction, RdrPass::UI);

			m_pContext->EndEvent();
		}

		m_profiler.EndFrame();
	}

	m_pContext->Present();
}

void Renderer::DrawGeo(const RdrPassData& rPass, const RdrGlobalConstants& rGlobalConstants, const RdrDrawOp* pDrawOp, const RdrLightParams& rLightParams, const RdrResourceHandle hTileLightIndices)
{
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

	RdrConstantBufferHandle hPerActionVs = rGlobalConstants.hVsPerFrame;
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
	if (rPass.bIsCubeMapCapture)
	{
		RdrGeometryShader geomShader = { RdrGeometryShaderType::Model_CubemapCapture, shaderFlags };
		m_drawState.pGeometryShader = RdrShaderSystem::GetGeometryShader(geomShader);
		m_drawState.gsConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hGsCubeMap)->bufferObj;
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

			m_drawState.psConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hPsPerFrame)->bufferObj;
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