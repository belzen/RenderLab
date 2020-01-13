#include "Precompiled.h"
#include "UserConfig.h"
#include "UtilsLib\JsonUtils.h"

UserConfig g_userConfig;

void UserConfig::Load()
{
	char path[MAX_PATH];
	sprintf_s(path, "%s\\%s", Paths::GetSrcDataDir(), "user.config");

	// Default config
	g_userConfig.renderDocPath = "";
	g_userConfig.defaultScene = "basic";
	g_userConfig.debugDevice = false;
	g_userConfig.debugShaders = false;
	g_userConfig.attachRenderDoc = false;
	g_userConfig.vsync = 0;

	// Override settings if the User.config file exists.
	Json::Value jRoot;
	if (FileLoader::LoadJson(path, jRoot))
	{
		g_userConfig.renderDocPath = jRoot.get("renderDocPath", g_userConfig.renderDocPath).asString();
		g_userConfig.defaultScene = jRoot.get("defaultScene", g_userConfig.defaultScene).asString();

		g_userConfig.attachRenderDoc = jRoot.get("attachRenderDoc", g_userConfig.attachRenderDoc).asBool();
		g_userConfig.debugDevice = jRoot.get("debugDevice", g_userConfig.debugDevice).asBool();
		g_userConfig.debugShaders = jRoot.get("debugShaders", g_userConfig.debugShaders).asBool();

		g_userConfig.vsync = jRoot.get("vsync", g_userConfig.vsync).asInt();
	}
}