#pragma once

#include "input/CameraInputContext.h"
#include "render/Camera.h"
#include "WindowBase.h"

class Renderer;
class Scene;
class FrameTimer;

class RenderWindow : public WindowBase
{
public:
	void Create(HWND hParentWnd, int width, int height, Renderer* pRenderer);

	void EarlyUpdate();
	void Update(float dt);
	void Draw(Scene& rScene, const FrameTimer& rFrameTimer);

private:
	bool HandleResize(int newWidth, int newHeight);
	bool HandleKeyDown(int key);
	bool HandleKeyUp(int key);
	bool HandleMouseDown(int button, int mx, int my);
	bool HandleMouseUp(int button, int mx, int my);
	bool HandleMouseMove(int mx, int my);

	InputManager m_inputManager;
	CameraInputContext m_defaultInputContext;
	Camera m_mainCamera;
	Renderer* m_pRenderer;
};