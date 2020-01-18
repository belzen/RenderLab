#include "Precompiled.h"
#include "Renderer.h"
#include "Camera.h"
#include "Entity.h"
#include "Font.h"
#include <DXGIFormat.h>
#include "debug\DebugConsole.h"
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

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

Renderer* g_pRenderer = nullptr;
DWORD Renderer::s_nRenderThreadId = 0;

namespace
{
	void cmdSetLightingMethod(DebugCommandArg *args, int numArgs)
	{
		g_pRenderer->SetLightingMethod((RdrLightingMethod)args[0].val.inum);
	}
}

//////////////////////////////////////////////////////

bool Renderer::Init(HWND hWnd, int width, int height, const InputManager* pInputManager)
{
	Assert(g_pRenderer == nullptr);
	g_pRenderer = this;

	DebugConsole::RegisterCommand("lightingMethod", cmdSetLightingMethod, DebugCommandArgType::Integer);

	m_pContext = new RdrContext(m_profiler);
	if (!m_pContext->Init(hWnd, width, height))
		return false;

	m_profiler.Init(m_pContext);

	RdrResourceSystem::Init(*this);

	// Set default lighting method before initialized shaders so global defines can be applied first.
	m_eLightingMethod = RdrLightingMethod::Clustered;
	SetLightingMethod(m_eLightingMethod);

	RdrShaderSystem::Init(m_pContext);
	RdrAction::InitSharedData(m_pContext, pInputManager);

	Resize(width, height);
	ApplyDeviceChanges();

	Font::Init();

	// Setup ImGui
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		RdrDescriptors* pImGuiFontSrv = m_pContext->GetSrvHeap().AllocateDescriptor(CREATE_BACKPOINTER(this));

		ImGui_ImplWin32_Init(hWnd);
		ImGui_ImplDX12_Init(m_pContext->GetDevice(), kNumBackBuffers,
			DXGI_FORMAT_R8G8B8A8_UNORM, nullptr,
			pImGuiFontSrv->GetCpuDesc(), pImGuiFontSrv->GetGpuDesc());
	}

	return true;
}

void Renderer::Cleanup()
{
	// todo: flip frame to finish resource frees.
	m_profiler.Cleanup();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

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
		RdrResourceCommandList& rResCommandList = GetResourceCommandList();
		m_pContext->Resize(m_pendingViewWidth, m_pendingViewHeight);

		m_viewWidth = m_pendingViewWidth;
		m_viewHeight = m_pendingViewHeight;
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

RdrResourceCommandList& Renderer::GetResourceCommandList()
{
	return GetQueueState().resourceCommands;
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
			pSrc->CopyResourceRegion(*m_pContext, pRequest->srcRegion, *pDst, IVec3::kZero);
		}
		else if (pRequest->frameCount == 3)
		{
			// After 2 frames, we can safely read from the resource without stalling.
			const RdrResource* pDst = RdrResourceSystem::GetResource(pRequest->hDstResource);
			pDst->ReadResource(*m_pContext, pRequest->pData, pRequest->dataSize);
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

	m_pContext->BeginFrame();

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Process threaded render commands.
	RdrShaderSystem::ProcessCommands(m_pContext);
	rFrameState.resourceCommands.ProcessPreFrameCommands(m_pContext);

	ProcessReadbackRequests();

	m_profiler.BeginFrame();

	// Draw the frame
	if (!m_pContext->IsIdle()) // If the device is idle (probably minimized), don't bother rendering anything.
	{
		for (RdrAction* pAction : rFrameState.actions)
		{
			pAction->Draw(m_pContext, &m_drawState, &m_profiler);
		}
	}
	else
	{
		// Process required commands even though we're not rendering.
		for (RdrAction* pAction : rFrameState.actions)
		{
			pAction->DrawIdle(m_pContext);
		}
	}

	//g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_pContext->GetCommandList());

	m_profiler.EndFrame();
	m_pContext->Present();

	// Process post-frame commands.
	rFrameState.resourceCommands.ProcessCleanupCommands(m_pContext);
}

RdrResourceReadbackRequestHandle Renderer::IssueTextureReadbackRequest(RdrResourceHandle hResource, const IVec3& pixelCoord)
{
	RdrResourceReadbackRequest* pReq = m_readbackRequests.alloc();
	const RdrResource* pSrcResource = RdrResourceSystem::GetResource(hResource);
	pReq->hSrcResource = hResource;
	pReq->frameCount = 0;
	pReq->bComplete = false;
	pReq->srcRegion = RdrBox(pixelCoord.x, pixelCoord.y, 0, 1, 1, 1);
	pReq->hDstResource = RdrResourceSystem::CreateTexture2D(1, 1, pSrcResource->GetTextureInfo().format,
		RdrResourceAccessFlags::CpuRO_GpuRW, nullptr, CREATE_BACKPOINTER(this));
	pReq->dataSize = rdrGetTexturePitch(1, pSrcResource->GetTextureInfo().format);
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
	pReq->hDstResource = RdrResourceSystem::CreateStructuredBuffer(nullptr, 1, numBytesToRead, RdrResourceAccessFlags::CpuRO_GpuRW, CREATE_BACKPOINTER(this));
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
	GetResourceCommandList().ReleaseResource(pReq->hDstResource, CREATE_BACKPOINTER(this));
	SAFE_DELETE(pReq->pData);

	m_readbackRequests.releaseId(hRequest);
}

namespace
{
	static constexpr RdrResourceFormat kSceneGBufferRtvFormats[] = { RdrResourceFormat::R16G16B16A16_FLOAT,  RdrResourceFormat::B8G8R8A8_UNORM,  RdrResourceFormat::B8G8R8A8_UNORM };
	static constexpr RdrResourceFormat kSceneRtvFormats[] = { RdrResourceFormat::R16G16B16A16_FLOAT };
	static constexpr RdrResourceFormat kUIRtvFormats[] = { RdrResourceFormat::R8G8B8A8_UNORM };
	static constexpr RdrResourceFormat kPrimaryRtvFormats[] = { RdrResourceFormat::R8G8B8A8_UNORM };
	static constexpr RdrResourceFormat kShadowMapRtvFormats[] = { kDefaultDepthFormat };
}

void Renderer::GetStageRenderTargetFormats(RdrRenderStage eStage, const RdrResourceFormat** ppOutFormats, uint* pOutNumFormats)
{
	switch (eStage)
	{
	case RdrRenderStage::kScene_ZPrepass:
		*ppOutFormats = nullptr;
		*pOutNumFormats = 0;
		break;
	case RdrRenderStage::kScene_GBuffer:
		*ppOutFormats = kSceneGBufferRtvFormats;
		*pOutNumFormats = ARRAY_SIZE(kSceneGBufferRtvFormats);
		break;
	case RdrRenderStage::kScene:
		*ppOutFormats = kSceneRtvFormats;
		*pOutNumFormats = ARRAY_SIZE(kSceneRtvFormats);
		break;
	case RdrRenderStage::kUI:
		*ppOutFormats = kUIRtvFormats;
		*pOutNumFormats = ARRAY_SIZE(kUIRtvFormats);
		break;
	case RdrRenderStage::kPrimary:
		*ppOutFormats = kPrimaryRtvFormats;
		*pOutNumFormats = ARRAY_SIZE(kPrimaryRtvFormats);
		break;
	case RdrRenderStage::kShadowMap:
		*ppOutFormats = kShadowMapRtvFormats;
		*pOutNumFormats = ARRAY_SIZE(kShadowMapRtvFormats);
		break;
	default:
		Assert(false);
		break;
	}
}
