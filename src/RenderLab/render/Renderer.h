#pragma once

#include "AssetLib/AssetLibForwardDecl.h"
#include "RdrContext.h"
#include "RdrAction.h"
#include "RdrShaderSystem.h"
#include "RdrResourceSystem.h"
#include "RdrDrawState.h"
#include "RdrPostProcess.h"
#include "RdrRequests.h"
#include "RdrProfiler.h"
#include "RdrLighting.h"
#include "RdrSky.h"
#include "RdrAction.h"

class Camera;
class Renderer;

struct RdrFrameState
{
	FixedVector<RdrAction*, MAX_ACTIONS_PER_FRAME> actions;
	RdrResourceCommandList resourceCommands;
};

// Global pointer to active renderer.
extern Renderer* g_pRenderer;

enum class RdrRenderStage
{
	kScene_ZPrepass,
	kScene_GBuffer,
	kScene,
	kShadowMap,
	kUI,
	kPrimary
};

class Renderer
{
public:
	static void SetRenderThread(DWORD nThreadId) { s_nRenderThreadId = nThreadId; }
	static DWORD GetRenderThreadId() { return s_nRenderThreadId; }
	static bool IsRenderThread() { return GetCurrentThreadId() == s_nRenderThreadId; }

	bool Init(HWND hWnd, int width, int height, const InputManager* pInputManager);
	void Cleanup();

	void Resize(int width, int height);

	void ApplyDeviceChanges();

	void QueueAction(RdrAction* pAction);

	RdrResourceCommandList& GetResourceCommandList();

	void DrawFrame();
	void PostFrameSync();

	int GetViewportWidth() const;
	int GetViewportHeight() const;
	Vec2 GetViewportSize() const;

	RdrResourceReadbackRequestHandle IssueTextureReadbackRequest(RdrResourceHandle hResource, const IVec3& pixelCoord);
	RdrResourceReadbackRequestHandle IssueStructuredBufferReadbackRequest(RdrResourceHandle hResource, uint startByteOffset, uint numBytesToRead);
	void ReleaseResourceReadbackRequest(RdrResourceReadbackRequestHandle hRequest);

	void SetLightingMethod(RdrLightingMethod eLightingMethod);

	const RdrGpuProfiler& GetProfiler() const;

	RdrLightingMethod GetLightingMethod() const;

	static void GetStageRenderTargetFormats(RdrRenderStage eStage, const RdrResourceFormat** ppOutFormats, uint* pOutNumFormats);

	RdrContext* GetContext();

private:
	RdrFrameState& GetQueueState();
	RdrFrameState& GetActiveState();

	void ProcessReadbackRequests();

	void DrawCpuProfiler();

private:
	static DWORD s_nRenderThreadId;

	///
	RdrContext* m_pContext;

	RdrGpuProfiler m_gpuProfiler;

	RdrDrawState m_drawState;

	RdrFrameState m_frameStates[2];
	uint          m_queueState; // Index of the state being queued to by the main thread. (The other state is the active frame state).

	int m_viewWidth;
	int m_viewHeight;
	int m_pendingViewWidth;
	int m_pendingViewHeight;

	RdrLightingMethod m_ePendingLightingMethod;
	RdrLightingMethod m_eLightingMethod;

	RdrResourceReadbackRequestList m_readbackRequests; // Average out to a max of 32 requests per frame
	std::vector<RdrResourceReadbackRequestHandle> m_pendingReadbackRequests;
	ThreadMutex m_readbackMutex;
};

inline int Renderer::GetViewportWidth() const 
{ 
	return m_viewWidth;
}

inline int Renderer::GetViewportHeight() const 
{ 
	return m_viewHeight; 
}

inline Vec2 Renderer::GetViewportSize() const 
{
	return Vec2((float)m_viewWidth, (float)m_viewHeight);
}

inline RdrFrameState& Renderer::GetQueueState()
{ 
	return m_frameStates[m_queueState]; 
}

inline RdrFrameState& Renderer::GetActiveState()
{ 
	return m_frameStates[!m_queueState]; 
}

inline const RdrGpuProfiler& Renderer::GetProfiler() const
{
	return m_gpuProfiler;
}

inline RdrLightingMethod Renderer::GetLightingMethod() const
{
	return m_eLightingMethod;
}

inline RdrContext* Renderer::GetContext()
{
	return m_pContext;
}
