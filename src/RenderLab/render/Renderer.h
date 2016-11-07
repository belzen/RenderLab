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

class Camera;
class WorldObject;
struct RdrDrawOp;
struct Light;
class LightList;
class RdrPostProcessEffects;

enum class RdrLightingMethod
{
	Tiled,
	Clustered
};

struct RdrTiledLightingData
{
	RdrResourceHandle       hLightIndices;
	RdrConstantBufferHandle hDepthMinMaxConstants;
	RdrConstantBufferHandle hCullConstants;
	RdrResourceHandle	    hDepthMinMaxTex;
	int					    tileCountX;
	int					    tileCountY;
};

struct RdrClusteredLightingData
{
	RdrResourceHandle       hLightIndices;
	RdrConstantBufferHandle hCullConstants;
	int					    clusterCountX;
	int					    clusterCountY;
	int					    clusterCountZ;
};

struct RdrVolumetricFogData
{
	RdrConstantBufferHandle hFogConstants;
	RdrResourceHandle hDensityLightLut;
	RdrResourceHandle hFinalLut;
	UVec3 lutSize;
};

class Renderer
{
public:
	bool Init(HWND hWnd, int width, int height, InputManager* pInputManager);
	void Cleanup();

	void Resize(int width, int height);

	void ApplyDeviceChanges();

	void BeginPrimaryAction(const Camera& rCamera, Scene& rScene, float dt);
	void EndAction();

	void QueueShadowMapPass(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport);
	void QueueShadowCubeMapPass(const Light* pLight, RdrDepthStencilViewHandle hDepthView, Rect& viewport);

	void AddDrawOpToBucket(const RdrDrawOp* pDrawOp, RdrBucketType eBucket);
	void AddComputeOpToPass(const RdrComputeOp* pComputeOp, RdrPass ePass);

	void DrawFrame();
	void PostFrameSync();

	const Camera& GetCurrentCamera(void) const;

	int GetViewportWidth() const;
	int GetViewportHeight() const;
	Vec2 GetViewportSize() const;

	RdrResourceReadbackRequestHandle IssueTextureReadbackRequest(RdrResourceHandle hResource, const IVec3& pixelCoord);
	RdrResourceReadbackRequestHandle IssueStructuredBufferReadbackRequest(RdrResourceHandle hResource, uint startByteOffset, uint numBytesToRead);
	void ReleaseResourceReadbackRequest(RdrResourceReadbackRequestHandle hRequest);

	void SetLightingMethod(RdrLightingMethod eLightingMethod);

	const RdrProfiler& GetProfiler();

private:
	void DrawPass(const RdrAction& rAction, RdrPass ePass);
	void DrawShadowPass(const RdrShadowPass& rPass);
	void QueueTiledLightCulling();
	void QueueClusteredLightCulling();
	void QueueVolumetricFog(const AssetLib::VolumetricFogSettings& rFogSettings);

	RdrFrameState& GetQueueState();
	RdrFrameState& GetActiveState();

	RdrAction* GetNextAction();

	void DrawBucket(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants, const RdrLightParams& rLightParams);
	void DrawGeo(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants,
		const RdrDrawOp* pDrawOp, const RdrLightParams& rLightParams, const RdrResourceHandle hTileLightIndices, uint instanceCount);
	void DispatchCompute(const RdrComputeOp* pComputeOp);

	void ProcessReadbackRequests();

	RdrResourceHandle GetLightIdsResource() const;

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

	RdrFrameState m_frameStates[2];
	uint          m_queueState; // Index of the state being queued to by the main thread. (The other state is the active frame state).

	RdrAction* m_pCurrentAction;

	int m_viewWidth;
	int m_viewHeight;
	int m_pendingViewWidth;
	int m_pendingViewHeight;

	RdrLightingMethod m_ePendingLightingMethod;
	RdrLightingMethod m_eLightingMethod;
	RdrTiledLightingData m_tiledLightData;
	RdrClusteredLightingData m_clusteredLightData;
	RdrVolumetricFogData m_volumetricFogData;

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

inline const RdrProfiler& Renderer::GetProfiler()
{
	return m_profiler;
}