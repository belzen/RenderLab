#include "Precompiled.h"
#include "IconLibrary.h"
#include "UtilsLib/Paths.h"

namespace
{
	struct IconData
	{
		const char* assetName;
		HBITMAP hBitmap;
	};

	IconData s_icons[(int)Icon::kNumIcons] = 
	{
		{ "icons/folder.bmp", 0 },	   // kFolder
		{ "icons/model.bmp", 0 },	   // kModel
		{ "icons/texture.bmp", 0 },	   // kTexture
		{ "icons/material.bmp", 0 },   // kMaterial
		{ "icons/postprocfx.bmp", 0 }, // kPostProcessEffects
		{ "icons/sky.bmp", 0 },	       // kSky
	};
}

HBITMAP IconLibrary::GetIconImage(Icon icon)
{
	IconData& rIconData = s_icons[(int)icon];

	if (!rIconData.hBitmap)
	{
		char imagePath[MAX_PATH];
		sprintf_s(imagePath, "%s/%s", Paths::GetDataDir(), rIconData.assetName);
		rIconData.hBitmap = (HBITMAP)::LoadImageA(NULL, imagePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	}

	return rIconData.hBitmap;
}