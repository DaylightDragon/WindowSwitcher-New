#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <atomic>
#include <Windows.h>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"
#include "WindowSwitcherNew.h"
#include "DataStash.h"

struct Settings {
public:
	bool allowAnyToBackgroundWindows = false;
	std::vector<std::wstring> allowToBackgroundWindows = getDefaultAllowedTobackgroundWindows();
	std::vector<std::wstring> showingBackFromBackgroundWindows = getDefaultShowBackFromBackgroundList();
	int minCellsGridSizeX = 2;
	int minCellsGridSizeY = 2;
	int maxCellsGridSizeX = 2;
	int maxCellsGridSizeY = 2;
	int minimalGameWindowSizeX = 802;
	int minimalGameWindowSizeY = 631;
	bool ignoreManuallyLinkedWindows = true;
	bool automaticallyReturnToLastWindow = true;

	Settings(const YAML::Node& config);
};

struct RuntimeData {
	std::atomic<HWND> previouslyActiveWindow;
	std::atomic<bool> hadCooldownOnPrevWindow{ false };

	void saveCurrentNonLinkedForgroundWindow();
	void activatePrevActiveWindow();
};
