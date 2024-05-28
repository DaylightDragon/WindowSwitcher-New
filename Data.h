#pragma once

#include <atomic>
#include <string>
#include <vector>

struct Settings {
	bool allowAnyToBackgroundWindows;
	std::vector<std::string> allowToBackgroundWindows;
};