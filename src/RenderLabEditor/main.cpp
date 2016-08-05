#include "Precompiled.h"
#include "render/Renderer.h"
#include "render/Font.h"
#include "Scene.h"
#include "input/Input.h"
#include "input/CameraInputContext.h"
#include "debug/Debug.h"
#include "RenderDoc/RenderDocUtil.h"
#include "MainWindow.h"

namespace
{
	const int kClientWidth = 1440;
	const int kClientHeight = 960;

	bool g_quitting = false;
}

int main(int argc, char** argv)
{
	RenderDoc::Init();
	Debug::Init();
	FileWatcher::Init(Paths::GetBinDataDir());

	MainWindow mainWindow;
	mainWindow.Create(kClientWidth, kClientHeight, "Render Lab");

	return mainWindow.Run();
}
