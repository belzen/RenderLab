#pragma once

#include "WindowBase.h"
#include "Scene.h"
#include "ViewModels\SceneViewModel.h"
#include "RenderWindow.h"
#include "Widgets/Menu.h"

class PropertyPanel;
class TreeView;
class AssetBrowser;

class MainWindow : public WindowBase
{
public:
	static MainWindow* Create(int width, int height, const char* title);

	int Run();

private:
	MainWindow(int width, int height, const char* title);

	// Input event handlers
	bool OnResize(int newWidth, int newHeight);
	bool OnClose();

private:
	static void RenderThreadMain(MainWindow* pWindow);
	static void OnRenderWindowSelectedObject(WorldObject* pObject, void* pUserData);

private:
	Scene m_scene;
	SceneViewModel m_sceneViewModel;

	RenderWindow* m_pRenderWindow;
	PropertyPanel* m_pPropertyPanel;
	TreeView* m_pSceneTreeView;
	AssetBrowser* m_pAssetBrowser;
	bool m_running;

	HANDLE m_hFrameDoneEvent;
	HANDLE m_hRenderFrameDoneEvent;

	Menu m_mainMenu;
	Menu m_fileMenu;
	Menu m_addMenu;
	Menu m_debugMenu;
};
