#pragma once
#include "MathLib/Vec3.h"
#include "RdrResource.h"

class Renderer;
class Scene;

namespace RdrOffscreenTasks
{
	void QueueSpecularProbeCapture(const Vec3& position, Scene* pScene, const Rect& viewport, 
		RdrResourceHandle hCubemapTex, int cubemapArrayIndex, RdrDepthStencilViewHandle hDepthView);

	void IssuePendingActions(Renderer& rRenderer);
}
