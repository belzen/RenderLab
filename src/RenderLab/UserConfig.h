#pragma once

struct UserConfig
{
	std::string renderDocPath;
	std::string defaultScene;

	static void Load();
};

extern UserConfig g_userConfig;