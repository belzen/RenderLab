#pragma once

#include "input/Input.h"

class Renderer;

enum class DebugCommandArgType
{
	Number,
	String,
};

union DebugCommandArg
{
	DebugCommandArgType type;
	union
	{
		float num;
		char* str;
	} val;
};

typedef void (*DebugCommandCallback)(DebugCommandArg* args, int numArgs);

namespace DebugConsole
{
	void Init();

	void RegisterCommand(const char* name, DebugCommandCallback func, DebugCommandArgType arg);
	void RegisterCommand(const char* name, DebugCommandCallback func, DebugCommandArgType arg1, DebugCommandArgType arg2);
	void RegisterCommand(const char* name, DebugCommandCallback func, DebugCommandArgType arg1, DebugCommandArgType arg2, DebugCommandArgType arg3);
	void RegisterCommand(const char* name, DebugCommandCallback func, DebugCommandArgType arg1, DebugCommandArgType arg2, DebugCommandArgType arg3, DebugCommandArgType arg4);

	void QueueDraw(Renderer& rRenderer);

	void ToggleActive();
}