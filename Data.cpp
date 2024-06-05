#include <atomic>
#include <string>
#include <vector>
#include <Windows.h>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"

// made this public because of 2.2 -> 2.3 config migration
// can move such things to a separate file for initializations
inline std::vector<std::wstring> getDefaultShowBackFromBackgroundList() {
	std::vector<std::wstring> list;
	list.push_back(L"Roblox");
	list.push_back(L"VMware Workstation");
	list.push_back(L"*WindowSwitcherNew*");
	return list;
}

struct Settings {
private:
	std::vector<std::wstring> getDefaultAllowedTobackgroundWindows() {
		std::vector<std::wstring> result;
		result.push_back(L"Roblox");
		result.push_back(L"*WindowSwitcherNew*");
		return result;
	}

public:
	bool allowAnyToBackgroundWindows = false;
	std::vector<std::wstring> allowToBackgroundWindows = getDefaultAllowedTobackgroundWindows();
	std::vector<std::wstring> defaultFastForegroundWindows = getDefaultShowBackFromBackgroundList();
	int minCellsGridSizeX = 2;
	int minCellsGridSizeY = 2;
	int maxCellsGridSizeX = 2;
	int maxCellsGridSizeY = 2;
	int minimalGameWindowSizeX = 802;
	int minimalGameWindowSizeY = 631;
	bool ignoreManuallyLinkedWindows = true;

	Settings(const YAML::Node& config) {
		minCellsGridSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/minCells/x", minCellsGridSizeX);
		minCellsGridSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/minCells/y", minCellsGridSizeY);
		maxCellsGridSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/maxCells/x", maxCellsGridSizeX);
		maxCellsGridSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/maxCells/y", maxCellsGridSizeY);

		minimalGameWindowSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/constants/minimalGameWindowSize/x", minimalGameWindowSizeX);
		minimalGameWindowSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/constants/minimalGameWindowSize/y", minimalGameWindowSizeY);

		ignoreManuallyLinkedWindows = getConfigBool(config, "settings/windowOperations/windowLinking/behaviour/ignoreManuallyLinkedWindows", ignoreManuallyLinkedWindows);

		allowAnyToBackgroundWindows = getConfigBool(config, "settings/windowOperations/windowVisualState/hidingToBackground/allowHidingAnyWindowToBackground", allowAnyToBackgroundWindows);
		allowToBackgroundWindows = getConfigVectorWstring(config, "settings/windowOperations/windowVisualState/hidingToBackground/allowSpecificWindows", allowToBackgroundWindows);

		defaultFastForegroundWindows = getConfigVectorWstring(config, "settings/windowOperations/windowVisualState/showingBackFromBackground/alwaysShowSpecificWindows", defaultFastForegroundWindows);
	}
};

struct RuntimeData {
	std::atomic<HWND> previouslyActiveWindow;
	std::atomic<bool> hadCooldownOnPrevWindow{ false };

	void saveCurrentForgroundWindow() {
		previouslyActiveWindow.store(GetForegroundWindow());
	}

	void activatePrevActiveWindow() {
		HWND hwnd = previouslyActiveWindow.load();
		if (IsWindow(hwnd)) {
			SetForegroundWindow(hwnd);
			//SetFocus(hwnd);
		}
	}
};
