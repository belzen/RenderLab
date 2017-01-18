#include "Precompiled.h"
#include "FileDialog.h"

std::string FileDialog::Show(const char* fileFilter)
{
	char filename[256] = { 0 };

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.lpstrFilter = fileFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filename;
	ofn.nMaxFile = ARRAY_SIZE(filename);
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;

	if (GetOpenFileNameA(&ofn))
	{
		return filename;
	}
	else
	{
		return "";
	}
}