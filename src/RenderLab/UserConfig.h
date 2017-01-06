#pragma once

// User config and settings.
// Reads initial settings from "data/user.config"
struct UserConfig
{
	std::string renderDocPath;
	std::string defaultScene;
	bool debugShaders;
	bool debugDevice;
	bool attachRenderDoc;
	int vsync;

	static void Load();
};

extern UserConfig g_userConfig;