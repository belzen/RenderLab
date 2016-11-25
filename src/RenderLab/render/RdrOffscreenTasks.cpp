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

	enum class RdrOffscreenTaskState
	{
		kPending,
		kCleanup
	};

	struct RdrOffscreenTask
	{
		RdrOffscreenTaskType type;
		RdrOffscreenTaskState state;
		RdrSurface outputSurfaces[(int)CubemapFace::Count];
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
	task.state = RdrOffscreenTaskState::kPending;
	task.position = position;
	task.viewport = viewport;
	task.pScene = pScene;

	for (int i = 0; i < 6; ++i)
	{
		task.outputSurfaces[i].hRenderTarget = RdrResourceSystem::CreateRenderTargetView(hCubemapTex, (cubemapArrayIndex * (int)CubemapFace::Count) + i, 1);
		task.outputSurfaces[i].hDepthTarget = hDepthView;
	}

	s_offscreenTasks.push_back(task);
}

void RdrOffscreenTasks::IssuePendingActions(Renderer& rRenderer)
{
	for (int i = 0; i < s_offscreenTasks.size(); ++i)
	{
		RdrOffscreenTask& rTask = s_offscreenTasks[i];

		// Cleanup resources a frame after the task is done when we know they are unused.
		if (rTask.state == RdrOffscreenTaskState::kCleanup)
		{
			for (int face = 0; face < (int)CubemapFace::Count; ++face)
			{
				if (rTask.outputSurfaces[face].hRenderTarget)
				{
					RdrResourceSystem::ReleaseRenderTargetView(rTask.outputSurfaces[face].hRenderTarget);
					rTask.outputSurfaces[face].hRenderTarget = 0;
				}
			}

			s_offscreenTasks.erase(s_offscreenTasks.begin() + i);
			--i;
			continue;
		}

		// 
		switch (rTask.type)
		{
		case RdrOffscreenTaskType::kSpecularProbe:
		{
			Camera cam;
			for (int face = 0; face < (int)CubemapFace::Count; ++face)
			{
				cam.SetAsCubemapFace(rTask.position, (CubemapFace)face, 0.01f, 1000.f);
				rRenderer.BeginOffscreenAction(L"Spec Probe Capture", cam, *rTask.pScene, false, rTask.viewport, rTask.outputSurfaces[face]);
				rRenderer.EndAction();
			}
			rTask.state = RdrOffscreenTaskState::kCleanup;
		}
		break;
		}
	}
}