#pragma once

#include "RdrContext.h"
#include "RdrFrameState.h"
#include "RdrShaderSystem.h"
#include "RdrGeoSystem.h"
#include "RdrResourceSystem.h"
#include "RdrDrawState.h"

class Camera;
class WorldObject;
struct RdrAction;
struct RdrPass;
struct RdrDrawOp;
struct Light;
class LightList;

class Renderer
{
public:
	bool Init(HWND hWnd, int width, int height);
	void Cleanup();

	void Resize(int width, int height);

	void BeginShadowMapAction(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport);
	void BeginShadowCubeMapAction(const Light* pLight, RdrRenderTargetViewHandle* ahTargetViews, Rect& viewport);
	void BeginPrimaryAction(const Camera& rCamera, const LightList* pLights);
	void EndAction();

	void AddToBucket(RdrDrawOp* pDrawOp, RdrBucketType bucket);

	void DrawFrame();
	void PostFrameSync();

	const Camera& GetCurrentCamera(void) const;

	// todo: most uses of these will be wrong when using different render targets
	int GetViewportWidth() const { return m_viewWidth; }
	int GetViewportHeight() const { return m_viewHeight; }
	Vec2 GetViewportSize() const { return Vec2((float)m_viewWidth, (float)m_viewHeight); }

	RdrShaderSystem& GetShaderSystem() { return m_shaders; }
	RdrResourceSystem& GetResourceSystem() { return m_resources; }
	RdrGeoSystem& GetGeoSystem() { return m_geos; }

private:
	void DrawPass(const RdrAction& rAction, RdrPassEnum ePass);
	void SetPerFrameConstants(const RdrAction& rAction, const RdrPass& rPass);
	void QueueLightCulling();

	RdrFrameState& GetQueueState() { return m_frameStates[m_queueState]; }
	RdrFrameState& GetActiveState() { return m_frameStates[!m_queueState]; }

	RdrAction* GetNextAction();

	void DrawGeo(RdrDrawOp* pDrawOp, RdrShaderMode eShaderMode, const RdrLightParams& rLightParams, RdrResourceHandle hTileLightIndices);
	void DispatchCompute(RdrDrawOp* pDrawOp);

	///
	RdrContext* m_pContext;

	RdrShaderSystem   m_shaders;
	RdrResourceSystem m_resources;
	RdrGeoSystem      m_geos;

	RdrDepthStencilViewHandle m_hPrimaryDepthStencilView;
	RdrResourceHandle         m_hPrimaryDepthBuffer;

	// todo: remove these
	RdrConstantBuffer m_perFrameBufferVS;
	RdrConstantBuffer m_perFrameBufferPS;
	RdrConstantBuffer m_cubemapPerFrameBufferGS;

	RdrDrawState m_drawState;

	RdrFrameState m_frameStates[2];
	uint          m_queueState; // Index of the state being queued to by the main thread. (The other state is the active frame state).

	RdrAction* m_pCurrentAction;

	RdrResourceHandle m_hTileLightIndices;

	int m_viewWidth;
	int m_viewHeight;

	// Forward+ lighting
	RdrConstantBufferHandle m_hDepthMinMaxConstants;
	RdrConstantBufferHandle m_hTileCullConstants;
	RdrShaderHandle		m_hDepthMinMaxShader;
	RdrShaderHandle		m_hLightCullShader;
	RdrResourceHandle	m_hDepthMinMaxTex;
	int					m_tileCountX;
	int					m_tileCountY;
};