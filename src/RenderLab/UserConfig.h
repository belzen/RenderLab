#pragma once

struct UserConfig
{
	std::string renderDocPath;

	static void Load();
};

extern UserConfig g_userConfig;