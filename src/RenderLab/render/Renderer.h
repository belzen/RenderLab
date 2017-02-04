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
	RdrResourceCommandList preFrameResourceCommands;
	RdrResourceCommandList postFrameResourceCommands;
};

// Global pointer to active renderer.
extern Renderer* g_pRenderer;

class Renderer
{
public:
	bool Init(HWND hWnd, int width, int height, InputManager* pInputManager);
	void Cleanup();

	void Resize(int width, int height);

	void ApplyDeviceChanges();

	void QueueAction(RdrAction* pAction);

	RdrResourceCommandList& GetPreFrameCommandList();
	RdrResourceCommandList& GetPostFrameCommandList();

	void DrawFrame();
	void PostFrameSync();

	int GetViewportWidth() const;
	int GetViewportHeight() const;
	Vec2 GetViewportSize() const;

	RdrResourceReadbackRequestHandle IssueTextureReadbackRequest(RdrResourceHandle hResource, const IVec3& pixelCoord);
	RdrResourceReadbackRequestHandle IssueStructuredBufferReadbackRequest(RdrResourceHandle hResource, uint startByteOffset, uint numBytesToRead);
	void ReleaseResourceReadbackRequest(RdrResourceReadbackRequestHandle hRequest);

	void SetLightingMethod(RdrLightingMethod eLightingMethod);

	const RdrProfiler& GetProfiler() const;

	RdrLightingMethod GetLightingMethod() const;

	const RdrPostProcess& GetPostProcess() const;

private:
	void DrawPass(const RdrAction& rAction, RdrPass ePass);
	void DrawShadowPass(const RdrAction& rAction, int shadowPassIndex);

	RdrFrameState& GetQueueState();
	RdrFrameState& GetActiveState();

	void DrawBucket(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants, const RdrLightResources& rLightParams);
	void DrawGeo(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants,
		const RdrDrawOp* pDrawOp, const RdrLightResources& rLightParams, uint instanceCount);
	void DispatchCompute(const RdrComputeOp* pComputeOp);

	void ProcessReadbackRequests();

private:
	///
	RdrContext* m_pContext;

	RdrProfiler m_profiler;
	InputManager* m_pInputManager;

	RdrDrawState m_drawState;

	RdrFrameState m_frameStates[2];
	uint          m_queueState; // Index of the state being queued to by the main thread. (The other state is the active frame state).

	int m_viewWidth;
	int m_viewHeight;
	int m_pendingViewWidth;
	int m_pendingViewHeight;

	RdrLightingMethod m_ePendingLightingMethod;
	RdrLightingMethod m_eLightingMethod;

	RdrPostProcess m_postProcess;

	RdrResourceReadbackRequestList m_readbackRequests; // Average out to a max of 32 requests per frame
	std::vector<RdrResourceReadbackRequestHandle> m_pendingReadbackRequests;
	ThreadMutex m_readbackMutex;

	// Instance object ID data.
	struct
	{
		RdrConstantBuffer buffer;
		char* pData; // Original non-aligned pointer.
		uint* ids;
	} m_instanceIds[8];
	uint m_currentInstanceIds;
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

inline const RdrProfiler& Renderer::GetProfiler() const
{
	return m_profiler;
}

inline RdrLightingMethod Renderer::GetLightingMethod() const
{
	return m_eLightingMethod;
}

inline const RdrPostProcess& Renderer::GetPostProcess() const
{
	return m_postProcess;
}
