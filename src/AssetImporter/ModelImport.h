#pragma once
#include <fstream>
#include <string>

namespace ModelImport
{
	bool ImportObj(const std::string& srcFilename, std::string& dstFilename);
}
