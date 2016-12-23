#include "Precompiled.h"
#include "render/Renderer.h"
#include "render/Font.h"
#include "Scene.h"
#include "input/Input.h"
#include "input/CameraInputContext.h"
#include "debug/Debug.h"
#include "RenderDoc/RenderDocUtil.h"
#include "MainWindow.h"
#include "GlobalState.h"
#include "UserConfig.h"

namespace
{
	const int kClientWidth = 1440;
	const int kClientHeight = 960;

	bool g_quitting = false;
}

int main(int argc, char** argv)
{
	GlobalState::Init();
	UserConfig::Load();
	if (g_userConfig.attachRenderDoc)
	{
		RenderDoc::Init();
	}
	FileWatcher::Init(Paths::GetDataDir());

	MainWindow* pMainWindow = MainWindow::Create(kClientWidth, kClientHeight, "Render Lab");

	return pMainWindow->Run();
}
