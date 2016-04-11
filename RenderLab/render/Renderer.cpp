#include "Precompiled.h"
#include "Renderer.h"
#include <assert.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include "Model.h"
#include "Camera.h"
#include "WorldObject.h"
#include "Light.h"
#include "Font.h"
#include <DXGIFormat.h>
#include "debug\DebugConsole.h"
#include "RdrContextD3D11.h"
#include "RdrShaderConstants.h"
#include "RdrTransientMem.h"
#include "RdrDrawOp.h"

namespace
{
	static bool s_wireframe = 0;
	static int s_msaaLevel = 2;

	void setWireframeEnabled(DebugCommandArg *args, int numArgs)
	{
		s_wireframe = (args[0].val.num != 0.f);
	}

	enum RdrReservedPsResourceSlots
	{
		kPsResource_ShadowMaps = 14,
		kPsResource_ShadowCubeMaps = 15,
		kPsResource_LightList = 16,
		kPsResource_TileLightIds = 17,
		kPsResource_ShadowMapData = 18,
	};

	enum RdrReservedPsSamplerSlots
	{
		kPsSampler_ShadowMap = 15,
	};

	// Pass to bucket mappings
	RdrBucketType s_passBuckets[] = {
		kRdrBucketType_Opaque,	     // kRdrPass_ZPrepass
		kRdrBucketType_LightCulling, // kRdrPass_LightCulling
		kRdrBucketType_Opaque,	     // kRdrPass_Opaque
		kRdrBucketType_Sky,	         // kRdrPass_Sky
		kRdrBucketType_Alpha,		 // kRdrPass_Alpha
		kRdrBucketType_UI,		     // kRdrPass_UI
	};
	static_assert(sizeof(s_passBuckets) / sizeof(s_passBuckets[0]) == kRdrPass_Count, "Missing pass -> bucket mappings");

	// Event names for passes
	static const wchar_t* s_passNames[] =
	{
		L"Z-Prepass",		// kRdrPass_ZPrepass
		L"Light Culling",	// kRdrPass_LightCulling
		L"Opaque",			// kRdrPass_Opaque
		L"Sky",				// kRdrPass_Sky
		L"Alpha",			// kRdrPass_Alpha
		L"UI",				// kRdrPass_UI
	};
	static_assert(sizeof(s_passNames) / sizeof(s_passNames[0]) == kRdrPass_Count, "Missing RdrPass names!");
}
//////////////////////////////////////////////////////


