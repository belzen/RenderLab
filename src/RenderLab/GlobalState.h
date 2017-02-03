#pragma once

enum class DebugVisMode
{
	kNone,
	kSsao,

	kNumModes
};

// Global state values for debugging and feature testing.
struct GlobalState
{
	int enableInstancing;
	bool wireframe;
	int msaaLevel;
	DebugVisMode visMode;

	static void Init();
};

extern GlobalState g_debugState;