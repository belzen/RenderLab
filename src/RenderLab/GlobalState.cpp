#include "Precompiled.h"
#include "GlobalState.h"

GlobalState g_debugState = { 0 };

namespace
{
	void cmdSetInstancingEnabled(DebugCommandArg *args, int numArgs)
	{
		g_debugState.enableInstancing = args[0].val.inum;
	}

	void cmdSetWireframeEnabled(DebugCommandArg *args, int numArgs)
	{
		g_debugState.wireframe = (args[0].val.inum != 0);
	}
}

void GlobalState::Init()
{
	// Register debug commands.
	DebugConsole::RegisterCommand("instancing", cmdSetInstancingEnabled, DebugCommandArgType::Integer);
	DebugConsole::RegisterCommand("wireframe", cmdSetWireframeEnabled, DebugCommandArgType::Integer);

	// Init default state
	g_debugState.enableInstancing = false; // Leaving this disabled as the engine is far more GPU bound than CPU right now.
	g_debugState.msaaLevel = 1;
	g_debugState.wireframe = false;
	g_debugState.visMode = DebugVisMode::kNone;
}