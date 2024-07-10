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

namespace WindowSwitcher {
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
		bool useSingleGlobalCooldownTimer = true;
		int globalTimerCooldownValue = 15000;

		int overlayMonitorNumber = 1;
		int overlayPositionTopBottomOrCenter = 2;
		int overlayPositionLeftRightOrCenter = 2;
		int overlayPositionPaddingX = 10;
		int overlayPositionPaddingY = 40;
		int overlaySizeX = 100;
		int overlaySizeY = 40;
		int overlayBarWidth = 80;
		int overlayBarHeight = 5;
		int overlayBarOutlineSize = 2;
		int overlayTextSize = 18;
		int overlayTextOutlineSize = 4;
		int overlayTextVerticalOffset = -4;

		Settings(const YAML::Node& config);
	};

	struct RuntimeData {
		std::atomic<HWND> previouslyActiveWindow;
		std::atomic<bool> hadCooldownOnPrevWindow{ false };

		void saveCurrentNonLinkedForgroundWindow();
		void activatePrevActiveWindow();
	};
}