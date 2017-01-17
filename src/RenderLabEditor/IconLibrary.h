#pragma once

enum class Icon
{
	kFolder,
	kModel,
	kTexture,
	kMaterial,

	kNumIcons
};

namespace IconLibrary
{
	HBITMAP GetIconImage(Icon icon);
}