bool Renderer::Init(HWND hWnd, int width, int height)
{
	DebugConsole::RegisterCommand("wireframe", setWireframeEnabled, kDebugCommandArgType_Number);

	m_pContext = new RdrContextD3D11();
	if (!m_pContext->Init(hWnd, width, height))
		return false;

	m_assets.shaders.Init(m_pContext);
	m_assets.resources.Init(m_pContext);
	m_assets.geos.Init(m_pContext);

	Resize(width, height);
	ApplyDeviceChanges();

	Font::Init(*this);
	m_postProcess.Init(m_assets);

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
			m_assets.resources.ReleaseDepthStencilView(m_hPrimaryDepthStencilView);
		if (m_hPrimaryDepthBuffer)
			m_assets.resources.ReleaseResource(m_hPrimaryDepthBuffer);
		if (m_hColorBuffer)
			m_assets.resources.ReleaseResource(m_hColorBuffer);
		if (m_hColorBufferMultisampled)
			m_assets.resources.ReleaseResource(m_hColorBufferMultisampled);
		if (m_hColorBufferRenderTarget)
			m_assets.resources.ReleaseRenderTargetView(m_hColorBufferRenderTarget);

		// FP16 color
		m_hColorBuffer = m_assets.resources.CreateTexture2D(m_pendingViewWidth, m_pendingViewHeight, kResourceFormat_R16G16B16A16_FLOAT);
		if (s_msaaLevel > 1)
		{
			m_hColorBufferMultisampled = m_assets.resources.CreateTexture2DMS(m_pendingViewWidth, m_pendingViewHeight, kResourceFormat_R16G16B16A16_FLOAT, s_msaaLevel);
			m_hColorBufferRenderTarget = m_assets.resources.CreateRenderTargetView(m_hColorBufferMultisampled);
		}
		else
		{
			m_hColorBufferMultisampled = 0;
			m_hColorBufferRenderTarget = m_assets.resources.CreateRenderTargetView(m_hColorBuffer);
		}

		// Depth Buffer
		m_hPrimaryDepthBuffer = m_assets.resources.CreateTexture2DMS(m_pendingViewWidth, m_pendingViewHeight, kResourceFormat_D24_UNORM_S8_UINT, s_msaaLevel);
		m_hPrimaryDepthStencilView = m_assets.resources.CreateDepthStencilView(m_hPrimaryDepthBuffer);

		m_viewWidth = m_pendingViewWidth;
		m_viewHeight = m_pendingViewHeight;

		m_postProcess.HandleResize(m_viewWidth, m_viewHeight, m_assets);
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
			m_assets.resources.ReleaseResource(m_hDepthMinMaxTex);
		m_hDepthMinMaxTex = m_assets.resources.CreateTexture2D(tileCountX, tileCountY, kResourceFormat_R16G16_FLOAT);
	}

	RdrDrawOp* pDepthOp = RdrDrawOp::Allocate();
	pDepthOp->eType = kRdrDrawOpType_Compute;
	pDepthOp->compute.shader = kRdrComputeShader_TiledDepthMinMax;
	pDepthOp->compute.threads[0] = tileCountX;
	pDepthOp->compute.threads[1] = tileCountY;
	pDepthOp->compute.threads[2] = 1;
	pDepthOp->compute.hViews[0] = m_hDepthMinMaxTex;
	pDepthOp->compute.viewCount = 1;
	pDepthOp->hTextures[0] = m_hPrimaryDepthBuffer;
	pDepthOp->texCount = 1;

	Matrix44 viewMtx;
	Matrix44 invProjMtx;
	rCamera.GetMatrices(viewMtx, invProjMtx);
	invProjMtx = Matrix44Inverse(invProjMtx);
	invProjMtx = Matrix44Transpose(invProjMtx);

	uint constantsSize = sizeof(Vec4) * 4;
	Vec4* pConstants = (Vec4*)RdrTransientMem::AllocAligned(constantsSize, 16);
	for (int i = 0; i < 4; ++i)
	{
		pConstants[i].x = invProjMtx.m[i][0];
		pConstants[i].y = invProjMtx.m[i][1];
		pConstants[i].z = invProjMtx.m[i][2];
		pConstants[i].w = invProjMtx.m[i][3];
	}
	if (m_hDepthMinMaxConstants)
	{
		m_assets.resources.UpdateConstantBuffer(m_hDepthMinMaxConstants, pConstants);
	}
	else
	{
		m_hDepthMinMaxConstants = m_assets.resources.CreateConstantBuffer(pConstants, constantsSize, kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);
	}
	pDepthOp->compute.hCsConstants = m_hDepthMinMaxConstants;

	AddToBucket(pDepthOp, kRdrBucketType_LightCulling);

	//////////////////////////////////////
	// Light culling
	if ( tileCountChanged )
	{
		const uint kMaxLightsPerTile = 128; // Sync with MAX_LIGHTS_PER_TILE in "light_inc.hlsli"
		if (m_hTileLightIndices)
			m_assets.resources.ReleaseResource(m_hTileLightIndices);
		m_hTileLightIndices = m_assets.resources.CreateStructuredBuffer(nullptr, tileCountX * tileCountY * kMaxLightsPerTile, sizeof(uint), kRdrResourceUsage_Default);
	}

	RdrDrawOp* pCullOp = RdrDrawOp::Allocate();
	pCullOp->eType = kRdrDrawOpType_Compute;
	pCullOp->compute.shader = kRdrComputeShader_TiledLightCull;
	pCullOp->compute.threads[0] = tileCountX;
	pCullOp->compute.threads[1] = tileCountY;
	pCullOp->compute.threads[2] = 1;
	pCullOp->compute.hViews[0] = m_hTileLightIndices;
	pCullOp->compute.viewCount = 1;
	pCullOp->hTextures[0] = m_pCurrentAction->lightParams.hLightListRes;
	pCullOp->hTextures[1] = m_hDepthMinMaxTex;
	pCullOp->texCount = 2;


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
	CullingParams* pParams = (CullingParams*)RdrTransientMem::AllocAligned(constantsSize, 16);
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
		m_assets.resources.UpdateConstantBuffer(m_hTileCullConstants, pParams);
	}
	else
	{
		m_hTileCullConstants = m_assets.resources.CreateConstantBuffer(pParams, constantsSize, kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);
	}
	pCullOp->compute.hCsConstants = m_hTileCullConstants;

	AddToBucket(pCullOp, kRdrBucketType_LightCulling);
}

