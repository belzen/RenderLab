#include "Precompiled.h"
#include <windowsx.h>
#include <thread>
#include "render/Renderer.h"
#include "render/Font.h"
#include "Scene.h"
#include "input/Input.h"
#include "input/CameraInputContext.h"
#include "UtilsLib/Timer.h"
#include "FrameTimer.h"
#include "debug/Debug.h"


namespace
{
	const int kClientWidth = 1440;
	const int kClientHeight = 960;

	Scene g_scene;

	Renderer g_renderer;

	HANDLE g_hFrameDoneEvent = 0;
	HANDLE g_hRenderFrameDoneEvent = 0;

	bool g_quitting = false;
	int g_mouseCaptureCount = 0;

	void captureMouse(HWND hWnd)
	{
		if (g_mouseCaptureCount == 0)
			SetCapture(hWnd);

		++g_mouseCaptureCount;
	}

	void releaseMouse()
	{
		--g_mouseCaptureCount;
		if (g_mouseCaptureCount == 0)
			ReleaseCapture();
	}
}

LRESULT CALLBACK WinProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
	case WM_KEYDOWN:
		Input::SetKeyDown((int)wParam, true);
		if (wParam == KEY_ESC)
			PostQuitMessage(0);
		return 0;
	case WM_KEYUP:
		Input::SetKeyDown((int)wParam, false);
		return 0;
	case WM_LBUTTONDOWN:
		Input::SetMouseDown(0, true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		captureMouse(hWnd);
		return 0;
	case WM_LBUTTONUP:
		Input::SetMouseDown(0, false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		releaseMouse();
		return 0;
	case WM_RBUTTONDOWN:
		Input::SetMouseDown(1, true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		captureMouse(hWnd);
		return 0;
	case WM_RBUTTONUP:
		Input::SetMouseDown(1, false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		releaseMouse();
		return 0;
	case WM_MOUSEMOVE:
		Input::SetMousePos(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_SIZE:
		g_renderer.Resize( LOWORD(lParam), HIWORD(lParam) );
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

void renderThreadMain()
{
	while ( !g_quitting )
	{
		g_renderer.DrawFrame();

		SetEvent(g_hRenderFrameDoneEvent);
		WaitForSingleObject(g_hFrameDoneEvent, INFINITE);
	}
}

int main(int argc, char** argv)
{
	HINSTANCE hInstance = GetModuleHandle(0);
	HWND hWnd;
	WNDCLASSEX wc = { 0 };
	RECT wr = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, false);

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WinProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = L"RenderLab";
	
	RegisterClassEx(&wc);

	hWnd = CreateWindowEx(NULL,
		wc.lpszClassName, L"Render Lab",
		WS_OVERLAPPEDWINDOW,
		10, 10,
		wr.right - wr.left, wr.bottom - wr.top,
		NULL, NULL, hInstance, NULL);

	ShowWindow(hWnd, 1);
	g_renderer.Init(hWnd, kClientWidth, kClientHeight);
	Debug::Init();

	FileWatcher::Init(Paths::GetBinDataDir());

	g_scene.Load("sponza");

	MSG msg;
	Timer::Handle hTimer = Timer::Create();

	CameraInputContext defaultInput;
	defaultInput.SetCamera(&g_scene.GetMainCamera());
	Input::PushContext(&defaultInput);

	FrameTimer frameTimer;

	g_hFrameDoneEvent = CreateEvent(NULL, false, false, L"Frame Done");
	g_hRenderFrameDoneEvent = CreateEvent(NULL, false, false, L"Render Frame Done");
	std::thread renderThread(renderThreadMain);

	while (!g_quitting)
	{
		Input::Reset();

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				g_quitting = true;
				break;
			}
		}

		// Apply device changes (resizing, fullscreen, etc)
		g_renderer.ApplyDeviceChanges();

		float dt = Timer::GetElapsedSecondsAndReset(hTimer);

		Input::GetActiveContext()->Update(dt);

		g_scene.Update(dt);
		Debug::Update(dt);

		// Primary render action
		{
			g_scene.PrepareDraw();

			g_renderer.BeginPrimaryAction(g_scene.GetMainCamera(), g_scene);
			{
				Debug::QueueDraw(g_renderer, g_scene, frameTimer);
			}
			g_renderer.EndAction();
		}

		// Wait for render thread.
		WaitForSingleObject(g_hRenderFrameDoneEvent, INFINITE);

		// Sync threads.
		g_renderer.PostFrameSync();

		// Restart render thread
		SetEvent(g_hFrameDoneEvent);

		frameTimer.Update(dt);
	}

	renderThread.join();

	g_renderer.Cleanup();
	FileWatcher::Cleanup();

	return (int)msg.wParam;
}

