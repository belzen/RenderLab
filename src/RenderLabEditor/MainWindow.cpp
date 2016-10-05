#include "Precompiled.h"
#include "MainWindow.h"
#include "UtilsLib/Timer.h"
#include "FrameTimer.h"
#include "Widgets/PropertyPanel.h"
#include "Widgets/ListView.h"
#include "AssetLib/AssetLibrary.h"
#include "ViewModels/IViewModel.h"
#include "AssetLib/SkyAsset.h"
#include "WorldObject.h"
#include "RenderDoc\RenderDocUtil.h"

MainWindow::MainWindow()
	: m_pPropertyPanel(nullptr)
	, m_pSceneListView(nullptr)
	, m_running(false)
	, m_hFrameDoneEvent(0)
	, m_hRenderFrameDoneEvent(0)
{

}

void MainWindow::Create(int width, int height, const char* title)
{
	::InitCommonControls();

	INITCOMMONCONTROLSEX icex;
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	::InitCommonControlsEx(&icex);

	WindowBase::Create(0, width, height, title);
	
	m_sceneViewModel.Init(&m_scene, SceneObjectAdded, this);

	m_fileMenu.Init();
	m_fileMenu.AddItem("Exit", [](void* pUserData) {
		::PostQuitMessage(0);
	}, this);

	m_addMenu.Init();
	m_addMenu.AddItem("Add Object", [](void* pUserData) {
		MainWindow* pWindow = static_cast<MainWindow*>(pUserData);
		ModelInstance* pModel = ModelInstance::Create("box");
		WorldObject* pObject = WorldObject::Create("New Object", pModel, Vec3::kZero, Quaternion::kIdentity, Vec3::kOne);
		pWindow->m_sceneViewModel.AddObject(pObject);
	}, this);

	m_debugMenu.Init();
	m_debugMenu.AddItem("RenderDoc Capture", [](void* pUserData) {
		RenderDoc::Capture();
	}, this);

	m_mainMenu.Init();
	m_mainMenu.AddSubMenu("File", &m_fileMenu);
	m_mainMenu.AddSubMenu("Add", &m_addMenu);
	m_mainMenu.AddSubMenu("Debug", &m_debugMenu);

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

		WorldObjectList& sceneObjects = m_scene.GetWorldObjects();
		for (WorldObject* pObject : sceneObjects)
		{
			m_pSceneListView->AddItem(pObject->GetName(), pObject);
		}

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

void MainWindow::RenderThreadMain(MainWindow* pWindow)
{
	while (pWindow->m_running)
	{
		pWindow->m_renderer.DrawFrame();

		::SetEvent(pWindow->m_hRenderFrameDoneEvent);
		::WaitForSingleObject(pWindow->m_hFrameDoneEvent, INFINITE);
	}
}

void MainWindow::SceneObjectAdded(WorldObject* pObject, void* pUserData)
{
	MainWindow* pWindow = static_cast<MainWindow*>(pUserData);
	pWindow->m_pSceneListView->AddItem(pObject->GetName(), pObject);
}