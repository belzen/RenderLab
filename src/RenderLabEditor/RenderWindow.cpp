#include "Precompiled.h"
#include "RenderWindow.h"
#include "render\Renderer.h"
#include "RenderDoc/RenderDocUtil.h"
#include "render\RdrOffscreenTasks.h"

RenderWindow* RenderWindow::Create(int x, int y, int width, int height, const Widget* pParent)
{
	return new RenderWindow(x, y, width, height, pParent);
}

RenderWindow::RenderWindow(int x, int y, int width, int height, const Widget* pParent)
	: WindowBase(x, y, width, height, "Renderer", pParent)
{
	m_renderer.Init(GetWindowHandle(), width, height, &m_inputManager);
	m_defaultInputContext.SetCamera(&m_mainCamera);
	m_inputManager.PushContext(&m_defaultInputContext);
}

void RenderWindow::Close()
{
	m_renderer.Cleanup();
	WindowBase::Close();
}

bool RenderWindow::HandleResize(int newWidth, int newHeight)
{
	m_renderer.Resize(newWidth, newHeight);
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

void RenderWindow::Update()
{
	m_inputManager.GetActiveContext()->Update(m_inputManager);
}

void RenderWindow::QueueDraw(Scene& rScene)
{
	// Apply device changes (resizing, fullscreen, etc)
	m_renderer.ApplyDeviceChanges();

	RdrOffscreenTasks::IssuePendingActions(m_renderer);

	// Primary render action
	m_renderer.BeginPrimaryAction(m_mainCamera, rScene);
	{
		Debug::QueueDraw(m_renderer, m_mainCamera);
	}
	m_renderer.EndAction();
}

void RenderWindow::DrawFrame()
{
	m_renderer.DrawFrame();
}

void RenderWindow::PostFrameSync()
{
	m_renderer.PostFrameSync();
}

void RenderWindow::SetCameraPosition(const Vec3& position, const Vec3& pitchYawRoll)
{
	m_mainCamera.SetPosition(position);
	m_mainCamera.SetPitchYawRoll(pitchYawRoll);
}
