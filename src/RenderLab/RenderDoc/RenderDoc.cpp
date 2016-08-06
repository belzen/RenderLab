#include "Precompiled.h"
#include "RenderDocUtil.h"
#include "renderdoc_app.h"

namespace
{
	HMODULE s_hRenderDocDll = NULL;
	RENDERDOC_API_1_1_0* s_pRenderDocApi = nullptr;
}

bool RenderDoc::Init()
{
	s_hRenderDocDll = LoadLibrary(L"D:\\RenderDoc_0.30_64\\RenderDoc.dll");
	if (!s_hRenderDocDll)
		return false;

	pRENDERDOC_GetAPI pGetApiFunc = (pRENDERDOC_GetAPI)GetProcAddress(s_hRenderDocDll, "RENDERDOC_GetAPI");
	if (!pGetApiFunc)
		return false;

	return !!pGetApiFunc(eRENDERDOC_API_Version_1_1_0, (void**)&s_pRenderDocApi);
}

void RenderDoc::Capture()
{
	s_pRenderDocApi->TriggerCapture();
	if (!s_pRenderDocApi->IsRemoteAccessConnected())
	{
		s_pRenderDocApi->LaunchReplayUI(1, "");
	}
}