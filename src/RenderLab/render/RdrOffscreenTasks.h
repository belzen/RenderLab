#pragma once
#include "MathLib/Vec3.h"
#include "RdrResource.h"

class Renderer;

namespace RdrOffscreenTasks
{
	void QueueSpecularProbeCapture(const Vec3& position, const Rect& viewport, 
		RdrResourceHandle hCubemapTex, int cubemapArrayIndex, RdrDepthStencilViewHandle hDepthView);
	void QueueDiffuseProbeCapture(const Vec3& position, const Rect& viewport,
		RdrResourceHandle hCubemapTex, int cubemapArrayIndex, RdrDepthStencilViewHandle hDepthView);

	void IssuePendingActions(Renderer& rRenderer);
}
