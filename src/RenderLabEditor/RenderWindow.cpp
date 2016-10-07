#include "Precompiled.h"
#include "RenderWindow.h"
#include "render\Renderer.h"
#include "RenderDoc/RenderDocUtil.h"


void RenderWindow::Create(HWND hParentWnd, int width, int height, Renderer* pRenderer)
{
	m_pRenderer = pRenderer;
	WindowBase::Create(hParentWnd, width, height, "RenderViewport");

	m_pRenderer->Init(GetWindowHandle(), width, height, &m_inputManager);

	m_defaultInputContext.SetCamera(&m_mainCamera);
	m_inputManager.PushContext(&m_defaultInputContext);
}

bool RenderWindow::HandleResize(int newWidth, int newHeight)
{
	m_pRenderer->Resize(newWidth, newHeight);
	return true;
}

bool RenderWindow::HandleKeyDown(int key)
{
	m_inputManager.SetKeyDown(key, true);
	return true;
}

bool RenderWindow::HandleKeyUp(int key)
{
	m_inputManager.SetKeyDown(key, false);
	return true;
}

bool RenderWindow::HandleMouseDown(int button, int mx, int my)
{
	m_inputManager.SetMouseDown(button, true, mx, my);
	return true;
}

bool RenderWindow::HandleMouseUp(int button, int mx, int my)
{
	m_inputManager.SetMouseDown(button, false, mx, my);
	return true;
}

bool RenderWindow::HandleMouseMove(int mx, int my)
{
	m_inputManager.SetMousePos(mx, my);
	return true;
}

void RenderWindow::EarlyUpdate()
{
	m_inputManager.Reset();
}

void RenderWindow::Update(float dt)
{
	m_inputManager.GetActiveContext()->Update(m_inputManager, dt);
}

void RenderWindow::Draw(Scene& rScene, const FrameTimer& rFrameTimer)
{
	// Apply device changes (resizing, fullscreen, etc)
	m_pRenderer->ApplyDeviceChanges();

	// Primary render action
	m_pRenderer->BeginPrimaryAction(m_mainCamera, rScene);
	{
		Debug::QueueDraw(*m_pRenderer, m_mainCamera, rFrameTimer);
	}
	m_pRenderer->EndAction();
}
