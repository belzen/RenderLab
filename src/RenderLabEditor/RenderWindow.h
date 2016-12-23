#pragma once

#include "input/CameraInputContext.h"
#include "render/Camera.h"
#include "WindowBase.h"
#include "render/Renderer.h"

class Scene;
class FrameTimer;

class RenderWindow : public WindowBase
{
public:
	static RenderWindow* Create(int x, int y, int width, int height, const Widget* pParent);

	void Close();

	void SetCameraPosition(const Vec3& position, const Vec3& pitchYawRoll);

	// Main thread updating/commands
	void EarlyUpdate();
	void Update();
	void QueueDraw(Scene& rScene);
	void PostFrameSync();

	// Render thread commands
	void DrawFrame();

private:
	RenderWindow(int x, int y, int width, int height, const Widget* pParent);

	bool HandleResize(int newWidth, int newHeight);
	bool HandleKeyDown(int key);
	bool HandleKeyUp(int key);
	bool HandleMouseDown(int button, int mx, int my);
	bool HandleMouseUp(int button, int mx, int my);
	bool HandleMouseMove(int mx, int my);

	InputManager m_inputManager;
	CameraInputContext m_defaultInputContext;
	Camera m_mainCamera;
	Renderer m_renderer;
};