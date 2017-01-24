#pragma once

class Renderer;
class Camera;
class FrameTimer;

class IDebugger
{
public:
	virtual void OnActivate() = 0;
	virtual void OnDeactivate() = 0;

	virtual void Update() = 0;
	virtual void QueueDraw(RdrAction* pAction) = 0;
};

namespace Debug
{
	void Init();

	void RegisterDebugger(const char* name, IDebugger* pDebugger);
	void ActivateDebugger(const char* name);
	void DeactivateDebugger(const char* name);

	void Update();
	void QueueDraw(RdrAction* pAction);
}