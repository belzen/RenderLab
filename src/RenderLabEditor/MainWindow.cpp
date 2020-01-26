#include "Precompiled.h"
#include "MainWindow.h"
#include "FileDialog.h"
#include "UtilsLib/Timer.h"
#include "Widgets/PropertyPanel.h"
#include "Widgets/TreeView.h"
#include "Widgets/AssetBrowser.h"
#include "AssetLib/AssetLibrary.h"
#include "ViewModels/IViewModel.h"
#include "Entity.h"
#include "RenderDoc\RenderDocUtil.h"
#include "UserConfig.h"
#include "Physics.h"
#include "Game.h"
#include "Time.h"
#include "GlobalState.h"
#include "components/Light.h"
#include "components/Renderable.h"
#include "components/VolumeComponent.h"
#include "components/ModelComponent.h"

namespace
{
	static const int kDefaultPanelWidth = 300;
	static const int kDefaultBrowserHeight = 200;

	void sceneTreeViewSelectionChanged(const TreeView* pTree, TreeViewItemHandle hSelectedItem, void* pUserData)
	{
		const TreeViewItem* pItem = pTree->GetItem(hSelectedItem);
		if (!pItem)
			return;

		SceneViewModel* pSceneViewModel = static_cast<SceneViewModel*>(pUserData);
		pSceneViewModel->SetSelected(static_cast<Entity*>(pItem->pData));
	}

	void sceneTreeViewItemDeleted(const TreeView* pTree, TreeViewItem* pDeletedItem, void* pUserData)
	{
		SceneViewModel* pSceneViewModel = static_cast<SceneViewModel*>(pUserData);
		pSceneViewModel->RemoveObject(static_cast<Entity*>(pDeletedItem->pData));
	}

	void setDebugVisualizationMode(Menu* pVisMenu, DebugVisMode visMode)
	{
		g_debugState.visMode = visMode;
		pVisMenu->SetRadioItemChecked((int)visMode);
	}
}

MainWindow* MainWindow::Create(int width, int height, const char* title)
{
	return new MainWindow(width, height, title);
}

