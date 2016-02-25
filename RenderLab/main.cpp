#include "Precompiled.h"
#include <windowsx.h>
#include "render/Renderer.h"
#include "render/Font.h"
#include "Scene.h"
#include "input/Input.h"
#include "input/CameraInputContext.h"
#include "Timer.h"
#include "debug/DebugConsole.h"

Renderer g_renderer;
Scene g_scene;

namespace
{
	const int kClientWidth = 1440;
	const int kClientHeight = 900;
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
	char fpsStr[32];
	float fpsTime = 0.f;
	int frameCount = 0;

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

		// Update fps
		++frameCount;
		fpsTime += dt;
		if (fpsTime > 1.f)
		{
			float ms = (fpsTime * 1000.f) / frameCount;
			float fps = frameCount / fpsTime;
			sprintf_s(fpsStr, "%.2f (%.2f ms)", fps, ms);
			fpsTime = 0.f;
			frameCount = 0;
		}

		Input::GetActiveContext()->Update(dt);

		g_scene.Update(dt);

		// Primary render action
		{
			g_renderer.BeginAction(&g_renderer.GetMainCamera(), nullptr);

			g_scene.QueueDraw(&g_renderer);
			DebugConsole::QueueDraw(&g_renderer);

			// Debug
			Font::QueueDraw(&g_renderer, Vec3(g_renderer.GetViewportWidth() - 150.f, 0.f, 0.f), 20.f, fpsStr, Color::kWhite);

			g_renderer.EndAction();
		}

		g_renderer.DrawFrame();
	}

	g_renderer.Cleanup();

	return (int)msg.wParam;
}

