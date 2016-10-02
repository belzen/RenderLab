#include "Precompiled.h"
#include "GlobalState.h"

GlobalState g_debugState = { 0 };

namespace
{
	void cmdSetInstancingEnabled(DebugCommandArg *args, int numArgs)
	{
		g_debugState.enableInstancing = args[0].val.inum;
		g_debugState.rebuildDrawOps = 1;
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
	g_debugState.debugDevice = true;
	g_debugState.debugShaders = false;
	g_debugState.attachRenderDoc = true;
	g_debugState.msaaLevel = 2;
	g_debugState.wireframe = false;
}