MainWindow::MainWindow(int width, int height, const char* title)
	: WindowBase(0, 0, width, height, title, nullptr, nullptr)
	, m_pPropertyPanel(nullptr)
	, m_pSceneTreeView(nullptr)
	, m_running(false)
	, m_hFrameDoneEvent(0)
	, m_hRenderFrameDoneEvent(0)
{
	::InitCommonControls();

	INITCOMMONCONTROLSEX icex;
	icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_STANDARD_CLASSES;
	::InitCommonControlsEx(&icex);
	
	HFONT hDefaultFont = CreateFontA(14, 0, 0, 0, FW_DONTCARE, false, false, false, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
	Widget::SetDefaultFont(hDefaultFont);

	//////////////////////////////////////////////////////////////////////////
	// File menu
	m_fileMenu.Init();
	m_fileMenu.AddItem("Open Scene...", [](void* pUserData) {
		std::string filename = FileDialog::Show("Scene (*.scene)\0*.scene\0\0");
		if (filename.length() > 0)
		{
			const char* relativePath = filename.c_str() + strlen(Paths::GetSrcDataDir()) + 1;
			char sceneName[AssetLib::AssetDef::kMaxNameLen];
			AssetLib::Scene::GetAssetDef().ExtractAssetName(relativePath, sceneName, AssetLib::AssetDef::kMaxNameLen);
			((MainWindow*)pUserData)->LoadScene(sceneName);
		}
	}, this);
	m_fileMenu.AddItem("Exit", [](void* pUserData) {
		::PostQuitMessage(0);
	}, this);

	//////////////////////////////////////////////////////////////////////////
	// Add/create menu
	m_addMenu.Init();
	m_addMenu.AddItem("Add Object", [](void* pUserData) {
		MainWindow* pWindow = static_cast<MainWindow*>(pUserData);
		Entity* pEntity = Entity::Create("New Object", Vec3::kZero, Rotation::kIdentity, Vec3::kOne);
		pEntity->AttachRenderable(ModelComponent::Create(Scene::GetComponentAllocator(), "box", nullptr, 0));
		pWindow->m_sceneViewModel.AddEntity(pEntity);
	}, this);

	//////////////////////////////////////////////////////////////////////////
	// Game menu
	m_gameMenu.Init();
	m_gameMenu.AddItem("Play", [](void* pUserData) {
		Game::Play();
	}, this);
	m_gameMenu.AddItem("Pause", [](void* pUserData) {
		Game::Pause();
	}, this);
	m_gameMenu.AddItem("Stop", [](void* pUserData) {
		Game::Stop();
	}, this);

	//////////////////////////////////////////////////////////////////////////
	// Debug menu
	m_debugMenu.Init();
	m_debugMenu.AddItem("CPU Profiler", [](void* pUserData) {
		g_debugState.showCpuProfiler = !g_debugState.showCpuProfiler;
	}, this);
	m_debugMenu.AddItem("RenderDoc Capture", [](void* pUserData) {
		RenderDoc::Capture();
	}, this);
	m_debugMenu.AddItem("Invalidate Spec Probes", [](void* pUserData) {
		Scene::InvalidateEnvironmentLights();
	}, this);

	// Visualization sub-menu
	{
		m_debugVisMenu.Init();
		m_debugVisMenu.AddItem("Disabled", [](void* pUserData) {
			MainWindow* pWindow = static_cast<MainWindow*>(pUserData);
			setDebugVisualizationMode(&pWindow->m_debugVisMenu, DebugVisMode::kNone);
		}, this);
		m_debugVisMenu.AddItem("SSAO", [](void* pUserData) {
			MainWindow* pWindow = static_cast<MainWindow*>(pUserData);
			setDebugVisualizationMode(&pWindow->m_debugVisMenu, DebugVisMode::kSsao);
		}, this);

		setDebugVisualizationMode(&m_debugVisMenu, DebugVisMode::kNone);
	}
	m_debugMenu.AddSubMenu("Visualization", &m_debugVisMenu);

	//////////////////////////////////////////////////////////////////////////
	// Main menu
	m_mainMenu.Init();
	m_mainMenu.AddSubMenu("File", &m_fileMenu);
	m_mainMenu.AddSubMenu("Add", &m_addMenu);
	m_mainMenu.AddSubMenu("Game", &m_gameMenu);
	m_mainMenu.AddSubMenu("Debug", &m_debugMenu);

	SetMenuBar(&m_mainMenu);
}

void MainWindow::LoadScene(const char* sceneName)
{
	Scene::Load(sceneName);
	m_pRenderWindow->SetCameraPosition(Scene::GetCameraSpawnPosition(), Scene::GetCameraSpawnRotation());
	m_sceneViewModel.PopulateTreeView(m_pSceneTreeView);
}

bool MainWindow::OnResize(int newWidth, int newHeight)
{
	int panelWidth = kDefaultPanelWidth;
	int browserHeight = kDefaultBrowserHeight;

	if (m_pRenderWindow)
	{
		m_pRenderWindow->SetSize(newWidth - panelWidth, newHeight - browserHeight);

		m_pAssetBrowser->SetSize(newWidth - panelWidth, browserHeight);
		m_pAssetBrowser->SetPosition(0, newHeight - browserHeight);

		m_pPropertyPanel->SetSize(panelWidth, newHeight / 2);
		m_pPropertyPanel->SetPosition(newWidth - panelWidth, newHeight / 2);

		m_pSceneTreeView->SetSize(panelWidth, newHeight / 2);
		m_pSceneTreeView->SetPosition(newWidth - panelWidth, 0);
	}
	return true;
}

bool MainWindow::OnClose()
{
	::PostQuitMessage(0);
	return true;
}

void MainWindow::OnEntityAddedToScene(Entity* pEntity)
{
	m_pSceneTreeView->AddItem(pEntity->GetName(), pEntity);
}

void MainWindow::OnEntityRemovedFromScene(Entity* pEntity)
{
	m_pSceneTreeView->RemoveItemByData(pEntity);
}

void MainWindow::OnSceneSelectionChanged(Entity* pEntity)
{
	m_pPropertyPanel->SetViewModel(IViewModel::Create(pEntity), true);
	if (pEntity->GetRenderable())
	{
		m_pPropertyPanel->AddViewModel(IViewModel::Create(pEntity->GetRenderable()));
	}
	if (pEntity->GetLight())
	{
		m_pPropertyPanel->AddViewModel(IViewModel::Create(pEntity->GetLight()));
	}
	if (pEntity->GetVolume())
	{
		m_pPropertyPanel->AddViewModel(IViewModel::Create(pEntity->GetVolume()));
	}
	m_pSceneTreeView->SelectItem(pEntity);
}

int MainWindow::Run()
{
	m_running = true;

	// Initialize renderer first.
	HWND hWnd = GetWindowHandle();

	int renderWindowWidth = GetWidth() - kDefaultPanelWidth;
	int renderWindowHeight = GetHeight() - kDefaultBrowserHeight;
	m_pRenderWindow = RenderWindow::Create(0, 0, renderWindowWidth, renderWindowHeight, &m_sceneViewModel, this);

	Debug::Init();

	// Finish editor setup.
	m_pPropertyPanel = PropertyPanel::Create(*this, renderWindowWidth, GetHeight() / 2, kDefaultPanelWidth, GetHeight() / 2);
	m_pAssetBrowser = AssetBrowser::Create(*this, 0, renderWindowHeight, renderWindowWidth, kDefaultBrowserHeight);

	m_pSceneTreeView = TreeView::Create(*this, renderWindowWidth, 0, kDefaultPanelWidth, GetHeight() / 2);
	m_pSceneTreeView->SetSelectionChangedCallback(sceneTreeViewSelectionChanged, &m_sceneViewModel);
	m_pSceneTreeView->SetItemDeletedCallback(sceneTreeViewItemDeleted, &m_sceneViewModel);

	m_sceneViewModel.SetListener(this);

	// Load in default scene
	LoadScene(g_userConfig.defaultScene.c_str());

	Timer::Handle hTimer = Timer::Create();

	m_hFrameDoneEvent = ::CreateEvent(NULL, false, false, L"Frame Done");
	m_hRenderFrameDoneEvent = ::CreateEvent(NULL, false, false, L"Render Frame Done");
	std::thread renderThread(MainWindow::RenderThreadMain, this);

	MSG msg = { 0 };
	while (m_running)
	{
		m_pRenderWindow->EarlyUpdate();

		while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				m_running = false;
				break;
			}
		}

		Time::Update((float)Timer::GetElapsedSecondsAndReset(hTimer));

		m_pRenderWindow->Update();

		Debug::Update();
		Scene::Update();

		m_pRenderWindow->QueueDraw();

		// Wait for render thread.
		WaitForSingleObject(m_hRenderFrameDoneEvent, INFINITE);

		// Sync threads.
		m_pRenderWindow->PostFrameSync();

		// Restart render thread
		::SetEvent(m_hFrameDoneEvent);
	}

	// End the render thread.
	::SetEvent(m_hFrameDoneEvent);
	renderThread.join();

	m_pRenderWindow->Close();

	FileWatcher::Cleanup();

	return (int)msg.wParam;
}

void MainWindow::RenderThreadMain(MainWindow* pWindow)
{
	Renderer::SetRenderThread(GetCurrentThreadId());
	RdrCpuThreadProfiler& threadProfiler = RdrCpuThreadProfiler::GetThreadProfiler(GetCurrentThreadId());

	while (pWindow->m_running)
	{
		threadProfiler.BeginFrame();
		pWindow->m_pRenderWindow->DrawFrame();
		threadProfiler.EndFrame();

		::SetEvent(pWindow->m_hRenderFrameDoneEvent);
		::WaitForSingleObject(pWindow->m_hFrameDoneEvent, INFINITE);
	}
}