void Renderer::BeginShadowMapAction(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport)
{
	assert(m_pCurrentAction == nullptr);

	m_pCurrentAction = GetNextAction();
	m_pCurrentAction->name = L"Shadow Map";
	m_pCurrentAction->camera = rCamera;

	// Setup action passes
	RdrPass& rPass = m_pCurrentAction->passes[kRdrPass_ZPrepass];
	{
		rPass.viewport = viewport;
		rPass.bEnabled = true;
		rPass.hDepthTarget = hDepthView;
		rPass.bClearDepthTarget = true;
		rPass.depthTestMode = kRdrDepthTestMode_Less;
		rPass.bAlphaBlend = false;
		rPass.shaderMode = kRdrShaderMode_DepthOnly;
	}

	CreatePerActionConstants();
}

void Renderer::BeginShadowCubeMapAction(const Light* pLight, RdrDepthStencilViewHandle hDepthView, Rect& viewport) // todo: finish
{
	assert(m_pCurrentAction == nullptr);

	float viewDist = pLight->radius * 2.f;

	m_pCurrentAction = GetNextAction();
	m_pCurrentAction->name = L"Shadow Cube Map";
	m_pCurrentAction->camera.SetAsSphere(pLight->position, 0.1f, viewDist);
	m_pCurrentAction->bIsCubemapCapture = true;

	// Setup action passes
	RdrPass& rPass = m_pCurrentAction->passes[kRdrPass_ZPrepass];
	{
		rPass.viewport = viewport;
		rPass.bEnabled = true;
		rPass.bAlphaBlend = false;
		rPass.shaderMode = kRdrShaderMode_DepthOnly;
		rPass.depthTestMode = kRdrDepthTestMode_Less;
		rPass.bClearDepthTarget = true;
		rPass.hDepthTarget = hDepthView;
	}

	CreatePerActionConstants();
	CreateCubemapCaptureConstants(pLight->position, 0.1f, pLight->radius * 2.f);
}

