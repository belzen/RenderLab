#pragma once
#include "MathLib/Vec3.h"
#include "RdrResource.h"

class Renderer;

namespace RdrOffscreenTasks
{
	void QueueSpecularProbeCapture(const Vec3& position, const Rect& viewport, RdrResourceHandle hCubemapTex, int cubemapArrayIndex);
	void QueueDiffuseProbeCapture(const Vec3& position, const Rect& viewport, RdrResourceHandle hCubemapTex, int cubemapArrayIndex);

	void IssuePendingActions(Renderer& rRenderer);
}
