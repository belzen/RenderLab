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
struct RdrAction;
struct RdrPass;
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
	int GetViewportWidth() const { return m_pendingViewWidth; }
	int GetViewportHeight() const { return m_pendingViewHeight; }
	Vec2 GetViewportSize() const { return Vec2((float)m_pendingViewWidth, (float)m_pendingViewHeight); }

	RdrShaderSystem& GetShaderSystem() { return m_assets.shaders; }
	RdrResourceSystem& GetResourceSystem() { return m_assets.resources; }
	RdrGeoSystem& GetGeoSystem() { return m_assets.geos; }

private:
	void DrawPass(const RdrAction& rAction, RdrPassEnum ePass);
	void CreatePerActionConstants();
	void CreateCubemapCaptureConstants(const Vec3& position, const float nearDist, const float farDist);
	void QueueLightCulling();

	RdrFrameState& GetQueueState() { return m_frameStates[m_queueState]; }
	RdrFrameState& GetActiveState() { return m_frameStates[!m_queueState]; }

	RdrAction* GetNextAction();

	void DrawGeo(const RdrAction& rAction, const RdrPassEnum ePass, const RdrDrawOp* pDrawOp, const RdrLightParams& rLightParams, const RdrResourceHandle hTileLightIndices);
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