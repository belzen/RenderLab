#include "Precompiled.h"
#include <windowsx.h>
#include "render/Renderer.h"
#include "render/Font.h"
#include "Scene.h"
#include "input/Input.h"
#include "input/CameraInputContext.h"
#include "Timer.h"
#include "FrameTimer.h"
#include "debug/DebugConsole.h"
#include "debug/Debug.h"

Renderer g_renderer;
Scene g_scene;

namespace
{
	const int kClientWidth = 1440;
	const int kClientHeight = 960;
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
	defaultInput.SetCamera(&g_renderer.GetMainCamera());
	Input::PushContext(&defaultInput);

	bool isQuitting = false;
	FrameTimer frameTimer;

	while (!isQuitting)
	{
		Input::Reset();

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				isQuitting = true;
				break;
			}
		}

		float dt = Timer::GetElapsedSecondsAndReset(hTimer);

		Input::GetActiveContext()->Update(dt);

		g_scene.Update(dt);

		// Primary render action
		{
			Camera* pCamera = &g_renderer.GetMainCamera();
			pCamera->UpdateFrustum();

			g_scene.QueueShadowMaps(g_renderer, pCamera);

			g_renderer.BeginPrimaryAction(pCamera, g_scene.GetLightList());

			g_scene.QueueDraw(g_renderer);
			DebugConsole::QueueDraw(g_renderer);

			// Debug
			Debug::QueueDebugDisplay(g_renderer, frameTimer);

			g_renderer.EndAction();
		}

		g_renderer.DrawFrame();


		frameTimer.Update(dt);
	}

	g_renderer.Cleanup();

	return (int)msg.wParam;
}

