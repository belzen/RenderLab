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
#include "RdrTransientHeap.h"


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
		kRdrBucketType_Alpha,		 // kRdrPass_Alpha
		kRdrBucketType_UI,		     // kRdrPass_UI
	};
	static_assert(sizeof(s_passBuckets) / sizeof(s_passBuckets[0]) == kRdrPass_Count, "Missing pass -> bucket mappings");

	// Event names for passes
	static const wchar_t* s_passNames[] =
	{
		L"Z-Prepass",		// kRdrPass_ZPrepass,
		L"Light Culling",	// kRdrPass_LightCulling,
		L"Opaque",			// kRdrPass_Opaque,
		L"Alpha",			// kRdrPass_Alpha,
		L"UI",				// kRdrPass_UI,
	};
	static_assert(sizeof(s_passNames) / sizeof(s_passNames[0]) == kRdrPass_Count, "Missing RdrPass names!");
}
//////////////////////////////////////////////////////


bool Renderer::Init(HWND hWnd, int width, int height)
{
	DebugConsole::RegisterCommand("wireframe", setWireframeEnabled, kDebugCommandArgType_Number);

	m_pContext = new RdrContextD3D11();
	if (!m_pContext->Init(hWnd, width, height, s_msaaLevel))
		return false;

	m_shaders.Init(m_pContext);
	m_resources.Init(m_pContext);
	m_geos.Init(m_pContext);

	Resize(width, height);

	// Constant buffers
	m_perFrameBufferVS.size = sizeof(VsPerFrame);
	m_perFrameBufferVS.bufferObj = m_pContext->CreateConstantBuffer(nullptr, sizeof(VsPerFrame), kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);
	m_perFrameBufferPS.size = sizeof(PsPerFrame);
	m_perFrameBufferPS.bufferObj = m_pContext->CreateConstantBuffer(nullptr, sizeof(PsPerFrame), kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);
	m_cubemapPerFrameBufferGS.size = sizeof(GsCubemapPerFrame);
	m_cubemapPerFrameBufferGS.bufferObj = m_pContext->CreateConstantBuffer(nullptr, sizeof(GsCubemapPerFrame), kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);

	Font::Init(*this);

	m_hDepthMinMaxShader = m_shaders.CreateShaderFromFile(kRdrShaderType_Compute, "c_tiled_depth_calc.hlsl");
	m_hLightCullShader = m_shaders.CreateShaderFromFile(kRdrShaderType_Compute, "c_tiled_light_cull.hlsl");

	return true;
}

void Renderer::Cleanup()
{
	m_pContext->ReleaseConstantBuffer(m_perFrameBufferVS.bufferObj);
	m_pContext->ReleaseConstantBuffer(m_perFrameBufferPS.bufferObj);
	m_pContext->ReleaseConstantBuffer(m_cubemapPerFrameBufferGS.bufferObj);

	// todo: flip frame to finish resource frees.

	m_pContext->Release();
	delete m_pContext;
}

