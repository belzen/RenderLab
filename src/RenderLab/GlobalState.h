#pragma once

struct GlobalState
{
	int enableInstancing;
	bool debugShaders;
	bool debugDevice;
	bool attachRenderDoc;
	bool rebuildDrawOps;
	bool wireframe;
	int msaaLevel;

	static void Init();
};

extern GlobalState g_debugState;