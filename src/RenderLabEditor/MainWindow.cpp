#include "Precompiled.h"
#include "MainWindow.h"
#include "UtilsLib/Timer.h"
#include "FrameTimer.h"
#include "Widgets/PropertyPanel.h"
#include "Widgets/ListView.h"
#include "AssetLib/AssetLibrary.h"
#include "ViewModels/IViewModel.h"
#include "AssetLib/SkyAsset.h"

void MainWindow::Create(int width, int height, const char* title)
{
	::InitCommonControls();

	INITCOMMONCONTROLSEX icex;
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	::InitCommonControlsEx(&icex);

	WindowBase::Create(0, width, height, title);
	
	m_mainMenu.Init();
	m_fileMenu.Init();

	m_fileMenu.AddItem("Exit", [](void* pUserData) {
		::PostQuitMessage(0);
	}, this);

	m_mainMenu.AddSubMenu("File", &m_fileMenu);
	SetMenuBar(&m_mainMenu);
}

bool MainWindow::HandleResize(int newWidth, int newHeight)
{
	int panelWidth = m_pPropertyPanel ? m_pPropertyPanel->GetWidth() : 0;
	m_renderWindow.Resize(newWidth - panelWidth, newHeight);
	return true;
}

bool MainWindow::HandleClose()
{
	::PostQuitMessage(0);
	return true;
}

void MainWindow::RenderThreadMain(MainWindow* pWindow)
{
	while (pWindow->m_running)
	{
		pWindow->m_renderer.DrawFrame();

		::SetEvent(pWindow->m_hRenderFrameDoneEvent);
		::WaitForSingleObject(pWindow->m_hFrameDoneEvent, INFINITE);
	}
}

void SceneListViewSelectionChanged(const ListView* pList, uint selectedIndex, void* pUserData)
{
	const ListViewItem* pItem = pList->GetItem(selectedIndex);
	if (!pItem)
		return;

	PropertyPanel* pPropertyPanel = static_cast<PropertyPanel*>(pUserData);
	pPropertyPanel->SetViewModel(IViewModel::Create(pItem->typeId, pItem->pData), true);
}

int MainWindow::Run()
{
	HWND hWnd = GetWindowHandle();

	m_running = true;
	m_scene.Load("basic");

	const int kDefaultPanelWidth = 300;
	m_renderWindow.Create(hWnd, GetWidth() - kDefaultPanelWidth, GetHeight(), &m_renderer);

	m_pPropertyPanel = PropertyPanel::Create(*this, GetWidth() - kDefaultPanelWidth, GetHeight() / 2, kDefaultPanelWidth, GetHeight() / 2);
	m_pSceneListView = ListView::Create(*this, GetWidth() - kDefaultPanelWidth, 0, kDefaultPanelWidth, GetHeight() / 2, SceneListViewSelectionChanged, m_pPropertyPanel);

	// Fill out the scene list view
	{
		AssetLib::Sky* pSky = AssetLibrary<AssetLib::Sky>::LoadAsset("cloudy");
		m_pSceneListView->AddItem("Sky", pSky);

		AssetLib::PostProcessEffects* pEffects = AssetLibrary<AssetLib::PostProcessEffects>::LoadAsset(m_scene.GetPostProcEffects()->GetEffectsAsset()->assetName);
		m_pSceneListView->AddItem("Post-Processing Effects", pEffects);

		m_pSceneListView->SelectItem(0);
	}

	Timer::Handle hTimer = Timer::Create();
	FrameTimer frameTimer;

	m_hFrameDoneEvent = ::CreateEvent(NULL, false, false, L"Frame Done");
	m_hRenderFrameDoneEvent = ::CreateEvent(NULL, false, false, L"Render Frame Done");
	std::thread renderThread(MainWindow::RenderThreadMain, this);

	MSG msg = { 0 };
	while (m_running)
	{
		m_renderWindow.EarlyUpdate();

		while (::PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
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
		::SetEvent(m_hFrameDoneEvent);

		frameTimer.Update(dt);
	}

	renderThread.join();

	m_renderWindow.Close();

	m_renderer.Cleanup();
	FileWatcher::Cleanup();

	return (int)msg.wParam;
}