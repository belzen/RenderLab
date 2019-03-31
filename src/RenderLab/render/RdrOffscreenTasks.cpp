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

		RdrResourceHandle hTargetResource;
		uint targetArrayIndex;

		Vec3 position;
		Rect viewport;
	};

	std::vector<RdrOffscreenTask> s_offscreenTasks;
}

void RdrOffscreenTasks::QueueSpecularProbeCapture(const Vec3& position, const Rect& viewport, 
	RdrResourceHandle hCubemapTex, int cubemapArrayIndex)
{
	RdrOffscreenTask task;
	task.type = RdrOffscreenTaskType::kSpecularProbe;
	task.state = 0;
	task.position = position;
	task.viewport = viewport;
	task.hTargetResource = hCubemapTex;
	task.targetArrayIndex = cubemapArrayIndex;

	s_offscreenTasks.push_back(task);
}

void RdrOffscreenTasks::QueueDiffuseProbeCapture(const Vec3& position, const Rect& viewport,
	RdrResourceHandle hCubemapTex, int cubemapArrayIndex)
{
	RdrOffscreenTask task;
	task.type = RdrOffscreenTaskType::kDiffuseProbe;
	task.state = 0;
	task.position = position;
	task.viewport = viewport;
	task.hTargetResource = hCubemapTex;
	task.targetArrayIndex = cubemapArrayIndex;

	s_offscreenTasks.push_back(task);
}

void RdrOffscreenTasks::IssuePendingActions(Renderer& rRenderer)
{
	RdrResourceCommandList& rResCommandList = rRenderer.GetResourceCommandList();

	const int kMaxOffscreenActionsPerFrame = 2;
	for (int numCompletedActions = 0; numCompletedActions < kMaxOffscreenActionsPerFrame && !s_offscreenTasks.empty(); ++numCompletedActions)
	{
		RdrOffscreenTask& rTask = s_offscreenTasks[0];
		switch (rTask.type)
		{
		case RdrOffscreenTaskType::kSpecularProbe:
			{
				if (rTask.state < 6)
				{
					Camera cam;
					cam.SetAsCubemapFace(rTask.position, (CubemapFace)rTask.state, 0.01f, 1000.f);

					RdrRenderTargetViewHandle hRenderTarget = rResCommandList.CreateRenderTargetView(rTask.hTargetResource, (rTask.targetArrayIndex * (uint)CubemapFace::Count) + rTask.state, 1);
					RdrAction* pAction = RdrAction::CreateOffscreen(L"Spec Probe Capture", cam, false, rTask.viewport, hRenderTarget);
					Scene::QueueDraw(pAction);
					rRenderer.QueueAction(pAction);

					// Clean up render target view
					rResCommandList.ReleaseRenderTargetView(hRenderTarget);
					++rTask.state;
				}
				else if (rTask.state == 6)
				{
					// todo: pre-filter
					++rTask.state;
				}
				else if (rTask.state == 7)
				{
					s_offscreenTasks.erase(s_offscreenTasks.begin());
				}
			}
			break;

		case RdrOffscreenTaskType::kDiffuseProbe:
			{
				if (rTask.state < 6)
				{
					Camera cam;
					cam.SetAsCubemapFace(rTask.position, (CubemapFace)rTask.state, 0.01f, 1000.f);

					RdrRenderTargetViewHandle hRenderTarget = rResCommandList.CreateRenderTargetView(rTask.hTargetResource, (rTask.targetArrayIndex * (uint)CubemapFace::Count) + rTask.state, 1);
					RdrAction* pAction = RdrAction::CreateOffscreen(L"Diffuse Probe Capture", cam, false, rTask.viewport, hRenderTarget);
					Scene::QueueDraw(pAction);
					rRenderer.QueueAction(pAction);

					// Clean up render target view
					rResCommandList.ReleaseRenderTargetView(hRenderTarget);
					rTask.state++;
				}
				else if (rTask.state == 6)
				{
					// todo: spherical harmonics
					rTask.state++;
				}
				else if (rTask.state == 7)
				{
					s_offscreenTasks.erase(s_offscreenTasks.begin());
				}
			}
			break;
		}
	}
}