#include "Precompiled.h"
#include "RdrDeviceTypes.h"

namespace
{
	struct RdrResourceBlockInfo
	{
		int blockPixelSize;
		int blockByteSize;
	};

	RdrResourceBlockInfo s_resourceBlockInfo[]
	{
		{1, 1}, // UNKNOWN
		{1, 2}, // D16
		{1, 4}, // D24_UNORM_S8_UINT
		{1, 2}, // R16_UNORM
		{1, 2}, // R16_UINT
		{1, 2}, // R16_FLOAT
		{1, 4}, // R32_UINT
		{1, 4}, // R16G16_FLOAT
		{1, 1}, // R8_UNORM
		{4, 2}, // DXT1
		{4, 2}, // DXT1_sRGB
		{4, 4}, // DXT5
		{4, 4}, // DXT5_sRGB
		{1, 4}, // B8G8R8A8_UNORM
		{1, 4}, // B8G8R8A8_UNORM_sRGB
		{1, 4}, // R8G8B8A8_UNORM
		{1, 2}, // R8G8_UNORM
		{1, 8}, // R16G16B16A16_FLOAT
	};

	static_assert(ARRAY_SIZE(s_resourceBlockInfo) == (int)RdrResourceFormat::Count, "Missing resource block info!");
}

int rdrGetTexturePitch(const int width, const RdrResourceFormat eFormat)
{
	const RdrResourceBlockInfo& rBlockInfo = s_resourceBlockInfo[(int)eFormat];
	int blockPixelsMinusOne = rBlockInfo.blockPixelSize - 1;
	int pitch = ((width + blockPixelsMinusOne) & ~blockPixelsMinusOne) * rBlockInfo.blockByteSize;
	return pitch;
}

int rdrGetTextureRows(const int height, const RdrResourceFormat eFormat)
{
	const RdrResourceBlockInfo& rBlockInfo = s_resourceBlockInfo[(int)eFormat];
	int blockPixelsMinusOne = rBlockInfo.blockPixelSize - 1;
	int rows = ((height + blockPixelsMinusOne) & ~blockPixelsMinusOne) / rBlockInfo.blockPixelSize;
	return rows;
}
