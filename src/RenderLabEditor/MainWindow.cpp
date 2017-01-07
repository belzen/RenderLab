#include "Precompiled.h"
#include "MainWindow.h"
#include "UtilsLib/Timer.h"
#include "Widgets/PropertyPanel.h"
#include "Widgets/TreeView.h"
#include "Widgets/AssetBrowser.h"
#include "AssetLib/AssetLibrary.h"
#include "ViewModels/IViewModel.h"
#include "AssetLib/SkyAsset.h"
#include "WorldObject.h"
#include "RenderDoc\RenderDocUtil.h"
#include "UserConfig.h"
#include "Physics.h"
#include "Time.h"

namespace
{
	void sceneTreeViewSelectionChanged(const TreeView* pTree, TreeViewItemHandle hSelectedItem, void* pUserData)
	{
		const TreeViewItem* pItem = pTree->GetItem(hSelectedItem);
		if (!pItem)
			return;

		SceneViewModel* pSceneViewModel = static_cast<SceneViewModel*>(pUserData);
		pSceneViewModel->SetSelected(static_cast<WorldObject*>(pItem->pData));
	}

	void sceneTreeViewItemDeleted(const TreeView* pTree, TreeViewItem* pDeletedItem, void* pUserData)
	{
		SceneViewModel* pSceneViewModel = static_cast<SceneViewModel*>(pUserData);
		pSceneViewModel->RemoveObject(static_cast<WorldObject*>(pDeletedItem->pData));
	}

	void sceneObjectAdded(WorldObject* pObject, void* pUserData)
	{
		TreeView* pTreeView = static_cast<TreeView*>(pUserData);
		pTreeView->AddItem(pObject->GetName(), pObject);
	}

	void sceneObjectRemoved(WorldObject* pObject, void* pUserData)
	{
		TreeView* pTreeView = static_cast<TreeView*>(pUserData);
		pTreeView->RemoveItemByData(pObject);
	}

	void sceneObjectSelected(WorldObject* pObject, void* pUserData)
	{
		PropertyPanel* pPropertyPanel = static_cast<PropertyPanel*>(pUserData);
		pPropertyPanel->SetViewModel(IViewModel::Create(pObject), true);
	}
}

MainWindow* MainWindow::Create(int width, int height, const char* title)
{
	return new MainWindow(width, height, title);
}

MainWindow::MainWindow(int width, int height, const char* title)
	: WindowBase(0, 0, width, height, title, nullptr)
	, m_pPropertyPanel(nullptr)
	, m_pSceneTreeView(nullptr)
	, m_running(false)
	, m_hFrameDoneEvent(0)
	, m_hRenderFrameDoneEvent(0)
{
	::InitCommonControls();

	INITCOMMONCONTROLSEX icex;
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES;
	::InitCommonControlsEx(&icex);
	
	m_sceneViewModel.Init(&m_scene);

	m_fileMenu.Init();
	m_fileMenu.AddItem("Exit", [](void* pUserData) {
		::PostQuitMessage(0);
	}, this);

	m_addMenu.Init();
	m_addMenu.AddItem("Add Object", [](void* pUserData) {
		MainWindow* pWindow = static_cast<MainWindow*>(pUserData);
		WorldObject* pObject = WorldObject::Create("New Object", Vec3::kZero, Quaternion::kIdentity, Vec3::kOne);
		pObject->AttachModel(ModelInstance::Create("box", nullptr, 0));
		pWindow->m_sceneViewModel.AddObject(pObject);
	}, this);

	m_debugMenu.Init();
	m_debugMenu.AddItem("RenderDoc Capture", [](void* pUserData) {
		RenderDoc::Capture();
	}, this);
	m_debugMenu.AddItem("Invalidate Spec Probes", [](void* pUserData) {
		static_cast<MainWindow*>(pUserData)->m_scene.GetLightList().InvalidateEnvironmentLights();
	}, this);

	m_mainMenu.Init();
	m_mainMenu.AddSubMenu("File", &m_fileMenu);
	m_mainMenu.AddSubMenu("Add", &m_addMenu);
	m_mainMenu.AddSubMenu("Debug", &m_debugMenu);

	SetMenuBar(&m_mainMenu);
}

