#pragma once

#include <windows.h>
#include <vector>
#include <set>
#include "RdrContext.h"

struct IDXGISwapChain;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11Buffer;
struct ID3DUserDefinedAnnotation;

class Camera;
class WorldObject;
struct RdrAction;
struct Light;

class Renderer
{
public:
	bool Init(HWND hWnd, int width, int height);
	void Cleanup();

	void Resize(int width, int height);

	void BeginAction(Camera* pCamera, ID3D11RenderTargetView* pRenderTarget);
	void EndAction();

	void AddToBucket(RdrDrawOp* pDrawOp, RdrBucketType bucket);

	void DrawFrame();

	RdrContext* GetContext() { return &m_context; }
	const Camera* GetCurrentCamera(void) const;

	// todo: most uses of these will be wrong when using different render targets
	int GetViewportWidth() const { return m_viewWidth; }
	int GetViewportHeight() const { return m_viewHeight; }
	Vec2 GetViewportSize() const { return Vec2((float)m_viewWidth, (float)m_viewHeight); }

	RdrTextureHandle GetDepthTex() const { return m_hDepthTex; }

	Camera& GetMainCamera() { return m_context.m_mainCamera; }

private:
	void CreatePrimaryTargets(int width, int height);

	void DrawPass(RdrAction* pAction, RdrPassEnum ePass);
	void SetPerFrameConstants(Camera* pCamera);
	void DispatchLightCulling(Camera* pCamera);

	RdrContext m_context;

	ID3DUserDefinedAnnotation* m_pAnnotator;

	IDXGISwapChain*			m_pSwapChain;
	ID3D11RenderTargetView* m_pPrimaryRenderTarget;
	ID3D11RasterizerState*	m_pRasterStateDefault;
	ID3D11RasterizerState*	m_pRasterStateWireframe;

	ID3D11DepthStencilState*	m_pDepthStencilState;
	ID3D11DepthStencilView*		m_pDepthStencilView;
	ID3D11Texture2D*			m_pDepthBuffer;
	ID3D11ShaderResourceView*	m_pDepthResourceView;
	RdrTextureHandle			m_hDepthTex;


	ID3D11Buffer* m_pPerFrameBufferVS;
	ID3D11Buffer* m_pPerFrameBufferPS;

	std::vector<RdrAction*> m_actions;
	RdrAction*				m_pCurrentAction;

	int m_viewWidth;
	int m_viewHeight;

	// Forward+ lighting
	ShaderHandle		m_hDepthMinMaxShader;
	ShaderHandle		m_hLightCullShader;
	RdrTextureHandle	m_hDepthMinMaxTex;
	int					m_tileCountX;
	int					m_tileCountY;
};