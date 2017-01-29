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
#include "components/ModelComponent.h"
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

	Resize(width, height);
	ApplyDeviceChanges();

	Font::Init();
	m_postProcess.Init(m_pContext);

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

void Renderer::SetLightingMethod(RdrLightingMethod eLightingMethod)
{
	m_ePendingLightingMethod = eLightingMethod;
	RdrShaderSystem::SetGlobalShaderDefine("CLUSTERED_LIGHTING", (eLightingMethod == RdrLightingMethod::Clustered));
}

void Renderer::QueueAction(RdrAction* pAction)
{
	GetQueueState().actions.push(pAction);
}

RdrResourceCommandList& Renderer::GetPreFrameCommandList()
{
	return GetQueueState().preFrameResourceCommands;
}

RdrResourceCommandList& Renderer::GetPostFrameCommandList()
{
	return GetQueueState().postFrameResourceCommands;
}

void Renderer::DrawPass(const RdrAction& rAction, RdrPass ePass)
{
	const RdrPassData& rPass = rAction.m_passes[(int)ePass];

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
		const RdrComputeOpBucket& rComputeBucket = rAction.GetComputeOpBucket(ePass);
		for (const RdrComputeOp* pComputeOp : rComputeBucket)
		{
			DispatchCompute(pComputeOp);
		}
	}

	// Draw ops
	const RdrDrawOpBucket& rBucket = rAction.GetDrawOpBucket(s_passBuckets[(int)ePass]);
	const RdrGlobalConstants& rGlobalConstants = (ePass == RdrPass::UI) ? rAction.m_uiConstants : rAction.m_constants;
	DrawBucket(rPass, rBucket, rGlobalConstants, rAction.m_lightResources);

	m_pContext->EndEvent();
	m_profiler.EndSection();
}

void Renderer::DrawShadowPass(const RdrAction& rAction, int shadowPassIndex)
{
	const RdrShadowPass& rShadowPass = rAction.m_shadowPasses[shadowPassIndex];
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
	const RdrDrawOpBucket& rDrawBucket = rAction.GetDrawOpBucket((RdrBucketType)((int)RdrBucketType::ShadowMap0 + shadowPassIndex));
	DrawBucket(rPassData, rDrawBucket, rShadowPass.constants, lightParams);

	m_pContext->EndEvent();
}

void Renderer::PostFrameSync()
{
	//////////////////////////////////////////////////////////////////////////
	// Finalize pending frame data.
	{
		RdrFrameState& rQueueState = GetQueueState();

		// Sort buckets and build object index buffers
		for (RdrAction* pAction : rQueueState.actions)
		{
			for (int i = 0; i < (int)RdrBucketType::Count; ++i)
			{
				pAction->SortDrawOps((RdrBucketType)i);
			}
		}

		// Update global data
		RdrInstancedObjectDataBuffer::UpdateBuffer(*this);
	}

	//////////////////////////////////////////////////////////////////////////
	// Clear the data that we just rendered with.
	RdrFrameState& rActiveState = GetActiveState();
	RdrResourceCommandList& rResCommandList = GetPostFrameCommandList();

	// Release actions.
	for (RdrAction* pAction : rActiveState.actions)
	{
		pAction->Release();
	}

	// Clear remaining state data.
	rActiveState.actions.clear();

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
		for (RdrAction* pAction : rFrameState.actions)
		{
			// Process resource system commands for this action
			pAction->m_resourceCommands.ProcessCommands(m_pContext);

			m_pContext->BeginEvent(pAction->m_name);

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
			for (int iShadow = 0; iShadow < pAction->m_shadowPassCount; ++iShadow)
			{
				DrawShadowPass(*pAction, iShadow);
			}
			m_profiler.EndSection();

			//////////////////////////////////////////////////////////////////////////
			// Normal draw passes
			rasterState.bEnableMSAA = (g_debugState.msaaLevel > 1);
			rasterState.bEnableScissor = false;
			rasterState.bWireframe = g_debugState.wireframe;
			rasterState.bUseSlopeScaledDepthBias = 0;
			m_pContext->SetRasterState(rasterState);

			DrawPass(*pAction, RdrPass::ZPrepass);
			DrawPass(*pAction, RdrPass::LightCulling);
			DrawPass(*pAction, RdrPass::VolumetricFog);
			DrawPass(*pAction, RdrPass::Opaque);
			DrawPass(*pAction, RdrPass::Sky);
			DrawPass(*pAction, RdrPass::Alpha);

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

			if (pAction->m_bEnablePostProcessing)
			{
				m_profiler.BeginSection(RdrProfileSection::PostProcessing);
				m_postProcess.DoPostProcessing(*m_pInputManager, m_pContext, m_drawState, pColorBuffer, pAction->m_postProcessEffects);
				m_profiler.EndSection();
			}

			// Wireframe
			{
				rasterState.bWireframe = true;
				m_pContext->SetRasterState(rasterState);

				DrawPass(*pAction, RdrPass::Wireframe);

				rasterState.bWireframe = false;
				m_pContext->SetRasterState(rasterState);
			}

			DrawPass(*pAction, RdrPass::Editor);
			DrawPass(*pAction, RdrPass::UI);

			m_pContext->EndEvent();
		}
	}
	else
	{
		// Process commands even though we're not rendering.
		for (RdrAction* pAction : rFrameState.actions)
		{
			pAction->m_resourceCommands.ProcessCommands(m_pContext);
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
		const RdrConstantBuffer& rBuffer = m_instanceIds[m_currentInstanceIds].buffer;
		m_pContext->UpdateConstantBuffer(rBuffer.bufferObj, m_instanceIds[m_currentInstanceIds].ids, rBuffer.size);
		m_drawState.vsConstantBuffers[1] = rBuffer.bufferObj;
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
		if (rPass.pOverridePixelShader)
		{
			m_drawState.pPixelShader = rPass.pOverridePixelShader;
		}
		else
		{
			m_drawState.pPixelShader = pMaterial->hPixelShaders[(int)rPass.shaderMode] ?
				RdrShaderSystem::GetPixelShader(pMaterial->hPixelShaders[(int)rPass.shaderMode]) :
				nullptr;
		}

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
			m_drawState.psConstantBufferCount = 1;

			if (pMaterial->hConstants)
			{
				m_drawState.psConstantBuffers[1] = RdrResourceSystem::GetConstantBuffer(pMaterial->hConstants)->bufferObj;
				m_drawState.psConstantBufferCount = 2;
			}

			if (pMaterial->bNeedsLighting && !bDepthOnly)
			{
				m_drawState.psConstantBuffers[2] = RdrResourceSystem::GetConstantBuffer(rLightParams.hGlobalLightsCb)->bufferObj;
				m_drawState.psConstantBuffers[3] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hPsAtmosphere)->bufferObj;
				m_drawState.psConstantBufferCount = 4;

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
