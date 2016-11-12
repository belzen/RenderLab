#include "Precompiled.h"
#include "UserConfig.h"
#include "UtilsLib\JsonUtils.h"

UserConfig g_userConfig;

void UserConfig::Load()
{
	char path[MAX_PATH];
	sprintf_s(path, "%s\\%s", Paths::GetDataDir(), "user.config");

	Json::Value jRoot;
	if (FileLoader::LoadJson(path, jRoot))
	{
		g_userConfig.renderDocPath = jRoot.get("renderDocPath", Json::nullValue).asString();
		g_userConfig.defaultScene = jRoot.get("defaultScene", "basic").asString();
	}
	else
	{
		g_userConfig.renderDocPath = "";
		g_userConfig.defaultScene = "basic";
	}
}