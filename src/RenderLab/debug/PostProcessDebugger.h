#pragma once

#include "render/RdrRequests.h"
#include "Debug.h"

class RdrPostProcess;
class Renderer;

class PostProcessDebugger : IDebugger
{
public:
	PostProcessDebugger();

	void Init(RdrPostProcess* pPostProc);

	bool IsActive() const;
	void OnActivate();
	void OnDeactivate();

	void Update();
	void QueueDraw(RdrAction* pAction);

private:
	RdrPostProcess* m_pPostProc;
	bool m_bIsActive;
};

inline bool PostProcessDebugger::IsActive() const
{
	return m_bIsActive;
}