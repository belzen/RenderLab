#pragma once
#include <fstream>
#include <string>

namespace ModelImport
{
	bool ImportObj(const std::string& srcFilename, std::string& dstFilename);
	bool ImportFbx(const std::string& srcFilename, std::string& dstFilename);
}
