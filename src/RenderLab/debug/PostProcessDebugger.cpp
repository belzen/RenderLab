#include "Precompiled.h"
#include "PostProcessDebugger.h"
#include "render/Renderer.h"
#include "render/Font.h"
#include "UI.h"

PostProcessDebugger::PostProcessDebugger()
	: m_pPostProc(nullptr)
	, m_bIsActive(false)
{

}

void PostProcessDebugger::Init(RdrPostProcess* pPostProc)
{
	m_pPostProc = pPostProc;

	Debug::RegisterDebugger("postproc", this);
}

void PostProcessDebugger::OnActivate()
{
	m_bIsActive = true;
}

void PostProcessDebugger::OnDeactivate()
{
	m_bIsActive = false;
}

void PostProcessDebugger::Update()
{

}

void PostProcessDebugger::QueueDraw(RdrAction* pAction)
{
	char str[128];
	const RdrPostProcessDbgData& dbgData = m_pPostProc->GetDebugData();

	UI::Position uiPos(0.f, 40.f, 0.f, UI::AlignmentFlags::Left);
	sprintf_s(str, "Avg Lum: %f", dbgData.adaptedLum);
	Font::QueueDraw(pAction, uiPos, 20.f, str, Color::kWhite);

	uiPos.y.val += 20.f;
	sprintf_s(str, "Exposure: %f (%.4f EV)", dbgData.linearExposure, log2f(dbgData.linearExposure));
	Font::QueueDraw(pAction, uiPos, 20.f, str, Color::kWhite);

	uiPos.y.val += 20.f;
	sprintf_s(str, "Lum @ cursor: %f (%.3f, %.3f, %.3f)", dbgData.lumAtCursor, dbgData.lumColor.x, dbgData.lumColor.y, dbgData.lumColor.z);
	Font::QueueDraw(pAction, uiPos, 20.f, str, Color::kWhite);
}