void Renderer::BeginPrimaryAction(const Camera& rCamera, const LightList* pLights)
{
	assert(m_pCurrentAction == nullptr);

	m_pCurrentAction = GetNextAction();
	m_pCurrentAction->name = L"Primary Action";
	m_pCurrentAction->lightParams.hLightListRes = pLights->GetLightListRes();
	m_pCurrentAction->lightParams.hShadowMapDataRes = pLights->GetShadowMapDataRes();
	m_pCurrentAction->lightParams.hShadowCubeMapTexArray = pLights->GetShadowCubeMapTexArray();
	m_pCurrentAction->lightParams.hShadowMapTexArray = pLights->GetShadowMapTexArray();
	m_pCurrentAction->lightParams.lightCount = pLights->GetLightCount();

	m_pCurrentAction->camera = rCamera;
	m_pCurrentAction->camera.SetAspectRatio(m_viewWidth / (float)m_viewHeight);

	// Z Prepass
	RdrPass* pPass = &m_pCurrentAction->passes[kRdrPass_ZPrepass];
	{
		pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
		pPass->bEnabled = true;
		pPass->bAlphaBlend = false;
		pPass->hDepthTarget = m_hPrimaryDepthStencilView;
		pPass->bClearDepthTarget = true;
		pPass->depthTestMode = kRdrDepthTestMode_Less;
		pPass->shaderMode = kRdrShaderMode_DepthOnly;
	}

	// Light Culling
	pPass = &m_pCurrentAction->passes[kRdrPass_LightCulling];
	{
		pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
		pPass->bEnabled = true;
		pPass->bAlphaBlend = false;
		pPass->bClearDepthTarget = false;
		pPass->depthTestMode = kRdrDepthTestMode_None;
		pPass->shaderMode = kRdrShaderMode_Normal;
	}

	// Opaque
	pPass = &m_pCurrentAction->passes[kRdrPass_Opaque];
	{
		pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
		pPass->bEnabled = true;
		pPass->ahRenderTargets[0] = m_hColorBufferRenderTarget;
		pPass->bClearRenderTargets = true;
		pPass->bAlphaBlend = false;
		pPass->hDepthTarget = m_hPrimaryDepthStencilView;
		pPass->depthTestMode = kRdrDepthTestMode_Equal;
		pPass->shaderMode = kRdrShaderMode_Normal;
	}

	// Sky
	pPass = &m_pCurrentAction->passes[kRdrPass_Sky];
	{
		pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
		pPass->bEnabled = true;
		pPass->ahRenderTargets[0] = m_hColorBufferRenderTarget;
		pPass->hDepthTarget = m_hPrimaryDepthStencilView;
		pPass->depthTestMode = kRdrDepthTestMode_Equal;
		pPass->shaderMode = kRdrShaderMode_Normal;
	}

	// Alpha
	pPass = &m_pCurrentAction->passes[kRdrPass_Alpha];
	{
		pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
		pPass->bEnabled = true;
		pPass->ahRenderTargets[0] = m_hColorBufferRenderTarget;
		pPass->hDepthTarget = m_hPrimaryDepthStencilView;
		pPass->depthTestMode = kRdrDepthTestMode_Equal;
		pPass->shaderMode = kRdrShaderMode_Normal;
		pPass->bAlphaBlend = true;
	}

	m_pCurrentAction->bEnablePostProcessing = true;

	// UI
	pPass = &m_pCurrentAction->passes[kRdrPass_UI];
	{
		pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
		pPass->bEnabled = true;
		pPass->ahRenderTargets[0] = RdrResourceSystem::kPrimaryRenderTargetHandle;
		pPass->bAlphaBlend = true;
		pPass->depthTestMode = kRdrDepthTestMode_None;
		pPass->shaderMode = kRdrShaderMode_Normal;
	}
	
	CreatePerActionConstants();
	QueueLightCulling();
}

void Renderer::EndAction()
{
	m_pCurrentAction = nullptr;
}

void Renderer::AddToBucket(RdrDrawOp* pDrawOp, RdrBucketType bucket)
{
	m_pCurrentAction->buckets[bucket].push_back(pDrawOp);
}

void Renderer::CreateCubemapCaptureConstants(const Vec3& position, const float nearDist, const float farDist)
{
	Camera cam;
	Matrix44 mtxView, mtxProj;

	uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(GsCubemapPerAction));
	GsCubemapPerAction* pGsConstants = (GsCubemapPerAction*)RdrTransientMem::AllocAligned(constantsSize, 16);
	for (uint f = 0; f < kCubemapFace_Count; ++f)
	{
		cam.SetAsCubemapFace(position, (CubemapFace)f, nearDist, farDist);
		cam.GetMatrices(mtxView, mtxProj);
		pGsConstants->mtxViewProj[f] = Matrix44Multiply(mtxView, mtxProj);
		pGsConstants->mtxViewProj[f] = Matrix44Transpose(pGsConstants->mtxViewProj[f]);
	}

	m_pCurrentAction->hPerActionCubemapGs = m_assets.resources.CreateTempConstantBuffer(pGsConstants, constantsSize, 0, kRdrResourceUsage_Default);
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
		VsPerAction* pVsPerAction = (VsPerAction*)RdrTransientMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);

		m_pCurrentAction->hPerActionVs = m_assets.resources.CreateTempConstantBuffer(pVsPerAction, constantsSize, 0, kRdrResourceUsage_Default);

		// PS
		constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(PsPerAction));
		PsPerAction* pPsPerAction = (PsPerAction*)RdrTransientMem::AllocAligned(constantsSize, 16);

		pPsPerAction->viewPos = rCamera.GetPosition();
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->viewWidth = m_viewWidth;

		m_pCurrentAction->hPerActionPs = m_assets.resources.CreateTempConstantBuffer(pPsPerAction, constantsSize, 0, kRdrResourceUsage_Default);
	}

	// Ui per-action
	if ( m_pCurrentAction->passes[kRdrPass_UI].bEnabled)
	{
		mtxView = Matrix44::kIdentity;
		mtxProj = DirectX::XMMatrixOrthographicLH((float)m_viewWidth, (float)m_viewHeight, 0.01f, 1000.f);

		// VS
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(VsPerAction));
		VsPerAction* pVsPerAction = (VsPerAction*)RdrTransientMem::AllocAligned(constantsSize, 16);

		pVsPerAction->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
		pVsPerAction->mtxViewProj = Matrix44Transpose(pVsPerAction->mtxViewProj);

		m_pCurrentAction->hUiPerActionVs = m_assets.resources.CreateTempConstantBuffer(pVsPerAction, constantsSize, 0, kRdrResourceUsage_Default);
	
		// PS
		constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(PsPerAction));
		PsPerAction* pPsPerAction = (PsPerAction*)RdrTransientMem::AllocAligned(constantsSize, 16);

		pPsPerAction->viewPos = Vec3::kOrigin;
		pPsPerAction->mtxInvProj = Matrix44Inverse(mtxProj);
		pPsPerAction->viewWidth = m_viewWidth;

		m_pCurrentAction->hUiPerActionPs = m_assets.resources.CreateTempConstantBuffer(pPsPerAction, constantsSize, 0, kRdrResourceUsage_Default);
	}
}

