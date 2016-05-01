#include "Precompiled.h"
#include "Debug.h"
#include "FrameTimer.h"
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
	if (args[1].val.num == 0)
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
	DebugConsole::RegisterCommand("dbg", cmdShowDebugger, DebugCommandArgType::String, DebugCommandArgType::Number);
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

void Debug::Update(float dt)
{
	for (DebuggerList::iterator iter = s_activeDebuggers.begin(); iter != s_activeDebuggers.end(); ++iter)
	{
		(*iter)->Update(dt);
	}
}

void Debug::QueueDraw(Renderer& rRenderer, const Scene& rScene, const FrameTimer& rFrameTimer)
{
	DebugConsole::QueueDraw(rRenderer);

	// FPS and position display
	{
		char fpsStr[32];
		sprintf_s(fpsStr, "%.2f (%.2f ms)", rFrameTimer.GetFps(), 1000.f / rFrameTimer.GetFps());

		UI::Position uiPos(UI::Coord(1.f, UI::Units::Percentage), 0.f, 0.f, UI::AlignmentFlags::Right);
		Font::QueueDraw(rRenderer, uiPos, 20.f, fpsStr, Color::kWhite);

		const Vec3& camPos = rScene.GetMainCamera().GetPosition();
		char cameraStr[32];
		uiPos.y.val = 20.f;
		sprintf_s(cameraStr, "%.2f, %.2f, %.2f", camPos.x, camPos.y, camPos.z);
		Font::QueueDraw(rRenderer, uiPos, 20.f, cameraStr, Color::kWhite);
	}


	for (DebuggerList::iterator iter = s_activeDebuggers.begin(); iter != s_activeDebuggers.end(); ++iter)
	{
		(*iter)->QueueDraw(rRenderer);
	}
}
