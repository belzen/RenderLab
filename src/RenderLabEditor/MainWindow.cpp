#include "MainWindow.h"
#include <thread>
#include "UtilsLib/Timer.h"
#include "FrameTimer.h"

void MainWindow::Create(int width, int height, const char* title)
{
	WindowBase::Create(0, width, height, title);
	
	m_mainMenu.Init();
	m_fileMenu.Init();

	m_fileMenu.AddItem("Exit", [](void* pUserData) {
		PostQuitMessage(0);
	}, this);

	m_mainMenu.AddSubMenu("File", &m_fileMenu);
	SetMenuBar(&m_mainMenu);
}

bool MainWindow::HandleResize(int newWidth, int newHeight)
{
	m_renderWindow.Resize(newWidth, newHeight);
	return true;
}

bool MainWindow::HandleClose()
{
	PostQuitMessage(0);
	return true;
}

void MainWindow::RenderThreadMain(MainWindow* pWindow)
{
	while (pWindow->m_running)
	{
		pWindow->m_renderer.DrawFrame();

		SetEvent(pWindow->m_hRenderFrameDoneEvent);
		WaitForSingleObject(pWindow->m_hFrameDoneEvent, INFINITE);
	}
}

int MainWindow::Run()
{
	HWND hWnd = GetWindowHandle();

	m_running = true;
	m_scene.Load("basic");

	m_renderWindow.Create(hWnd, GetWidth(), GetHeight(), &m_renderer);

	Timer::Handle hTimer = Timer::Create();
	FrameTimer frameTimer;

	m_hFrameDoneEvent = CreateEvent(NULL, false, false, L"Frame Done");
	m_hRenderFrameDoneEvent = CreateEvent(NULL, false, false, L"Render Frame Done");
	std::thread renderThread(MainWindow::RenderThreadMain, this);

	MSG msg = { 0 };
	while (m_running)
	{
		m_renderWindow.EarlyUpdate();

		while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				m_running = false;
				break;
			}
		}

		float dt = Timer::GetElapsedSecondsAndReset(hTimer);
		m_renderWindow.Update(dt);

		m_scene.Update(dt);
		Debug::Update(dt);

		m_scene.PrepareDraw();
		m_renderWindow.Draw(m_scene, frameTimer);

		// Wait for render thread.
		WaitForSingleObject(m_hRenderFrameDoneEvent, INFINITE);

		// Sync threads.
		m_renderer.PostFrameSync();

		// Restart render thread
		SetEvent(m_hFrameDoneEvent);

		frameTimer.Update(dt);
	}

	renderThread.join();

	m_renderWindow.Close();

	m_renderer.Cleanup();
	FileWatcher::Cleanup();

	return (int)msg.wParam;
}