void Renderer::DrawPass(const RdrAction& rAction, RdrPassEnum ePass)
{
	const RdrPass& rPass = rAction.passes[ePass];

	if (!rPass.bEnabled)
		return;

	m_pContext->BeginEvent(s_passNames[ePass]);

	RdrRenderTargetView renderTargets[MAX_RENDER_TARGETS];
	for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
	{
		if (rPass.ahRenderTargets[i])
		{
			renderTargets[i] = m_assets.resources.GetRenderTargetView(rPass.ahRenderTargets[i]);
		}
		else
		{
			renderTargets[i].pView = nullptr;
		}
	}

	RdrDepthStencilView depthView = { 0 };
	if (rPass.hDepthTarget)
	{
		depthView = m_assets.resources.GetDepthStencilView(rPass.hDepthTarget);
	}

	if (rPass.bClearRenderTargets)
	{
		for (uint i = 0; i < MAX_RENDER_TARGETS; ++i)
		{
			if (renderTargets[i].pView)
			{
				const Color clearColor(0.f, 0.f, 0.f, 1.f);
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

	std::vector<RdrDrawOp*>::const_iterator opIter = rAction.buckets[ s_passBuckets[ePass] ].begin();
	std::vector<RdrDrawOp*>::const_iterator opEndIter = rAction.buckets[ s_passBuckets[ePass] ].end();
	for ( ; opIter != opEndIter; ++opIter )
	{
		RdrDrawOp* pDrawOp = *opIter;
		if (pDrawOp->eType == kRdrDrawOpType_Compute)
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
		for (uint iBucket = 0; iBucket < kRdrBucketType_Count; ++iBucket)
		{
			uint numOps = (uint)rAction.buckets[iBucket].size();
			std::vector<RdrDrawOp*>::iterator iter = rAction.buckets[iBucket].begin();
			std::vector<RdrDrawOp*>::iterator endIter = rAction.buckets[iBucket].end();
			for (; iter != endIter; ++iter)
			{
				RdrDrawOp* pDrawOp = *iter;
				if (pDrawOp->eType == kRdrDrawOpType_Graphics && pDrawOp->graphics.bFreeGeo)
				{
					m_assets.geos.ReleaseGeo(pDrawOp->graphics.hGeo);
				}

				RdrDrawOp::Release(pDrawOp);
			}
			rAction.buckets[iBucket].clear();
		}

		rAction.Reset();
	}

	// Clear remaining state data.
	rActiveState.numActions = 0;

	// Swap state
	m_queueState = !m_queueState;
	m_assets.resources.FlipState();
	m_assets.geos.FlipState();
	m_assets.shaders.FlipState();
	RdrTransientMem::FlipState();
}

void Renderer::DrawFrame()
{
	RdrFrameState& rFrameState = GetActiveState();

	m_assets.shaders.ProcessCommands();
	m_assets.resources.ProcessCommands();
	m_assets.geos.ProcessCommands();

	if (!m_pContext->IsIdle()) // If the device is idle (probably minimized), don't bother rendering anything.
	{
		for (uint iAction = 0; iAction < rFrameState.numActions; ++iAction)
		{
			RdrAction& rAction = rFrameState.actions[iAction];
			m_pContext->BeginEvent(rAction.name);

			RdrRasterState rasterState;
			rasterState.bEnableMSAA = (s_msaaLevel > 1); // todo: this is only true for final render target
			rasterState.bEnableScissor = false;
			rasterState.bWireframe = s_wireframe;
			m_pContext->SetRasterState(rasterState);

			// todo: sort buckets 
			//std::sort(pAction->buckets[kRdrBucketType_Opaque].begin(), pAction->buckets[kRdrBucketType_Opaque].end(), );

			DrawPass(rAction, kRdrPass_ZPrepass);
			DrawPass(rAction, kRdrPass_LightCulling);
			DrawPass(rAction, kRdrPass_Opaque);
			DrawPass(rAction, kRdrPass_Sky);
			DrawPass(rAction, kRdrPass_Alpha);

			// Resolve multisampled color buffer.
			const RdrResource* pColorBuffer = m_assets.resources.GetResource(m_hColorBuffer);
			if (s_msaaLevel)
			{
				const RdrResource* pColorBufferMultisampled = m_assets.resources.GetResource(m_hColorBufferMultisampled);
				m_pContext->ResolveSurface(*pColorBufferMultisampled, *pColorBuffer);
			}

			if (rAction.bEnablePostProcessing)
				m_postProcess.DoPostProcessing(m_pContext, m_drawState, pColorBuffer, m_assets);

			// UI should never be wireframe
			if (s_wireframe)
			{
				rasterState.bWireframe = false;
				m_pContext->SetRasterState(rasterState);
			}
			DrawPass(rAction, kRdrPass_UI);

			m_pContext->EndEvent();
		}
	}

	m_pContext->Present();
}

void Renderer::DrawGeo(const RdrAction& rAction, const RdrPassEnum ePass, const RdrDrawOp* pDrawOp, const RdrLightParams& rLightParams, const RdrResourceHandle hTileLightIndices)
{
	const RdrPass& rPass = rAction.passes[ePass];
	bool bDepthOnly = (rPass.shaderMode == kRdrShaderMode_DepthOnly);
	const RdrGeometry* pGeo = m_assets.geos.GetGeo(pDrawOp->graphics.hGeo);
	RdrShaderFlags shaderFlags = (RdrShaderFlags)0;

	// Vertex shader
	RdrVertexShader vertexShader = pDrawOp->graphics.vertexShader;
	if (bDepthOnly)
	{
		shaderFlags |= kRdrShaderFlag_DepthOnly;
	}
	vertexShader.flags |= shaderFlags;

	m_drawState.pVertexShader = m_assets.shaders.GetVertexShader(vertexShader);

	RdrConstantBufferHandle hPerActionVs = (ePass == kRdrPass_UI) ? rAction.hUiPerActionVs : rAction.hPerActionVs;
	m_drawState.vsConstantBuffers[0] = m_assets.resources.GetConstantBuffer(hPerActionVs)->bufferObj;
	if (pDrawOp->graphics.hVsConstants)
	{
		m_drawState.vsConstantBuffers[1] = m_assets.resources.GetConstantBuffer(pDrawOp->graphics.hVsConstants)->bufferObj;
		m_drawState.vsConstantBufferCount = 2;
	}
	else
	{
		m_drawState.vsConstantBufferCount = 1;
	}

	// Geom shader
	if (rAction.bIsCubemapCapture)
	{
		RdrGeometryShader geomShader = { kRdrGeometryShader_Model_CubemapCapture, shaderFlags };
		m_drawState.pGeometryShader = m_assets.shaders.GetGeometryShader(geomShader);
		m_drawState.gsConstantBuffers[0] = m_assets.resources.GetConstantBuffer(rAction.hPerActionCubemapGs)->bufferObj;
		m_drawState.gsConstantBufferCount = 1;
	}

	// Pixel shader
	m_drawState.pPixelShader = pDrawOp->graphics.hPixelShaders[rPass.shaderMode] ?
		m_assets.shaders.GetPixelShader(pDrawOp->graphics.hPixelShaders[rPass.shaderMode]) :
		nullptr;
	if (m_drawState.pPixelShader)
	{
		for (uint i = 0; i < pDrawOp->texCount; ++i)
		{
			m_drawState.psResources[i] = m_assets.resources.GetResource(pDrawOp->hTextures[i])->resourceView;
			m_drawState.psSamplers[i] = pDrawOp->samplers[i];
		}

		if (pDrawOp->graphics.bNeedsLighting && !bDepthOnly)
		{
			m_drawState.psResources[kPsResource_LightList] = m_assets.resources.GetResource(rLightParams.hLightListRes)->resourceView;
			m_drawState.psResources[kPsResource_TileLightIds] = m_assets.resources.GetResource(hTileLightIndices)->resourceView;
			m_drawState.psResources[kPsResource_ShadowMaps] = m_assets.resources.GetResource(rLightParams.hShadowMapTexArray)->resourceView;
			m_drawState.psResources[kPsResource_ShadowCubeMaps] = m_assets.resources.GetResource(rLightParams.hShadowCubeMapTexArray)->resourceView;
			m_drawState.psSamplers[kPsSampler_ShadowMap] = RdrSamplerState(kComparisonFunc_LessEqual, kRdrTexCoordMode_Clamp, false);
			m_drawState.psResources[kPsResource_ShadowMapData] = m_assets.resources.GetResource(rLightParams.hShadowMapDataRes)->resourceView;
		}

		RdrConstantBufferHandle hPerActionPs = (ePass == kRdrPass_UI) ? rAction.hUiPerActionPs : rAction.hPerActionPs;
		m_drawState.psConstantBuffers[0] = m_assets.resources.GetConstantBuffer(rAction.hPerActionPs)->bufferObj;
		m_drawState.psConstantBufferCount = 1;
	}

	// Input assembly
	m_drawState.pInputLayout = m_assets.shaders.GetInputLayout(pDrawOp->graphics.hInputLayout); // todo: layouts per flags
	m_drawState.eTopology = kRdrTopology_TriangleList;

	m_drawState.vertexBuffers[0] = *m_assets.geos.GetVertexBuffer(pGeo->hVertexBuffer);
	m_drawState.vertexStride = pGeo->geoInfo.vertStride;
	m_drawState.vertexOffset = 0;
	m_drawState.vertexCount = pGeo->geoInfo.numVerts;

	if (pGeo->hIndexBuffer)
	{
		m_drawState.indexBuffer = *m_assets.geos.GetIndexBuffer(pGeo->hIndexBuffer);
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

void Renderer::DispatchCompute(RdrDrawOp* pDrawOp)
{
	m_drawState.pComputeShader = m_assets.shaders.GetComputeShader(pDrawOp->compute.shader);

	m_drawState.csConstantBuffers[0] = m_assets.resources.GetConstantBuffer(pDrawOp->compute.hCsConstants)->bufferObj;
	m_drawState.csConstantBufferCount = 1;

	for (uint i = 0; i < pDrawOp->texCount; ++i)
	{
		if (pDrawOp->hTextures[i])
			m_drawState.csResources[i] = m_assets.resources.GetResource(pDrawOp->hTextures[i])->resourceView;
		else
			m_drawState.csResources[i].pTypeless = nullptr;
	}

	for (uint i = 0; i < pDrawOp->compute.viewCount; ++i)
	{
		if (pDrawOp->hTextures[i])
			m_drawState.csUavs[i] = m_assets.resources.GetResource(pDrawOp->compute.hViews[i])->uav;
		else
			m_drawState.csUavs[i].pTypeless = nullptr;
	}

	m_pContext->DispatchCompute(m_drawState, pDrawOp->compute.threads[0], pDrawOp->compute.threads[1], pDrawOp->compute.threads[2]);

	m_drawState.Reset();
}
