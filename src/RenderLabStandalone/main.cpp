#include "UtilsLib/Timer.h"
#include "AssetLib/AssetLibrary.h"
#include "Entity.h"
#include "RenderDoc\RenderDocUtil.h"
#include "render\Renderer.h"
#include "render\RdrOffscreenTasks.h"
#include "UserConfig.h"
#include "Physics.h"
#include "Time.h"
#include "Scene.h"
#include "GlobalState.h"
#include "input/Input.h"
#include "input/CameraInputContext.h"
#include <windowsx.h>
#include <thread>

namespace
{
	const int kClientWidth = 1440;
	const int kClientHeight = 960;

	InputManager g_inputManager;
	Renderer g_renderer;
	bool g_running = true;
	HANDLE g_hRenderFrameDoneEvent;
	HANDLE g_hFrameDoneEvent;
	int g_mouseCaptureCount = 0;

	void renderThreadMain()
	{
		while (g_running)
		{
			g_renderer.DrawFrame();

			::SetEvent(g_hRenderFrameDoneEvent);
			::WaitForSingleObject(g_hFrameDoneEvent, INFINITE);
		}
	}

	void captureMouse(HWND hWnd, int* pMouseCaptureCount)
	{
		if (*pMouseCaptureCount == 0)
			::SetCapture(hWnd);

		++(*pMouseCaptureCount);
	}

	void releaseMouse(int* pMouseCaptureCount)
	{
		--(*pMouseCaptureCount);
		if ((*pMouseCaptureCount) == 0)
			::ReleaseCapture();
	}

	LRESULT CALLBACK renderWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case WM_MOUSEMOVE:
			g_inputManager.SetMousePos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_LBUTTONDOWN:
			captureMouse(hWnd, &g_mouseCaptureCount);
			::SetFocus(hWnd);
			g_inputManager.SetMouseDown(0, true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_LBUTTONUP:
			g_inputManager.SetMouseDown(0, false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			releaseMouse(&g_mouseCaptureCount);
			break;
		case WM_RBUTTONDOWN:
			captureMouse(hWnd, &g_mouseCaptureCount);
			::SetFocus(hWnd);
			g_inputManager.SetMouseDown(1, true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_RBUTTONUP:
			g_inputManager.SetMouseDown(1, false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			releaseMouse(&g_mouseCaptureCount);
			break;
		case WM_KEYDOWN:
			g_inputManager.SetKeyDown((int)wParam, true);
			break;
		case WM_KEYUP:
			g_inputManager.SetKeyDown((int)wParam, false);
			break;
		case WM_SIZE:
			g_renderer.Resize(LOWORD(lParam), HIWORD(lParam));
			break;
		case WM_CLOSE:
		case WM_QUIT:
			g_running = false;
			break;
		}

		return ::DefWindowProc(hWnd, msg, wParam, lParam);
	}

	HWND createRenderWindow(int width, int height)
	{
		WNDCLASSEXA wc = { 0 };
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = renderWndProc;
		wc.hInstance = ::GetModuleHandle(NULL);
		wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
		wc.lpszClassName = "RenderWindow";

		::RegisterClassExA(&wc);

		DWORD windowType = WS_OVERLAPPEDWINDOW;
		RECT wr = { 0, 0, width, height };
		::AdjustWindowRect(&wr, windowType, false);

		return ::CreateWindowExA(
			0,
			"RenderWindow",
			"RenderLab",
			windowType | WS_VISIBLE,
			0, 0, (wr.right - wr.left), (wr.bottom - wr.top),
			0,
			NULL,
			::GetModuleHandle(NULL),
			NULL);
	}
}


int main(int argc, char** argv)
{
	GlobalState::Init();
	UserConfig::Load();
	if (g_userConfig.attachRenderDoc)
	{
		RenderDoc::Init();
	}
	FileWatcher::Init(Paths::GetDataDir());

	HWND hWnd = createRenderWindow(kClientWidth, kClientHeight);
	g_renderer.Init(hWnd, kClientWidth, kClientHeight, &g_inputManager);

	Debug::Init();
	Physics::Init();

	CameraInputContext inputContext;
	Camera mainCamera;

	inputContext.SetCamera(&mainCamera);
	g_inputManager.PushContext(&inputContext);

	// Load in default scene
	Scene::Load(g_userConfig.defaultScene.c_str());
	mainCamera.SetPosition(Scene::GetCameraSpawnPosition());
	mainCamera.SetRotation(Scene::GetCameraSpawnRotation());

	Timer::Handle hTimer = Timer::Create();

	g_hFrameDoneEvent = ::CreateEvent(NULL, false, false, L"Frame Done");
	g_hRenderFrameDoneEvent = ::CreateEvent(NULL, false, false, L"Render Frame Done");
	std::thread renderThread(renderThreadMain);

	MSG msg = { 0 };
	while (g_running)
	{
		g_inputManager.Reset();

		while (::PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				g_running = false;
				break;
			}
		}

		Time::Update(Timer::GetElapsedSecondsAndReset(hTimer));
		g_inputManager.GetActiveContext()->Update(g_inputManager);

		Debug::Update();
		Scene::Update();

		// Queue drawing
		{
			// Apply device changes (resizing, fullscreen, etc)
			g_renderer.ApplyDeviceChanges();

			RdrOffscreenTasks::IssuePendingActions(g_renderer);

			// Primary render action
			g_renderer.BeginPrimaryAction(mainCamera);
			{
				Debug::QueueDraw(g_renderer, mainCamera);
			}
			g_renderer.EndAction();
		}

		// Wait for render thread.
		WaitForSingleObject(g_hRenderFrameDoneEvent, INFINITE);

		// Sync threads.
		g_renderer.PostFrameSync();

		// Restart render thread
		::SetEvent(g_hFrameDoneEvent);
	}

	renderThread.join();

	g_renderer.Cleanup();
	FileWatcher::Cleanup();

	return (int)msg.wParam;
}
