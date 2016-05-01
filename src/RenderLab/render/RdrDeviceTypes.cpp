#include "Precompiled.h"
#include "RdrDeviceTypes.h"


int rdrGetTexturePitch(const int width, const RdrResourceFormat eFormat)
{
	switch (eFormat)
	{
	case RdrResourceFormat::R16G16B16A16_FLOAT:
		return width * 8;
	case RdrResourceFormat::R8_UNORM:
		return width * 1;
	case RdrResourceFormat::DXT1:
		return ((width + 3) & ~3) * 2;
	case RdrResourceFormat::DXT5:
	case RdrResourceFormat::DXT5_sRGB:
		return ((width + 3) & ~3) * 4;
	case RdrResourceFormat::B8G8R8A8_UNORM:
	case RdrResourceFormat::B8G8R8A8_UNORM_sRGB:
		return width * 4;
	default:
		assert(false);
		return 0;
	}
}

int rdrGetTextureRows(const int height, const RdrResourceFormat eFormat)
{
	switch (eFormat)
	{
	case RdrResourceFormat::R16G16B16A16_FLOAT:
	case RdrResourceFormat::R8_UNORM:
	case RdrResourceFormat::B8G8R8A8_UNORM:
	case RdrResourceFormat::B8G8R8A8_UNORM_sRGB:
		return height;
	case RdrResourceFormat::DXT1:
	case RdrResourceFormat::DXT5:
	case RdrResourceFormat::DXT5_sRGB:
		return ((height + 3) & ~3) / 4;
	default:
		assert(false);
		return 0;
	}
}
