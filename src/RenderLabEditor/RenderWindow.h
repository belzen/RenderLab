#pragma once

#include "input/CameraInputContext.h"
#include "render/Camera.h"
#include "WindowBase.h"
#include "render/Renderer.h"

class SceneViewModel;
class FrameTimer;

namespace Physics
{
	struct RaycastResult;
}

class RenderWindow : public WindowBase
{
public:
	static RenderWindow* Create(int x, int y, int width, int height, SceneViewModel* pSceneViewModel, const Widget* pParent);

	void Close();

	void SetCameraPosition(const Vec3& position, const Vec3& pitchYawRoll);

	// Main thread updating/commands
	void EarlyUpdate();
	void Update();
	void QueueDraw();
	void PostFrameSync();

	// Render thread commands
	void DrawFrame();

private:
	RenderWindow(int x, int y, int width, int height, SceneViewModel* pSceneViewModel, const Widget* pParent);

	//////////////////////////////////////////////////////////////////////////
	// Input event handling
	bool OnResize(int newWidth, int newHeight);
	bool OnKeyDown(int key);
	bool OnKeyUp(int key);
	bool OnChar(char c);
	bool OnMouseDown(int button, int mx, int my);
	bool OnMouseUp(int button, int mx, int my);
	bool OnMouseMove(int mx, int my);
	bool OnMouseEnter(int mx, int my);
	bool OnMouseLeave();
	void OnDragEnd(Widget* pDraggedWidget);
	bool ShouldCaptureMouse() const;

	bool RaycastAtCursor(int mx, int my, Physics::RaycastResult* pResult);

	//////////////////////////////////////////////////////////////////////////
	void CancelObjectPlacement();

private:
	InputManager m_inputManager;
	CameraInputContext m_defaultInputContext;
	Camera m_mainCamera;
	Renderer m_renderer;
	SceneViewModel* m_pSceneViewModel;

	WorldObject* m_pPlacingObject;
};