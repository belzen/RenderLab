#pragma once

// Global state values for debugging and feature testing.
struct GlobalState
{
	int enableInstancing;
	bool wireframe;
	int msaaLevel;

	static void Init();
};

extern GlobalState g_debugState;