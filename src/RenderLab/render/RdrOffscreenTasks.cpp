#include "Precompiled.h"
#include "RdrOffscreenTasks.h"
#include "Renderer.h"
#include "Camera.h"
#include "Scene.h"

namespace
{
	enum class RdrOffscreenTaskType
	{
		kDiffuseProbe,
		kSpecularProbe,
	};

	struct RdrOffscreenTask
	{
		RdrOffscreenTaskType type;
		int state;

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
	task.state = 0;
	task.position = position;
	task.viewport = viewport;
	task.pScene = pScene;
	task.hTargetResource = hCubemapTex;
	task.targetArrayIndex = cubemapArrayIndex;
	task.hDepthView = hDepthView;

	s_offscreenTasks.push_back(task);
}

void RdrOffscreenTasks::QueueDiffuseProbeCapture(const Vec3& position, Scene* pScene, const Rect& viewport,
	RdrResourceHandle hCubemapTex, int cubemapArrayIndex, RdrDepthStencilViewHandle hDepthView)
{
	RdrOffscreenTask task;
	task.type = RdrOffscreenTaskType::kDiffuseProbe;
	task.state = 0;
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

	const int kMaxOffscreenActionsPerFrame = 2;
	int i = 0;

	while (i < kMaxOffscreenActionsPerFrame && !s_offscreenTasks.empty())
	{
		RdrOffscreenTask& rTask = s_offscreenTasks[i];
		switch (rTask.type)
		{
		case RdrOffscreenTaskType::kSpecularProbe:
			{
				if (rTask.state < 6)
				{
					Camera cam;
					cam.SetAsCubemapFace(rTask.position, (CubemapFace)rTask.state, 0.01f, 1000.f);

					RdrSurface surface;
					surface.hRenderTarget = rPrimaryCommandList.CreateRenderTargetView(rTask.hTargetResource, (rTask.targetArrayIndex * (uint)CubemapFace::Count) + rTask.state, 1);
					surface.hDepthTarget = rTask.hDepthView;

					rRenderer.BeginOffscreenAction(L"Spec Probe Capture", cam, *rTask.pScene, false, rTask.viewport, surface);
					rRenderer.EndAction();

					// Clean up render target view
					rCleanupCommandList.ReleaseRenderTargetView(surface.hRenderTarget);
					++rTask.state;
				}
				else if (rTask.state == 6)
				{
					// todo2: pre-filter
					++rTask.state;
				}
				else if (rTask.state == 7)
				{
					s_offscreenTasks.erase(s_offscreenTasks.begin() + i);
					--i;
				}
			}
			break;

		case RdrOffscreenTaskType::kDiffuseProbe:
			{
				if (rTask.state < 6)
				{
					Camera cam;
					cam.SetAsCubemapFace(rTask.position, (CubemapFace)rTask.state, 0.01f, 1000.f);

					RdrSurface surface;
					surface.hRenderTarget = rPrimaryCommandList.CreateRenderTargetView(rTask.hTargetResource, (rTask.targetArrayIndex * (uint)CubemapFace::Count) + rTask.state, 1);
					surface.hDepthTarget = rTask.hDepthView;

					rRenderer.BeginOffscreenAction(L"Diffuse Probe Capture", cam, *rTask.pScene, false, rTask.viewport, surface);
					rRenderer.EndAction();

					// Clean up render target view
					rCleanupCommandList.ReleaseRenderTargetView(surface.hRenderTarget);
					rTask.state++;
				}
				else if (rTask.state == 6)
				{
					// todo2: spherical harmonics
					rTask.state++;
				}
				else if (rTask.state == 7)
				{
					s_offscreenTasks.erase(s_offscreenTasks.begin() + i);
					--i;
				}
			}
			break;
		}
	}
}