void Renderer::Resize(int width, int height)
{
	if (!m_pContext || width == 0 || height == 0)
		return;

	m_viewWidth = width;
	m_viewHeight = height;

	m_pContext->Resize(width, height);


	// Release existing resources
	if (m_hPrimaryDepthStencilView)
		m_resources.ReleaseDepthStencilView(m_hPrimaryDepthStencilView);
	if (m_hPrimaryDepthBuffer)
		m_resources.ReleaseResource(m_hPrimaryDepthBuffer);

	// Depth Buffer
	m_hPrimaryDepthBuffer = m_resources.CreateTexture2DMS(width, height, kResourceFormat_D24_UNORM_S8_UINT, s_msaaLevel);
	m_hPrimaryDepthStencilView = m_resources.CreateDepthStencilView(m_hPrimaryDepthBuffer);
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
			m_resources.ReleaseResource(m_hDepthMinMaxTex);
		m_hDepthMinMaxTex = m_resources.CreateTexture2D(tileCountX, tileCountY, kResourceFormat_RG_F16);
	}

	RdrDrawOp* pDepthOp = RdrDrawOp::Allocate();
	pDepthOp->eType = kRdrDrawOpType_Compute;
	pDepthOp->compute.hShader = m_hDepthMinMaxShader;
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
	Vec4* pConstants = (Vec4*)RdrTransientHeap::AllocAligned(constantsSize, 16);
	for (int i = 0; i < 4; ++i)
	{
		pConstants[i].x = invProjMtx.m[i][0];
		pConstants[i].y = invProjMtx.m[i][1];
		pConstants[i].z = invProjMtx.m[i][2];
		pConstants[i].w = invProjMtx.m[i][3];
	}
	if (m_hDepthMinMaxConstants)
	{
		m_resources.UpdateConstantBuffer(m_hDepthMinMaxConstants, pConstants);
	}
	else
	{
		m_hDepthMinMaxConstants = m_resources.CreateConstantBuffer(pConstants, constantsSize, kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);
	}
	pDepthOp->compute.hCsConstants = m_hDepthMinMaxConstants;

	AddToBucket(pDepthOp, kRdrBucketType_LightCulling);

	//////////////////////////////////////
	// Light culling
	if ( tileCountChanged )
	{
		const uint kMaxLightsPerTile = 128; // Sync with MAX_LIGHTS_PER_TILE in "light_inc.hlsli"
		if (m_hTileLightIndices)
			m_resources.ReleaseResource(m_hTileLightIndices);
		m_hTileLightIndices = m_resources.CreateStructuredBuffer(nullptr, tileCountX * tileCountY * kMaxLightsPerTile, sizeof(uint), false, kRdrResourceUsage_Default);
	}

	RdrDrawOp* pCullOp = RdrDrawOp::Allocate();
	pCullOp->eType = kRdrDrawOpType_Compute;
	pCullOp->compute.hShader = m_hLightCullShader;
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

	constantsSize = sizeof(Vec4) * (uint)ceilf(sizeof(CullingParams) / (float)sizeof(Vec4));
	CullingParams* pParams = (CullingParams*)RdrTransientHeap::AllocAligned(constantsSize, 16);
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
		m_resources.UpdateConstantBuffer(m_hTileCullConstants, pParams);
	}
	else
	{
		m_hTileCullConstants = m_resources.CreateConstantBuffer(pParams, constantsSize, kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);
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
	m_pCurrentAction->bDoLightCulling = false;

	// Setup action passes
	RdrPass& rPass = m_pCurrentAction->passes[kRdrPass_ZPrepass];
	rPass.pCamera = &m_pCurrentAction->camera;
	rPass.viewport = viewport;
	rPass.bEnabled = true;
	rPass.hDepthTarget = hDepthView;
	rPass.bClearDepthTarget = true;
	rPass.depthTestMode = kRdrDepthTestMode_Less;
	rPass.bAlphaBlend = false;
	rPass.shaderMode = kRdrShaderMode_DepthOnly;
}

