#include "Precompiled.h"
#include <windowsx.h>
#include <thread>
#include "render/Renderer.h"
#include "render/Font.h"
#include "Scene.h"
#include "input/Input.h"
#include "input/CameraInputContext.h"
#include "Timer.h"
#include "FrameTimer.h"
#include "debug/DebugConsole.h"
#include "debug/Debug.h"

namespace
{
	const int kClientWidth = 1440;
	const int kClientHeight = 960;

	Scene g_scene;

	Renderer g_renderer;

	HANDLE g_hFrameSignal;
	HANDLE g_hRenderFrameDoneEvent;

	bool g_quitting;
}

LRESULT CALLBACK WinProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch (message)
	{
	case WM_KEYDOWN:
		Input::SetKeyDown(wParam, true);
		if (wParam == KEY_ESC)
			PostQuitMessage(0);
		return 0;
	case WM_KEYUP:
		Input::SetKeyDown(wParam, false);
		return 0;
	case WM_LBUTTONDOWN:
		Input::SetMouseDown(0, true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		SetCapture(hWnd);
		return 0;
	case WM_LBUTTONUP:
		Input::SetMouseDown(0, false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		ReleaseCapture();
		return 0;
	case WM_RBUTTONDOWN:
		Input::SetMouseDown(1, true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		SetCapture(hWnd);
		return 0;
	case WM_RBUTTONUP:
		Input::SetMouseDown(1, false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		ReleaseCapture();
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
		WaitForSingleObject(g_hFrameSignal, INFINITE);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE , LPSTR , int nCmdShow )
{
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

	ShowWindow(hWnd, nCmdShow);
	g_renderer.Init(hWnd, kClientWidth, kClientHeight);
	DebugConsole::Init(g_renderer.GetContext());

	g_scene.Load(g_renderer.GetContext(), "basic.scene");

	MSG msg;
	Timer::Handle hTimer = Timer::Create();

	CameraInputContext defaultInput;
	defaultInput.SetCamera(&g_scene.GetMainCamera());
	Input::PushContext(&defaultInput);

	FrameTimer frameTimer;

	g_hFrameSignal = CreateEvent(NULL, false, false, L"Frame Done");
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

		float dt = Timer::GetElapsedSecondsAndReset(hTimer);

		Input::GetActiveContext()->Update(dt);

		g_scene.Update(dt);

		// Primary render action
		{
			Camera& rCamera = g_scene.GetMainCamera();
			rCamera.UpdateFrustum();

			g_scene.QueueShadowMaps(g_renderer, rCamera);

			g_renderer.BeginPrimaryAction(rCamera, g_scene.GetLightList());

			g_scene.QueueDraw(g_renderer);
			DebugConsole::QueueDraw(g_renderer);

			// Debug
			Debug::QueueDebugDisplay(g_renderer, g_scene, frameTimer);

			g_renderer.EndAction();
		}

		// Wait for render thread.
		WaitForSingleObject(g_hRenderFrameDoneEvent, INFINITE);

		// Sync threads.
		g_renderer.PostFrameSync();

		// Restart render thread
		SetEvent(g_hFrameSignal);

		frameTimer.Update(dt);
	}

	renderThread.join();

	g_renderer.Cleanup();

	return (int)msg.wParam;
}

