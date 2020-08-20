#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iomanip>
#include <filesystem>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <cstdlib>

namespace fs = std::filesystem;

int main()
{
	std::vector<fs::path> paths;
	for (auto entry : fs::recursive_directory_iterator("..\\res")) {
		if (!entry.is_directory()) {
			paths.push_back(entry);
		}
	}
	std::ofstream ofs("..\\resList.h");
	ofs << "R\"(";
	for (fs::path path : paths) {
		std::string result = path.string();
		result = result.substr(3);
		for (char& ch : result) {
			if (ch == '\\') {
				ch = '/';
			}
		}
		ofs << result << std::endl;
	}
	ofs << ")\"";
}