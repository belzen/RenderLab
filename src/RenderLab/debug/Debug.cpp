#include "Precompiled.h"
#include "Debug.h"
#include "UI.h"
#include "Scene.h"
#include "render/Renderer.h"
#include "render/Font.h"
#include "DebugConsole.h"

namespace
{
	typedef std::map<std::string, IDebugger*, StringInvariantCompare> DebuggerMap;
	typedef std::vector<IDebugger*> DebuggerList;

	DebuggerMap s_registeredDebuggers;
	DebuggerList s_activeDebuggers;

}

void cmdShowDebugger(DebugCommandArg* args, int numArgs)
{
	if (args[1].val.inum == 0)
	{
		Debug::DeactivateDebugger(args[0].val.str);
	}
	else
	{
		Debug::ActivateDebugger(args[0].val.str);
	}
}

void Debug::Init()
{
	DebugConsole::Init();
	DebugConsole::RegisterCommand("dbg", cmdShowDebugger, DebugCommandArgType::String, DebugCommandArgType::Integer);
}

void Debug::RegisterDebugger(const char* name, IDebugger* pDebugger)
{
	s_registeredDebuggers.insert(std::make_pair(name, pDebugger));
}

void Debug::ActivateDebugger(const char* name)
{
	DebuggerMap::iterator mapIter = s_registeredDebuggers.find(name);
	if (mapIter != s_registeredDebuggers.end())
	{
		IDebugger* pDebugger = mapIter->second;
		DebuggerList::iterator iter = std::find(s_activeDebuggers.begin(), s_activeDebuggers.end(), pDebugger);

		if (iter == s_activeDebuggers.end())
		{
			pDebugger->OnActivate();
			s_activeDebuggers.push_back(pDebugger);
		}
	}
}

void Debug::DeactivateDebugger(const char* name)
{
	DebuggerMap::iterator mapIter = s_registeredDebuggers.find(name);
	if (mapIter != s_registeredDebuggers.end())
	{
		IDebugger* pDebugger = mapIter->second;
		DebuggerList::iterator iter = std::find(s_activeDebuggers.begin(), s_activeDebuggers.end(), pDebugger);

		if (iter != s_activeDebuggers.end())
		{
			pDebugger->OnDeactivate();
			s_activeDebuggers.erase(iter);
		}
	}
}

void Debug::Update()
{
	for (DebuggerList::iterator iter = s_activeDebuggers.begin(); iter != s_activeDebuggers.end(); ++iter)
	{
		(*iter)->Update();
	}
}

void Debug::QueueDraw(Renderer& rRenderer, const Camera& rCamera)
{
	DebugConsole::QueueDraw(rRenderer);

	// FPS and position display
	{
		char line[64];
		sprintf_s(line, "%.2f (%.2f ms)", Time::Fps(), 1000.f / Time::Fps());

		UI::Position uiPos(UI::Coord(1.f, UI::Units::Percentage), 0.f, 0.f, UI::AlignmentFlags::Right);
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		const Vec3& camPos = rCamera.GetPosition();
		uiPos.y.val = 20.f;
		sprintf_s(line, "%.2f, %.2f, %.2f", camPos.x, camPos.y, camPos.z);
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		const RdrProfiler& rProfiler = rRenderer.GetProfiler();
		
		uiPos.y.val += 20.f;
		sprintf_s(line, "GPU Frame: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::Frame));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Shadows: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::Shadows));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Z-Prepass: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::ZPrepass));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Light Cull: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::LightCulling));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Volumetric Fog: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::VolumetricFog));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Opaque: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::Opaque));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Alpha: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::Alpha));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Sky: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::Sky));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Post-Proc: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::PostProcessing));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  UI: %.3f ms", rProfiler.GetSectionTime(RdrProfileSection::UI));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Draw Calls: %d", rProfiler.GetCounter(RdrProfileCounter::DrawCall));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  Tris: %d", rProfiler.GetCounter(RdrProfileCounter::Triangles));
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);

		uiPos.y.val += 20.f;
		sprintf_s(line, "  RT: %.4f ms", rProfiler.GetRenderThreadTime() * 1000.f);
		Font::QueueDraw(rRenderer, uiPos, 20.f, line, Color::kWhite);
	}


	for (DebuggerList::iterator iter = s_activeDebuggers.begin(); iter != s_activeDebuggers.end(); ++iter)
	{
		(*iter)->QueueDraw(rRenderer);
	}
}
