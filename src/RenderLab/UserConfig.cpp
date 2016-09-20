#include "Precompiled.h"
#include "UserConfig.h"
#include "UtilsLib\JsonUtils.h"

UserConfig g_userConfig;

void UserConfig::Load()
{
	char path[MAX_PATH];
	sprintf_s(path, "%s\\%s", Paths::GetBinDataDir(), "user.config");

	Json::Value jRoot;
	if (FileLoader::LoadJson(path, jRoot))
	{
		g_userConfig.renderDocPath = jRoot.get("renderDocPath", Json::nullValue).asString();
	}
	else
	{
		g_userConfig.renderDocPath = "";
	}
}