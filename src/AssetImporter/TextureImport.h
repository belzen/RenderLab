#pragma once
#include <fstream>
#include <string>

namespace TextureImport
{
	bool Import(const std::string& srcFilename, std::string& dstFilename);
}
