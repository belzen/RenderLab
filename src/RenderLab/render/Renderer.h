#pragma once

#include "AssetLib/AssetLibForwardDecl.h"
#include "RdrContext.h"
#include "RdrFrameState.h"
#include "RdrShaderSystem.h"
#include "RdrResourceSystem.h"
#include "RdrDrawState.h"
#include "RdrPostProcess.h"
#include "RdrRequests.h"
#include "RdrProfiler.h"
#include "RdrLighting.h"
#include "RdrSky.h"

class Camera;
class Renderer;

struct RdrSurface
{
	RdrRenderTargetViewHandle hRenderTarget;
	RdrDepthStencilViewHandle hDepthTarget;
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

	void BeginPrimaryAction(Camera& rCamera, Scene& rScene);
	void BeginOffscreenAction(const wchar_t* actionName, Camera& rCamera, Scene& rScene,
		bool enablePostprocessing, const Rect& viewport, const RdrSurface& outputSurface);
	void EndAction();

	RdrResourceCommandList& GetPreFrameCommandList();
	RdrResourceCommandList& GetPostFrameCommandList();
	RdrResourceCommandList* GetActionCommandList();

	void QueueShadowMapPass(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport);
	void QueueShadowCubeMapPass(const PointLight& rLight, RdrDepthStencilViewHandle hDepthView, Rect& viewport);

	void AddDrawOpToBucket(const RdrDrawOp* pDrawOp, RdrBucketType eBucket);
	void AddComputeOpToPass(const RdrComputeOp* pComputeOp, RdrPass ePass);

	void DrawFrame();
	void PostFrameSync();

	const Camera& GetCurrentCamera() const;

	int GetViewportWidth() const;
	int GetViewportHeight() const;
	Vec2 GetViewportSize() const;

	RdrResourceReadbackRequestHandle IssueTextureReadbackRequest(RdrResourceHandle hResource, const IVec3& pixelCoord);
	RdrResourceReadbackRequestHandle IssueStructuredBufferReadbackRequest(RdrResourceHandle hResource, uint startByteOffset, uint numBytesToRead);
	void ReleaseResourceReadbackRequest(RdrResourceReadbackRequestHandle hRequest);

	void SetLightingMethod(RdrLightingMethod eLightingMethod);

	const RdrProfiler& GetProfiler() const;

	RdrResourceHandle GetPrimaryDepthBuffer() const;
	RdrResourceHandle GetPrimaryDepthStencilView() const;

private:
	void DrawPass(const RdrAction& rAction, RdrPass ePass);
	void DrawShadowPass(const RdrShadowPass& rPass);

	RdrFrameState& GetQueueState();
	RdrFrameState& GetActiveState();

	RdrAction* GetNextAction(const wchar_t* actionName, const Rect& viewport, bool enablePostProcessing, const RdrSurface& outputSurface);

	void DrawBucket(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants, const RdrLightResources& rLightParams);
	void DrawGeo(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants,
		const RdrDrawOp* pDrawOp, const RdrLightResources& rLightParams, uint instanceCount);
	void DispatchCompute(const RdrComputeOp* pComputeOp);

	void ProcessReadbackRequests();

	void QueueScene(const Scene& rScene);

private:
	///
	RdrContext* m_pContext;

	RdrProfiler m_profiler;
	InputManager* m_pInputManager;

	RdrDepthStencilViewHandle m_hPrimaryDepthStencilView;
	RdrResourceHandle         m_hPrimaryDepthBuffer;

	RdrResourceHandle         m_hColorBuffer;
	RdrResourceHandle         m_hColorBufferMultisampled;
	RdrRenderTargetViewHandle m_hColorBufferRenderTarget;

	RdrDrawState m_drawState;
	RdrLighting m_lighting;

	RdrFrameState m_frameStates[2];
	uint          m_queueState; // Index of the state being queued to by the main thread. (The other state is the active frame state).

	RdrAction* m_pCurrentAction;

	int m_viewWidth;
	int m_viewHeight;
	int m_pendingViewWidth;
	int m_pendingViewHeight;

	RdrLightingMethod m_ePendingLightingMethod;
	RdrLightingMethod m_eLightingMethod;

	RdrSky m_sky;
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
	return m_pCurrentAction ? (int)m_pCurrentAction->primaryViewport.width : m_viewWidth;
}

inline int Renderer::GetViewportHeight() const 
{ 
	return m_pCurrentAction ? (int)m_pCurrentAction->primaryViewport.height : m_viewHeight; 
}

inline Vec2 Renderer::GetViewportSize() const 
{
	return m_pCurrentAction ?
		Vec2(m_pCurrentAction->primaryViewport.width, m_pCurrentAction->primaryViewport.height) :
		Vec2((float)m_viewWidth, (float)m_viewHeight);
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

inline RdrResourceHandle Renderer::GetPrimaryDepthBuffer() const
{
	return m_hPrimaryDepthBuffer;
}

inline RdrResourceHandle Renderer::GetPrimaryDepthStencilView() const
{
	return m_hPrimaryDepthStencilView;
}