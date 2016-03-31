#pragma once

#include "RdrContext.h"
#include "RdrFrameState.h"

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

	void BeginShadowMapAction(const Camera& rCamera, RdrDepthStencilView depthView, Rect& viewport);
	void BeginShadowCubeMapAction(const Light* pLight, RdrRenderTargetView* pTargetViews, Rect& viewport);
	void BeginPrimaryAction(const Camera& rCamera, const LightList* pLights);
	void EndAction();

	void AddToBucket(RdrDrawOp* pDrawOp, RdrBucketType bucket);

	void DrawFrame();
	void PostFrameSync();

	RdrContext* GetContext() { return m_pContext; }
	const Camera& GetCurrentCamera(void) const;

	// todo: most uses of these will be wrong when using different render targets
	int GetViewportWidth() const { return m_viewWidth; }
	int GetViewportHeight() const { return m_viewHeight; }
	Vec2 GetViewportSize() const { return Vec2((float)m_viewWidth, (float)m_viewHeight); }

private:
	void DrawPass(const RdrAction& rAction, RdrPassEnum ePass);
	void SetPerFrameConstants(const RdrAction& rAction, const RdrPass& rPass);
	void DispatchLightCulling(const RdrAction& rAction);
	RdrAction* GetNextAction();

	RdrContext* m_pContext;

	RdrResourceHandle m_hPerFrameBufferVS;
	RdrResourceHandle m_hPerFrameBufferPS;
	RdrResourceHandle m_hCubemapPerFrameBufferGS;

	RdrFrameState m_states[2];
	uint          m_queueState; // Index of the state being queued to by the main thread. (The other state is the active frame state).

	RdrAction* m_pCurrentAction;

	RdrResourceHandle m_hTileLightIndices;

	int m_viewWidth;
	int m_viewHeight;

	// Forward+ lighting
	RdrShaderHandle		m_hDepthMinMaxShader;
	RdrShaderHandle		m_hLightCullShader;
	RdrResourceHandle	m_hDepthMinMaxTex;
	int					m_tileCountX;
	int					m_tileCountY;
};