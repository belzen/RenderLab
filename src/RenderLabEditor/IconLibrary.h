#pragma once

enum class Icon
{
	kFolder,
	kModel,
	kTexture,
	kMaterial,
	kPostProcessEffects,
	kSky,

	kNumIcons
};

namespace IconLibrary
{
	HBITMAP GetIconImage(Icon icon);
}