void Renderer::BeginShadowCubeMapAction(const Light* pLight, RdrRenderTargetViewHandle* ahTargetViews, Rect& viewport) // todo: finish
{
	assert(m_pCurrentAction == nullptr);

	m_pCurrentAction = GetNextAction();
	m_pCurrentAction->name = L"Shadow Cube Map";
	m_pCurrentAction->camera = pLight->MakeCamera(kCubemapFace_PositiveX);
	m_pCurrentAction->bDoLightCulling = false;

	// Setup action passes
	RdrPass& rPass = m_pCurrentAction->passes[kRdrPass_ZPrepass];
	rPass.pCamera = &m_pCurrentAction->camera;
	rPass.viewport = viewport;
	rPass.bEnabled = true;
	rPass.bAlphaBlend = false;
	rPass.shaderMode = kRdrShaderMode_CubeMapDepthOnly;
	rPass.depthTestMode = kRdrDepthTestMode_Less;
	rPass.bClearDepthTarget = true;
	rPass.bClearRenderTargets = true;
	for (int i = 0; i < 6; ++i)
	{
		rPass.ahRenderTargets[i] = ahTargetViews[i];
	}
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
	pPass->pCamera = &m_pCurrentAction->camera;
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->bAlphaBlend = false;
	pPass->hDepthTarget = m_hPrimaryDepthStencilView;
	pPass->bClearDepthTarget = true;
	pPass->depthTestMode = kRdrDepthTestMode_Less;
	pPass->shaderMode = kRdrShaderMode_DepthOnly;

	// Light Culling
	pPass = &m_pCurrentAction->passes[kRdrPass_LightCulling];
	pPass->pCamera = &m_pCurrentAction->camera;
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->bAlphaBlend = false;
	pPass->bClearDepthTarget = false;
	pPass->depthTestMode = kRdrDepthTestMode_None;
	pPass->shaderMode = kRdrShaderMode_Normal;

	// Opaque
	pPass = &m_pCurrentAction->passes[kRdrPass_Opaque];
	pPass->pCamera = &m_pCurrentAction->camera;
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->ahRenderTargets[0] = RdrResourceSystem::kPrimaryRenderTargetHandle;
	pPass->bClearRenderTargets = true;
	pPass->bAlphaBlend = false;
	pPass->hDepthTarget = m_hPrimaryDepthStencilView;
	pPass->depthTestMode = kRdrDepthTestMode_Equal;
	pPass->shaderMode = kRdrShaderMode_Normal;

	// Alpha
	pPass = &m_pCurrentAction->passes[kRdrPass_Alpha];
	pPass->pCamera = &m_pCurrentAction->camera;
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->ahRenderTargets[0] = RdrResourceSystem::kPrimaryRenderTargetHandle;
	pPass->bAlphaBlend = true;
	pPass->hDepthTarget = m_hPrimaryDepthStencilView;
	pPass->depthTestMode = kRdrDepthTestMode_None;
	pPass->shaderMode = kRdrShaderMode_Normal;

	// UI
	pPass = &m_pCurrentAction->passes[kRdrPass_UI];
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->ahRenderTargets[0] = RdrResourceSystem::kPrimaryRenderTargetHandle;
	pPass->bAlphaBlend = true;
	pPass->hDepthTarget = m_hPrimaryDepthStencilView;
	pPass->depthTestMode = kRdrDepthTestMode_None;
	pPass->shaderMode = kRdrShaderMode_Normal;
	
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

void Renderer::SetPerFrameConstants(const RdrAction& rAction, const RdrPass& rPass)
{
	VsPerFrame vsPerFrame;
	Matrix44 mtxView;
	Matrix44 mtxProj;
	if (rPass.pCamera)
	{
		rPass.pCamera->GetMatrices(mtxView, mtxProj);
	}
	else
	{
		mtxView = Matrix44::kIdentity;
		mtxProj = DirectX::XMMatrixOrthographicLH((float)m_viewWidth, (float)m_viewHeight, 0.01f, 1000.f);
	}

	vsPerFrame.mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
	vsPerFrame.mtxViewProj = Matrix44Transpose(vsPerFrame.mtxViewProj);

	m_pContext->UpdateConstantBuffer(m_perFrameBufferVS, &vsPerFrame);
	

	// PS buffer.
	PsPerFrame psPerFrame;
	if (rPass.pCamera)
	{
		psPerFrame.viewPos = rPass.pCamera->GetPosition();
	}
	else
	{
		psPerFrame.viewPos = Vec3::kOrigin;
	}
	psPerFrame.mtxInvProj = Matrix44Inverse(mtxProj);
	psPerFrame.viewWidth = m_viewWidth;

	m_pContext->UpdateConstantBuffer(m_perFrameBufferPS, &psPerFrame);

	/* todo
	if (rAction.bIsCubemapCapture)
	{
		GsCubemapPerFrame* gsConstants = (GsCubemapPerFrame*)m_pContext->MapResource(m_hCubemapPerFrameBufferGS, kRdrResourceMap_WriteDiscard);

		for (uint f = 0; f < 6; ++f)
		{
			rPass.pCamera->GetMatrices(mtxView, mtxProj);
			gsConstants->mtxViewProj[f] = Matrix44Multiply(mtxView, mtxProj);
			gsConstants->mtxViewProj[f] = Matrix44Transpose(gsConstants->mtxViewProj[f]);
		}

		m_pContext->UnmapResource(m_hCubemapPerFrameBufferGS);
		m_pContext->GSSetConstantBuffers(0, 1, &m_hCubemapPerFrameBufferGS);
	}
	*/
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
			renderTargets[i] = m_resources.GetRenderTargetView(rPass.ahRenderTargets[i]);
		}
		else
		{
			renderTargets[i].pView = nullptr;
		}
	}

	RdrDepthStencilView depthView = { 0 };
	if (rPass.hDepthTarget)
	{
		depthView = m_resources.GetDepthStencilView(rPass.hDepthTarget);
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

	SetPerFrameConstants(rAction, rPass);

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
			DrawGeo(pDrawOp, rPass.shaderMode, rAction.lightParams, m_hTileLightIndices);
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
					m_geos.ReleaseGeo(pDrawOp->graphics.hGeo);
				}

				RdrDrawOp::Release(pDrawOp);
			}
			rAction.buckets[iBucket].clear();
		}

		rAction.Reset();
	}

	// Clear remaining state data.
	rActiveState.numActions = 0;
	rActiveState.constantBufferUpdates.clear();
	rActiveState.resourceUpdates.clear();

	// Swap state
	m_queueState = !m_queueState;
	m_resources.FlipState();
	m_geos.FlipState();
	m_shaders.FlipState();
}

