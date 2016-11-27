#include "Precompiled.h"
#include "RdrOffscreenTasks.h"
#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"

namespace
{
	enum class RdrOffscreenTaskType
	{
		kSpecularProbe,
	};

	struct RdrOffscreenTask
	{
		RdrOffscreenTaskType type;

		RdrDepthStencilViewHandle hDepthView;
		RdrResourceHandle hTargetResource;
		uint targetArrayIndex;

		Vec3 position;
		Rect viewport;
		Scene* pScene;
	};

	std::vector<RdrOffscreenTask> s_offscreenTasks;
}

void RdrOffscreenTasks::QueueSpecularProbeCapture(const Vec3& position, Scene* pScene, const Rect& viewport, 
	RdrResourceHandle hCubemapTex, int cubemapArrayIndex, RdrDepthStencilViewHandle hDepthView)
{
	RdrOffscreenTask task;
	task.type = RdrOffscreenTaskType::kSpecularProbe;
	task.position = position;
	task.viewport = viewport;
	task.pScene = pScene;
	task.hTargetResource = hCubemapTex;
	task.targetArrayIndex = cubemapArrayIndex;
	task.hDepthView = hDepthView;

	s_offscreenTasks.push_back(task);
}

void RdrOffscreenTasks::IssuePendingActions(Renderer& rRenderer)
{
	RdrResourceCommandList& rPrimaryCommandList = rRenderer.GetPreFrameCommandList();
	RdrResourceCommandList& rCleanupCommandList = rRenderer.GetPostFrameCommandList();

	for (int i = 0; i < s_offscreenTasks.size(); ++i)
	{
		RdrOffscreenTask& rTask = s_offscreenTasks[i];
		switch (rTask.type)
		{
		case RdrOffscreenTaskType::kSpecularProbe:
		{
			Camera cam;
			RdrSurface surface;
			for (int face = 0; face < (int)CubemapFace::Count; ++face)
			{
				surface.hRenderTarget = rPrimaryCommandList.CreateRenderTargetView(rTask.hTargetResource, (rTask.targetArrayIndex * (uint)CubemapFace::Count) + face, 1);
				surface.hDepthTarget = rTask.hDepthView;

				cam.SetAsCubemapFace(rTask.position, (CubemapFace)face, 0.01f, 1000.f);
				rRenderer.BeginOffscreenAction(L"Spec Probe Capture", cam, *rTask.pScene, false, rTask.viewport, surface);
				rRenderer.EndAction();

				// Clean up render target view
				rCleanupCommandList.ReleaseRenderTargetView(surface.hRenderTarget);
			}

			s_offscreenTasks.erase(s_offscreenTasks.begin() + i);
			--i;
		}
		break;
		}
	}
}