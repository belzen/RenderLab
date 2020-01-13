#pragma once
#include <fstream>
#include <string>

namespace ModelImport
{
	bool ImportObj(const std::string& srcFilename, const std::string& dstFilename);
	bool ImportFbx(const std::string& srcFilename, const std::string& dstFilename);
}
