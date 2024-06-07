#include <atomic>
#include <string>
#include <vector>
#include <Windows.h>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"
#include "WindowSwitcherNew.h"
#include "Data.h"

Settings::Settings(const YAML::Node& config) {
	minCellsGridSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/minCells/x", minCellsGridSizeX);
	minCellsGridSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/minCells/y", minCellsGridSizeY);
	maxCellsGridSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/maxCells/x", maxCellsGridSizeX);
	maxCellsGridSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/maxCells/y", maxCellsGridSizeY);

	minimalGameWindowSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/constants/minimalGameWindowSize/x", minimalGameWindowSizeX);
	minimalGameWindowSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/constants/minimalGameWindowSize/y", minimalGameWindowSizeY);

	ignoreManuallyLinkedWindows = getConfigBool(config, "settings/windowOperations/windowLinking/behaviour/ignoreManuallyLinkedWindows", ignoreManuallyLinkedWindows);

	allowAnyToBackgroundWindows = getConfigBool(config, "settings/windowOperations/windowVisualState/hidingToBackground/allowHidingAnyWindowToBackground", allowAnyToBackgroundWindows);
	allowToBackgroundWindows = getConfigVectorWstring(config, "settings/windowOperations/windowVisualState/hidingToBackground/allowSpecificWindows", allowToBackgroundWindows);

	showingBackFromBackgroundWindows = getConfigVectorWstring(config, "settings/windowOperations/windowVisualState/showingBackFromBackground/alwaysShowSpecificWindows", showingBackFromBackgroundWindows);
}

void RuntimeData::saveCurrentNonLinkedForgroundWindow() {
	HWND hwnd = GetForegroundWindow();

	bool linkedManually = windowIsLinkedManually(hwnd);
	bool linkedAutomatically = windowIsLinkedAutomatically(hwnd);
	//std::cout << hwnd << ": " << linkedManually << ", " << linkedAutomatically << '\n';
	if (!linkedManually && !linkedAutomatically) {
		previouslyActiveWindow.store(hwnd);
	}
}

void RuntimeData::activatePrevActiveWindow() {
	HWND hwnd = previouslyActiveWindow.load();
	if (IsWindow(hwnd)) {
		bool linkedManually = windowIsLinkedManually(hwnd);
		bool linkedAutomatically = windowIsLinkedAutomatically(hwnd);

		if (!linkedManually && !linkedAutomatically) {
			SetForegroundWindow(hwnd);
			//SetFocus(hwnd);
		}
	}
}