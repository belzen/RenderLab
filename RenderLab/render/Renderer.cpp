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

#define MAX_RENDERTARGETS 6

namespace
{
	static bool s_wireframe = 0;
	static int s_msaaLevel = 2;

	void setWireframeEnabled(DebugCommandArg *args, int numArgs)
	{
		s_wireframe = (args[0].val.num != 0.f);
	}
}

struct RdrPass
{
	RdrRenderTargetView aRenderTargets[MAX_RENDERTARGETS];
	RdrDepthStencilView depthTarget;

	// If no camera is set, an ortho projection is used.
	const Camera* pCamera;
	Rect viewport;

	RdrShaderMode shaderMode;
	RdrDepthTestMode depthTestMode;
	bool bAlphaBlend;

	bool bEnabled;
	bool bClearRenderTargets;
	bool bClearDepthTarget;
};

struct RdrAction
{
	static RdrAction* Allocate();
	static void Release(RdrAction* pAction);

	///
	LPCWSTR name;

	std::vector<RdrDrawOp*> buckets[kRdrBucketType_Count];
	RdrPass passes[kRdrPass_Count];
	bool bDoLightCulling;

	Camera camera;
	const LightList* pLights;
};

static RdrAction s_actionRingBuffer[32];
static uint s_nextAction = 0;
static uint s_actionsInUse = 0;

RdrAction* RdrAction::Allocate()
{
	RdrAction* pAction = &s_actionRingBuffer[s_nextAction];
	s_nextAction = (s_nextAction + 1) % ARRAYSIZE(s_actionRingBuffer);
	++s_actionsInUse;
	assert(s_actionsInUse < ARRAYSIZE(s_actionRingBuffer));
	return pAction;
}

void RdrAction::Release(RdrAction* pAction)
{
	for (int i = 0; i < kRdrBucketType_Count; ++i)
		pAction->buckets[i].clear();
	memset(pAction->passes, 0, sizeof(pAction->passes));
	pAction->pLights = nullptr;
	--s_actionsInUse;
}

//////////////////////////////////////////////////////

// Pass to bucket mappings
RdrBucketType s_passBuckets[] = {
	kRdrBucketType_Opaque,	// kRdrPass_ZPrepass
	kRdrBucketType_Opaque,	// kRdrPass_Opaque
	kRdrBucketType_Alpha,	// kRdrPass_Alpha
	kRdrBucketType_UI,		// kRdrPass_UI
};
static_assert(sizeof(s_passBuckets) / sizeof(s_passBuckets[0]) == kRdrPass_Count, "Missing pass -> bucket mappings");

// Event names for passes
static const wchar_t* s_passNames[] =
{
	L"Z-Prepass",		// kRdrPass_ZPrepass,
	L"Opaque",			// kRdrPass_Opaque,
	L"Alpha",			// kRdrPass_Alpha,
	L"UI",				// kRdrPass_UI,
};
static_assert(sizeof(s_passNames) / sizeof(s_passNames[0]) == kRdrPass_Count, "Missing RdrPass names!");

//////////////////////////////////////////////////////


bool Renderer::Init(HWND hWnd, int width, int height)
{
	DebugConsole::RegisterCommand("wireframe", setWireframeEnabled, kDebugCommandArgType_Number);

	m_pContext = new RdrContextD3D11();
	if (!m_pContext->Init(hWnd, width, height, s_msaaLevel))
		return false;

	Resize(width, height);

	// Constant buffers
	m_hPerFrameBufferVS = m_pContext->CreateConstantBuffer(sizeof(VsPerFrame), kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);
	m_hPerFrameBufferPS = m_pContext->CreateConstantBuffer(sizeof(PsPerFrame), kRdrCpuAccessFlag_Write, kRdrResourceUsage_Dynamic);

	Font::Init(m_pContext);

	m_hDepthMinMaxShader = m_pContext->LoadComputeShader("c_tiled_depth_calc.hlsl");
	m_hLightCullShader = m_pContext->LoadComputeShader("c_tiled_light_cull.hlsl");

	return true;
}

void Renderer::Cleanup()
{
	m_pContext->ReleaseResource(m_hPerFrameBufferVS);
	m_pContext->ReleaseResource(m_hPerFrameBufferPS);

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
}

const Camera& Renderer::GetCurrentCamera(void) const
{
	return m_pCurrentAction->camera;
}

