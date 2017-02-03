#pragma once

#include "WindowBase.h"
#include "Scene.h"
#include "ViewModels\SceneViewModel.h"
#include "RenderWindow.h"
#include "Widgets/Menu.h"

class PropertyPanel;
class TreeView;
class AssetBrowser;

class MainWindow 
	: public WindowBase
	, public ISceneListener
{
public:
	static MainWindow* Create(int width, int height, const char* title);

	int Run();

private:
	MainWindow(int width, int height, const char* title);

	void LoadScene(const char* sceneName);

	// Input event handlers
	bool OnResize(int newWidth, int newHeight);
	bool OnClose();

private:
	static void RenderThreadMain(MainWindow* pWindow);

	// ISceneListener
	void OnEntityAddedToScene(Entity* pEntity);
	void OnEntityRemovedFromScene(Entity* pEntity);
	void OnSceneSelectionChanged(Entity* pEntity);

private:
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
	Menu m_gameMenu;
	Menu m_debugMenu;
	Menu m_debugVisMenu;
};