void Renderer::DrawFrame()
{
	RdrFrameState& rFrameState = GetActiveState();

	m_shaders.ProcessCommands();
	m_resources.ProcessCommands();
	m_geos.ProcessCommands();

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
		//std::sort(pAction->buckets[kRdrBucketType_Opaque].begin(), pAction->buckets[kRdrBucketType_Opaque].end(), );

		DrawPass(rAction, kRdrPass_ZPrepass);
		DrawPass(rAction, kRdrPass_LightCulling);
		DrawPass(rAction, kRdrPass_Opaque);
		DrawPass(rAction, kRdrPass_Alpha);

		// UI should never be wireframe
		if (s_wireframe)
		{
			rasterState.bWireframe = false;
			m_pContext->SetRasterState(rasterState);
		}
		DrawPass(rAction, kRdrPass_UI);

		m_pContext->EndEvent();
	}

	m_pContext->Present();
}

void Renderer::DrawGeo(RdrDrawOp* pDrawOp, RdrShaderMode eShaderMode, const RdrLightParams& rLightParams, RdrResourceHandle hTileLightIndices)
{
	bool bDepthOnly = (eShaderMode == kRdrShaderMode_DepthOnly || eShaderMode == kRdrShaderMode_CubeMapDepthOnly);
	const RdrGeometry* pGeo = m_geos.GetGeo(pDrawOp->graphics.hGeo);

	m_drawState.pVertexShader = m_shaders.GetShader(kRdrShaderType_Vertex, pDrawOp->graphics.hVertexShaders[eShaderMode]);
	m_drawState.pGeometryShader = pDrawOp->graphics.hGeometryShaders[eShaderMode] ?
		m_shaders.GetShader(kRdrShaderType_Geometry, pDrawOp->graphics.hGeometryShaders[eShaderMode]) :
		nullptr;
	m_drawState.pPixelShader = pDrawOp->graphics.hPixelShaders[eShaderMode] ?
		m_shaders.GetShader(kRdrShaderType_Pixel, pDrawOp->graphics.hPixelShaders[eShaderMode]) :
		nullptr;

	for (uint i = 0; i < pDrawOp->texCount; ++i)
	{
		m_drawState.pPsResources[i] = m_resources.GetResource(pDrawOp->hTextures[i]);
		m_drawState.psSamplers[i] = pDrawOp->samplers[i];
	}

	if (pDrawOp->graphics.bNeedsLighting && !bDepthOnly)
	{
		m_drawState.pPsResources[kPsResource_LightList] = m_resources.GetResource(rLightParams.hLightListRes);
		m_drawState.pPsResources[kPsResource_TileLightIds] = m_resources.GetResource(hTileLightIndices);
		m_drawState.pPsResources[kPsResource_ShadowMaps] = m_resources.GetResource(rLightParams.hShadowMapTexArray);
		m_drawState.pPsResources[kPsResource_ShadowCubeMaps] = m_resources.GetResource(rLightParams.hShadowCubeMapTexArray);
		m_drawState.psSamplers[kPsSampler_ShadowMap] = RdrSamplerState(kComparisonFunc_LessEqual, kRdrTexCoordMode_Clamp, false);
		m_drawState.pPsResources[kPsResource_ShadowMapData] = m_resources.GetResource(rLightParams.hShadowMapDataRes);
	}

	m_drawState.pInputLayout = m_shaders.GetInputLayout(pDrawOp->graphics.hInputLayouts[eShaderMode]);
	m_drawState.eTopology = kRdrTopology_TriangleList;

	m_drawState.vertexBuffers[0] = *m_geos.GetVertexBuffer(pGeo->hVertexBuffer);
	m_drawState.vertexStride = pGeo->geoInfo.vertStride;
	m_drawState.vertexOffset = 0;
	m_drawState.vertexCount = pGeo->geoInfo.numVerts;

	m_drawState.vsConstantBuffers[0] = m_perFrameBufferVS.bufferObj;
	if (pDrawOp->graphics.hVsConstants)
	{
		m_drawState.vsConstantBuffers[1] = m_resources.GetConstantBuffer(pDrawOp->graphics.hVsConstants)->bufferObj;
		m_drawState.vsConstantBufferCount = 2;
	}
	else
	{
		m_drawState.vsConstantBufferCount = 1;
	}

	m_drawState.psConstantBuffers[0] = m_perFrameBufferPS.bufferObj;
	m_drawState.psConstantBufferCount = 1;

	if (pGeo->hIndexBuffer)
	{
		m_drawState.indexBuffer = *m_geos.GetIndexBuffer(pGeo->hIndexBuffer);
		m_drawState.indexCount = pGeo->geoInfo.numIndices;
	}
	else
	{
		m_drawState.indexBuffer.pBuffer = nullptr;
	}

	m_pContext->Draw(m_drawState);

	memset(&m_drawState, 0, sizeof(m_drawState));
	// Reset state.  Todo: don't do this
	m_drawState.pPsResources[kPsResource_LightList] = nullptr;
	m_drawState.pPsResources[kPsResource_TileLightIds] = nullptr;
}

void Renderer::DispatchCompute(RdrDrawOp* pDrawOp)
{
	m_drawState.pComputeShader = m_shaders.GetShader(kRdrShaderType_Compute, pDrawOp->compute.hShader);

	m_drawState.csConstantBuffers[0] = m_resources.GetConstantBuffer(pDrawOp->compute.hCsConstants)->bufferObj;
	m_drawState.csConstantBufferCount = 1;

	for (uint i = 0; i < pDrawOp->texCount; ++i)
	{
		m_drawState.pCsResources[i] = m_resources.GetResource(pDrawOp->hTextures[i]);
	}

	for (uint i = 0; i < pDrawOp->compute.viewCount; ++i)
	{
		m_drawState.pCsUavs[i] = m_resources.GetResource(pDrawOp->compute.hViews[i]);
	}

	m_pContext->DispatchCompute(m_drawState, pDrawOp->compute.threads[0], pDrawOp->compute.threads[1], pDrawOp->compute.threads[2]);

	memset(&m_drawState, 0, sizeof(m_drawState));
}