void Renderer::DispatchLightCulling(RdrAction* pAction)
{
	const Camera& rCamera = pAction->camera;

	m_pContext->BeginEvent(L"Light Culling");

	RdrRenderTargetView renderTarget = { nullptr };
	RdrDepthStencilView depthTarget = { nullptr };
	m_pContext->SetRenderTargets(1, &renderTarget, depthTarget);
	m_pContext->SetDepthStencilState(kRdrDepthTestMode_None);
	m_pContext->SetBlendState(false);

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
			m_pContext->ReleaseResource(m_hDepthMinMaxTex);
		m_hDepthMinMaxTex = m_pContext->CreateTexture2D(tileCountX, tileCountY, kResourceFormat_RG_F16);
	}

	RdrDrawOp* pDepthOp = RdrDrawOp::Allocate();
	pDepthOp->hComputeShader = m_hDepthMinMaxShader;
	pDepthOp->computeThreads[0] = tileCountX;
	pDepthOp->computeThreads[1] = tileCountY;
	pDepthOp->computeThreads[2] = 1;
	pDepthOp->hTextures[0] = m_pContext->GetPrimaryDepthTexture();
	pDepthOp->texCount = 1;
	pDepthOp->hViews[0] = m_hDepthMinMaxTex;
	pDepthOp->viewCount = 1;

	Matrix44 viewMtx;
	Matrix44 invProjMtx;
	rCamera.GetMatrices(viewMtx, invProjMtx);
	invProjMtx = Matrix44Inverse(invProjMtx);
	invProjMtx = Matrix44Transpose(invProjMtx);

	for (int i = 0; i < 4; ++i)
	{
		pDepthOp->constants[i].x = invProjMtx.m[i][0];
		pDepthOp->constants[i].y = invProjMtx.m[i][1];
		pDepthOp->constants[i].z = invProjMtx.m[i][2];
		pDepthOp->constants[i].w = invProjMtx.m[i][3];
	}
	pDepthOp->numConstants = 4;

	// Dispatch 
	m_pContext->DispatchCompute(pDepthOp);
	RdrDrawOp::Release(pDepthOp);

	//////////////////////////////////////
	// Light culling
	if ( tileCountChanged )
	{
		const uint kMaxLightsPerTile = 128; // Sync with MAX_LIGHTS_PER_TILE in "light_inc.hlsli"
		if (m_hTileLightIndices)
			m_pContext->ReleaseResource(m_hTileLightIndices);
		m_hTileLightIndices = m_pContext->CreateStructuredBuffer(nullptr, tileCountX * tileCountY * kMaxLightsPerTile, sizeof(uint));
	}

	RdrDrawOp* pCullOp = RdrDrawOp::Allocate();
	pCullOp->hComputeShader = m_hLightCullShader;
	pCullOp->computeThreads[0] = tileCountX;
	pCullOp->computeThreads[1] = tileCountY;
	pCullOp->computeThreads[2] = 1;
	pCullOp->hTextures[0] = pAction->pLights->GetLightListRes();
	pCullOp->hTextures[1] = m_hDepthMinMaxTex;
	pCullOp->texCount = 2;
	pCullOp->hViews[0] = m_hTileLightIndices;
	pCullOp->viewCount = 1;


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

	CullingParams* pParams = reinterpret_cast<CullingParams*>(pCullOp->constants);
	pParams->cameraPos = rCamera.GetPosition();
	pParams->cameraDir = rCamera.GetDirection();
	pParams->cameraNearDist = rCamera.GetNearDist();
	pParams->cameraFarDist = rCamera.GetFarDist();
	pParams->screenSize = GetViewportSize();
	pParams->fovY = rCamera.GetFieldOfViewY();
	pParams->aspectRatio = rCamera.GetAspectRatio();
	pParams->lightCount = pAction->pLights->GetLightCount();
	pParams->tileCountX = tileCountX;
	pParams->tileCountY = tileCountY;

	pCullOp->numConstants = (sizeof(CullingParams) / sizeof(Vec4)) + 1;

	// Dispatch 
	m_pContext->DispatchCompute(pCullOp);
	RdrDrawOp::Release(pCullOp);

	m_pContext->EndEvent();
}

void Renderer::BeginShadowMapAction(const Camera& rCamera, RdrDepthStencilView depthView, Rect& viewport)
{
	assert(m_pCurrentAction == nullptr);

	m_pCurrentAction = RdrAction::Allocate();
	m_pCurrentAction->name = L"Shadow Map";
	m_pCurrentAction->camera = rCamera;
	m_pCurrentAction->bDoLightCulling = false;

	// Setup action passes
	RdrPass& rPass = m_pCurrentAction->passes[kRdrPass_ZPrepass];
	rPass.pCamera = &m_pCurrentAction->camera;
	rPass.viewport = viewport;
	rPass.bEnabled = true;
	rPass.depthTarget = depthView;
	rPass.bClearDepthTarget = true;
	rPass.depthTestMode = kRdrDepthTestMode_Less;
	rPass.bAlphaBlend = false;
	rPass.shaderMode = kRdrShaderMode_DepthOnly;
}

