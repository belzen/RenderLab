#pragma once

#include "RdrContext.h"
#include "RdrFrameState.h"
#include "RdrShaderSystem.h"
#include "RdrGeoSystem.h"
#include "RdrResourceSystem.h"
#include "RdrDrawState.h"
#include "RdrPostProcess.h"

class Camera;
class WorldObject;
struct RdrDrawOp;
struct Light;
class LightList;

struct RdrAssetSystems
{
	RdrShaderSystem   shaders;
	RdrResourceSystem resources;
	RdrGeoSystem      geos;
};

class Renderer
{
public:
	bool Init(HWND hWnd, int width, int height);
	void Cleanup();

	void Resize(int width, int height);

	void ApplyDeviceChanges();

	void BeginShadowMapAction(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport);
	void BeginShadowCubeMapAction(const Light* pLight, RdrDepthStencilViewHandle hDepthView, Rect& viewport);
	void BeginPrimaryAction(const Camera& rCamera, const LightList* pLights);
	void EndAction();

	void AddToBucket(RdrDrawOp* pDrawOp, RdrBucketType bucket);

	void DrawFrame();
	void PostFrameSync();

	const Camera& GetCurrentCamera(void) const;

	// todo: most uses of these will be wrong when using different render targets
	int GetViewportWidth() const;
	int GetViewportHeight() const;
	Vec2 GetViewportSize() const;

	RdrShaderSystem& GetShaderSystem();
	RdrResourceSystem& GetResourceSystem();
	RdrGeoSystem& GetGeoSystem();

private:
	void DrawPass(const RdrAction& rAction, RdrPass ePass);
	void CreatePerActionConstants();
	void CreateCubemapCaptureConstants(const Vec3& position, const float nearDist, const float farDist);
	void QueueLightCulling();

	RdrFrameState& GetQueueState();
	RdrFrameState& GetActiveState();

	RdrAction* GetNextAction();

	void DrawGeo(const RdrAction& rAction, const RdrPass ePass, const RdrDrawOp* pDrawOp, const RdrLightParams& rLightParams, const RdrResourceHandle hTileLightIndices);
	void DispatchCompute(RdrDrawOp* pDrawOp);

	///
	RdrContext* m_pContext;

	RdrAssetSystems m_assets;

	RdrDepthStencilViewHandle m_hPrimaryDepthStencilView;
	RdrResourceHandle         m_hPrimaryDepthBuffer;

	RdrResourceHandle         m_hColorBuffer;
	RdrResourceHandle         m_hColorBufferMultisampled;
	RdrRenderTargetViewHandle m_hColorBufferRenderTarget;

	RdrDrawState m_drawState;

	RdrFrameState m_frameStates[2];
	uint          m_queueState; // Index of the state being queued to by the main thread. (The other state is the active frame state).

	RdrAction* m_pCurrentAction;

	RdrResourceHandle m_hTileLightIndices;

	int m_viewWidth;
	int m_viewHeight;
	int m_pendingViewWidth;
	int m_pendingViewHeight;

	// Forward+ lighting
	RdrConstantBufferHandle m_hDepthMinMaxConstants;
	RdrConstantBufferHandle m_hTileCullConstants;
	RdrResourceHandle	m_hDepthMinMaxTex;
	int					m_tileCountX;
	int					m_tileCountY;

	RdrPostProcess m_postProcess;
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

inline RdrShaderSystem& Renderer::GetShaderSystem()
{ 
	return m_assets.shaders; 
}

inline RdrResourceSystem& Renderer::GetResourceSystem()
{ 
	return m_assets.resources;
}

inline RdrGeoSystem& Renderer::GetGeoSystem()
{ 
	return m_assets.geos; 
}

inline RdrFrameState& Renderer::GetQueueState()
{ 
	return m_frameStates[m_queueState]; 
}

inline RdrFrameState& Renderer::GetActiveState()
{ 
	return m_frameStates[!m_queueState]; 
}