bool MainWindow::OnResize(int newWidth, int newHeight)
{
	int panelWidth = m_pPropertyPanel ? m_pPropertyPanel->GetWidth() : 0;
	if (m_pRenderWindow)
	{
		m_pRenderWindow->SetSize(newWidth - panelWidth, newHeight);
	}
	return true;
}

bool MainWindow::OnClose()
{
	::PostQuitMessage(0);
	return true;
}

int MainWindow::Run()
{
	m_running = true;

	// Initialize renderer first.
	HWND hWnd = GetWindowHandle();

	const int kDefaultPanelWidth = 300;
	const int kDefaultBrowserHeight = 200;

	int renderWindowWidth = GetWidth() - kDefaultPanelWidth;
	int renderWindowHeight = GetHeight() - kDefaultBrowserHeight;
	m_pRenderWindow = RenderWindow::Create(0, 0, renderWindowWidth, renderWindowHeight, &m_sceneViewModel, this);

	Debug::Init();
	Physics::Init();

	// Load in default scene
	m_scene.Load(g_userConfig.defaultScene.c_str());
	m_pRenderWindow->SetCameraPosition(m_scene.GetCameraSpawnPosition(), m_scene.GetCameraSpawnPitchYawRoll());
	m_pRenderWindow->SetObjectSelectedCallback(OnRenderWindowSelectedObject, this);

	// Finish editor setup.
	m_pPropertyPanel = PropertyPanel::Create(*this, renderWindowWidth, GetHeight() / 2, kDefaultPanelWidth, GetHeight() / 2);
	m_pAssetBrowser = AssetBrowser::Create(*this, 0, renderWindowHeight, renderWindowWidth, kDefaultBrowserHeight);

	m_pSceneTreeView = TreeView::Create(*this, renderWindowWidth, 0, kDefaultPanelWidth, GetHeight() / 2);
	m_pSceneTreeView->SetSelectionChangedCallback(sceneTreeViewSelectionChanged, &m_sceneViewModel);
	m_pSceneTreeView->SetItemDeletedCallback(sceneTreeViewItemDeleted, &m_sceneViewModel);

	m_sceneViewModel.PopulateTreeView(m_pSceneTreeView);
	m_sceneViewModel.SetObjectAddedCallback(sceneObjectAdded, m_pSceneTreeView);
	m_sceneViewModel.SetObjectRemovedCallback(sceneObjectRemoved, m_pSceneTreeView);
	m_sceneViewModel.SetSelectionChangedCallback(sceneObjectSelected, m_pPropertyPanel);

	Timer::Handle hTimer = Timer::Create();

	m_hFrameDoneEvent = ::CreateEvent(NULL, false, false, L"Frame Done");
	m_hRenderFrameDoneEvent = ::CreateEvent(NULL, false, false, L"Render Frame Done");
	std::thread renderThread(MainWindow::RenderThreadMain, this);

	MSG msg = { 0 };
	while (m_running)
	{
		m_pRenderWindow->EarlyUpdate();

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

		Time::Update(Timer::GetElapsedSecondsAndReset(hTimer));

		m_pRenderWindow->Update();

		m_scene.Update();
		Physics::Update();
		Debug::Update();

		m_pRenderWindow->QueueDraw();

		// Wait for render thread.
		WaitForSingleObject(m_hRenderFrameDoneEvent, INFINITE);

		// Sync threads.
		m_pRenderWindow->PostFrameSync();

		// Restart render thread
		::SetEvent(m_hFrameDoneEvent);
	}

	renderThread.join();

	m_pRenderWindow->Close();

	FileWatcher::Cleanup();

	return (int)msg.wParam;
}

void MainWindow::OnRenderWindowSelectedObject(WorldObject* pObject, void* pUserData)
{
	MainWindow* pWindow = static_cast<MainWindow*>(pUserData);
	pWindow->m_pSceneTreeView->SelectItem(pObject);
}

void MainWindow::RenderThreadMain(MainWindow* pWindow)
{
	while (pWindow->m_running)
	{
		pWindow->m_pRenderWindow->DrawFrame();

		::SetEvent(pWindow->m_hRenderFrameDoneEvent);
		::WaitForSingleObject(pWindow->m_hFrameDoneEvent, INFINITE);
	}
}