void Renderer::BeginShadowCubeMapAction(const Light* pLight, RdrRenderTargetView* pTargetViews, Rect& viewport) // todo: finish
{
	assert(m_pCurrentAction == nullptr);

	m_pCurrentAction = RdrAction::Allocate();
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
		rPass.aRenderTargets[i] = pTargetViews[i];
	}
}

void Renderer::BeginPrimaryAction(const Camera& rCamera, const LightList* pLights)
{
	assert(m_pCurrentAction == nullptr);

	RdrRenderTargetView primaryRenderTarget = m_pContext->GetPrimaryRenderTarget();
	RdrDepthStencilView primaryDepthTarget = m_pContext->GetPrimaryDepthStencilTarget();

	m_pCurrentAction = RdrAction::Allocate();
	m_pCurrentAction->name = L"Primary Action";
	m_pCurrentAction->pLights = pLights;
	m_pCurrentAction->bDoLightCulling = true;

	m_pCurrentAction->camera = rCamera;
	m_pCurrentAction->camera.SetAspectRatio(m_viewWidth / (float)m_viewHeight);

	// Z Prepass
	RdrPass* pPass = &m_pCurrentAction->passes[kRdrPass_ZPrepass];
	pPass->pCamera = &m_pCurrentAction->camera;
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->bAlphaBlend = false;
	pPass->depthTarget = primaryDepthTarget;
	pPass->bClearDepthTarget = true;
	pPass->depthTestMode = kRdrDepthTestMode_Less;
	pPass->shaderMode = kRdrShaderMode_DepthOnly;

	// Opaque
	pPass = &m_pCurrentAction->passes[kRdrPass_Opaque];
	pPass->pCamera = &m_pCurrentAction->camera;
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->aRenderTargets[0] = primaryRenderTarget;
	pPass->bClearRenderTargets = true;
	pPass->bAlphaBlend = false;
	pPass->depthTarget = primaryDepthTarget;
	pPass->depthTestMode = kRdrDepthTestMode_Equal;
	pPass->shaderMode = kRdrShaderMode_Normal;

	// Alpha
	pPass = &m_pCurrentAction->passes[kRdrPass_Alpha];
	pPass->pCamera = &m_pCurrentAction->camera;
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->aRenderTargets[0] = primaryRenderTarget;
	pPass->bAlphaBlend = true;
	pPass->depthTarget = primaryDepthTarget;
	pPass->depthTestMode = kRdrDepthTestMode_None;
	pPass->shaderMode = kRdrShaderMode_Normal;

	// UI
	pPass = &m_pCurrentAction->passes[kRdrPass_UI];
	pPass->viewport = Rect(0.f, 0.f, (float)m_viewWidth, (float)m_viewHeight);
	pPass->bEnabled = true;
	pPass->aRenderTargets[0] = primaryRenderTarget;
	pPass->bAlphaBlend = true;
	pPass->depthTarget = primaryDepthTarget;
	pPass->depthTestMode = kRdrDepthTestMode_None;
	pPass->shaderMode = kRdrShaderMode_Normal;
}

void Renderer::EndAction()
{
	m_queuedActions.push_back(m_pCurrentAction);
	m_pCurrentAction = nullptr;
}

void Renderer::AddToBucket(RdrDrawOp* pDrawOp, RdrBucketType bucket)
{
	m_pCurrentAction->buckets[bucket].push_back(pDrawOp);
}

void Renderer::SetPerFrameConstants(const RdrAction* pAction, const RdrPass* pPass)
{
	// VS buffer
	VsPerFrame* vsConstants = (VsPerFrame*)m_pContext->MapResource(m_hPerFrameBufferVS, kRdrResourceMap_WriteDiscard);

	Matrix44 mtxView;
	Matrix44 mtxProj;
	if (pPass->pCamera)
	{
		pPass->pCamera->GetMatrices(mtxView, mtxProj);
	}
	else
	{
		mtxView = Matrix44::kIdentity;
		mtxProj = DirectX::XMMatrixOrthographicLH((float)m_viewWidth, (float)m_viewHeight, 0.01f, 1000.f);
	}

	vsConstants->mtxViewProj = Matrix44Multiply(mtxView, mtxProj);
	vsConstants->mtxViewProj = Matrix44Transpose(vsConstants->mtxViewProj);

	m_pContext->UnmapResource(m_hPerFrameBufferVS);
	m_pContext->VSSetConstantBuffers(0, 1, &m_hPerFrameBufferVS);


	// PS buffer.
	PsPerFrame* psConstants = (PsPerFrame*)m_pContext->MapResource(m_hPerFrameBufferPS, kRdrResourceMap_WriteDiscard);

	if (pPass->pCamera)
	{
		psConstants->viewPos = pPass->pCamera->GetPosition();
	}
	else
	{
		psConstants->viewPos = Vec3::kOrigin;
	}
	psConstants->mtxInvProj = Matrix44Inverse(mtxProj);
	psConstants->viewWidth = m_viewWidth;

	m_pContext->UnmapResource(m_hPerFrameBufferPS);
	m_pContext->PSSetConstantBuffers(0, 1, &m_hPerFrameBufferPS);
}

