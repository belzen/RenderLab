#include "../3rdparty/DirectXTex/include/DirectXTex.h"
#include <corecrt_wstring.h>
#include <assert.h>

#pragma comment (lib, "DirectXTex.lib")

struct Point
{
	int dx, dy;

	int DistSqr() 
	{
		return dx * dx + dy * dy;
	}
};

void CompareDistance(Point* grid, int x, int y, int w, int h, int offsetx, int offsety)
{
	if (x + offsetx < 0 || x + offsetx >= w)
		return;
	else if (y + offsety < 0 || y + offsety >= h)
		return;

	Point& pt = grid[x + y * w];
	Point other = grid[x + offsetx + (y + offsety) * w];

	other.dx += offsetx;
	other.dy += offsety;

	if (other.DistSqr() < pt.DistSqr())
		pt = other;
}

void CreateSDF(Point* grid, int w, int h)
{
	for (int x = 0; x < w; ++x)
	{
		for (int y = 0; y < h; ++y)
		{
			CompareDistance(grid, x, y, w, h, -1, 0);
			CompareDistance(grid, x, y, w, h, 1, 0);
			CompareDistance(grid, x, y, w, h, -1, -1);
			CompareDistance(grid, x, y, w, h, 0, -1);
			CompareDistance(grid, x, y, w, h, 1, -1);
		}
	}

	// Reverse direction
	for (int x = w - 1; x >= 0; --x)
	{
		for (int y = h - 1; y >= 0; --y)
		{
			CompareDistance(grid, x, y, w, h, -1, 0);
			CompareDistance(grid, x, y, w, h, 1, 0);
			CompareDistance(grid, x, y, w, h, -1, 1);
			CompareDistance(grid, x, y, w, h, 0, 1);
			CompareDistance(grid, x, y, w, h, 1, 1);
		}
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
		return 1;

	WCHAR filename[MAX_PATH] = { 0 };
	int c = MultiByteToWideChar(CP_UTF8, 0, argv[1], strlen(argv[1]), filename, MAX_PATH);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(hr == S_OK);

	// Load texture
	DWORD wicFlags = DirectX::TEX_FILTER_DEFAULT;
	DirectX::TexMetadata info;
	DirectX::ScratchImage* image = new (std::nothrow) DirectX::ScratchImage();

	hr = DirectX::LoadFromWICFile(filename, wicFlags, &info, *image);
	assert(hr == S_OK);

	// Create signed distance field
	uint8_t* pixels = image->GetPixels();
	int numPixels = info.width * info.height;
	int pixelSize = image->GetPixelsSize() / numPixels;
	int maxDim = max(info.width, info.height) + 1;

	Point* grid1 = new Point[numPixels]; // Test inside
	Point* grid2 = new Point[numPixels]; // Test outside
	for (int i = 0; i < numPixels; ++i)
	{
		if (pixels[i * pixelSize] > 125 && pixels[i * pixelSize + 3] > 125)
		{
			grid1[i].dx = grid1[i].dy = 0;
			grid2[i].dx = grid2[i].dy = maxDim;
		}
		else
		{
			grid1[i].dx = grid1[i].dy = maxDim;
			grid2[i].dx = grid2[i].dy = 0;
		}
	}

	CreateSDF(grid1, info.width, info.height);
	CreateSDF(grid2, info.width, info.height);

	// Merge grid values and write to file.

	DirectX::ScratchImage sdfImage;
	sdfImage.Initialize2D(DXGI_FORMAT_R8_UNORM, info.width, info.height, 1, 0, 0);
	pixels = sdfImage.GetPixels();
	int range = 3;
	for (int i = 0; i < numPixels; ++i)
	{
		double dist1 = sqrt((double)grid1[i].DistSqr());
		double dist2 = sqrt((double)grid2[i].DistSqr());
		double scale = (dist2 - dist1) / range;
		if (scale < -1.0)
			scale = -1.0;
		else if (scale > 1.0)
			scale = 1.0;
		pixels[i] = (uint8_t)( (scale + 1) / 2.0 * 255 );
	}

	// Save out as dds
	wcscat_s(filename, L".dds");
	DirectX::SaveToDDSFile(*sdfImage.GetImage(0, 0, 0), 0, filename);

	delete grid2;
	delete grid1;

	return 0;
}