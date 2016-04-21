#pragma once

class Renderer;
class Scene;
class FrameTimer;

class IDebugger
{
public:
	virtual void OnActivate() = 0;
	virtual void OnDeactivate() = 0;

	virtual void Update(float dt) = 0;
	virtual void QueueDraw(Renderer& rRenderer) = 0;
};

namespace Debug
{
	void Init();

	void RegisterDebugger(const char* name, IDebugger* pDebugger);
	void ActivateDebugger(const char* name);
	void DeactivateDebugger(const char* name);

	void Update(float dt);
	void QueueDraw(Renderer& rRenderer, const Scene& rScene, const FrameTimer& rFrameTimer);
}