void Renderer::DrawPass(RdrAction* pAction, RdrPassEnum ePass)
{
	RdrPass* pPass = &pAction->passes[ePass];

	if (!pPass->bEnabled)
		return;

	m_pContext->BeginEvent(s_passNames[ePass]);

	if (pPass->bClearRenderTargets)
	{
		for (uint i = 0; i < MAX_RENDERTARGETS; ++i)
		{
			if (pPass->aRenderTargets[i].pView)
			{
				const Color clearColor(0.f, 0.f, 0.f, 1.f);
				m_pContext->ClearRenderTargetView(pPass->aRenderTargets[i], clearColor);
			}
		}
	}

	if (pPass->bClearDepthTarget)
	{
		m_pContext->ClearDepthStencilView(pPass->depthTarget, true, 1.f, true, 0);
	}

	// Clear resource bindings to avoid input/output binding errors
	m_pContext->PSClearResources();

	m_pContext->SetRenderTargets(MAX_RENDERTARGETS, pPass->aRenderTargets, pPass->depthTarget);
	m_pContext->SetDepthStencilState(pPass->depthTestMode);
	m_pContext->SetBlendState(pPass->bAlphaBlend);

	SetPerFrameConstants(pAction, pPass);

	m_pContext->SetViewport(pPass->viewport);

	std::vector<RdrDrawOp*>::iterator opIter = pAction->buckets[ s_passBuckets[ePass] ].begin();
	std::vector<RdrDrawOp*>::iterator opEndIter = pAction->buckets[ s_passBuckets[ePass] ].end();
	for ( ; opIter != opEndIter; ++opIter )
	{
		RdrDrawOp* pDrawOp = *opIter;
		if (pDrawOp->hComputeShader)
		{
			m_pContext->DispatchCompute(pDrawOp);
		}
		else
		{
			m_pContext->DrawGeo(pDrawOp, pPass->shaderMode, pAction->pLights, m_hTileLightIndices);
		}
	}

	m_pContext->EndEvent();
}

void Renderer::PostFrameSync()
{
	// Return old frame actions to the pool.
	uint numFrameActions = m_frameActions.size();
	for (uint iAction = 0; iAction < numFrameActions; ++iAction)
	{
		RdrAction* pAction = m_frameActions[iAction];

		// Free draw ops.
		for (uint iBucket = 0; iBucket < kRdrBucketType_Count; ++iBucket)
		{
			uint numOps = pAction->buckets[iBucket].size();
			std::vector<RdrDrawOp*>::iterator iter = pAction->buckets[iBucket].begin();
			std::vector<RdrDrawOp*>::iterator endIter = pAction->buckets[iBucket].end();
			for (; iter != endIter; ++iter)
			{
				RdrDrawOp* pDrawOp = *iter;
				if (pDrawOp->bFreeGeo)
				{
					m_pContext->ReleaseGeometry(pDrawOp->hGeo);
				}

				RdrDrawOp::Release(pDrawOp);
			}
			pAction->buckets[iBucket].clear();
		}

		RdrAction::Release(pAction);
	}

	// Swap in new actions
	m_frameActions = m_queuedActions;
	m_queuedActions.clear();
}

void Renderer::DrawFrame()
{
	uint numFrameActions = m_frameActions.size();
	for (uint iAction = 0; iAction < numFrameActions; ++iAction)
	{
		RdrAction* pAction = m_frameActions[iAction];
		m_pContext->BeginEvent(pAction->name);

		RdrRasterState rasterState;
		rasterState.bEnableMSAA = (s_msaaLevel > 1);
		rasterState.bEnableScissor = false;
		rasterState.bWireframe = s_wireframe;
		m_pContext->SetRasterState(rasterState);

		// todo: sort buckets 
		//std::sort(pAction->buckets[kRdrBucketType_Opaque].begin(), pAction->buckets[kRdrBucketType_Opaque].end(), );

		DrawPass(pAction, kRdrPass_ZPrepass);

		if ( pAction->bDoLightCulling )
			DispatchLightCulling(pAction);

		DrawPass(pAction, kRdrPass_Opaque);
		DrawPass(pAction, kRdrPass_Alpha);

		// UI should never be wireframe
		if (s_wireframe)
		{
			rasterState.bWireframe = false;
			m_pContext->SetRasterState(rasterState);
		}
		DrawPass(pAction, kRdrPass_UI);

		m_pContext->EndEvent();
	}

	m_pContext->Present();
}
