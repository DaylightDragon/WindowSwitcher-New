#include <atomic>
#include <string>
#include <vector>
#include <Windows.h>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"
#include "WindowSwitcherNew.h"
#include "Data.h"

WindowSwitcher::Settings::Settings(const YAML::Node& config) {
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

	automaticallyReturnToLastWindow = getConfigBool(config, "settings/macro/general/automaticallyReturnToPreviousWindow", automaticallyReturnToLastWindow);

	useSingleGlobalCooldownTimer = getConfigBool(config, "settings/macro/general/singleGlobalCooldownTimer/enabled", useSingleGlobalCooldownTimer);
	globalTimerCooldownValue = getConfigInt(config, "settings/macro/general/singleGlobalCooldownTimer/cooldownDuration_milliseconds", globalTimerCooldownValue);

	overlayMonitorNumber = getConfigInt(config, "settings/stateOverlay/locations/window/monitorNumber", overlayMonitorNumber);
	overlayPositionLeftRightOrCenter = std::clamp(getConfigInt(config, "settings/stateOverlay/locations/window/position/anchors/horizontalPoint_whichOne_leftRightCenter", overlayPositionLeftRightOrCenter), 1, 3);
	overlayPositionTopBottomOrCenter = std::clamp(getConfigInt(config, "settings/stateOverlay/locations/window/position/anchors/verticalPoint_whichOne_topBottomCenter", overlayPositionTopBottomOrCenter), 1, 3);

	overlayPositionPaddingX = getConfigInt(config, "settings/stateOverlay/locations/window/position/padding/horizontal", overlayPositionPaddingX);
	overlayPositionPaddingY = getConfigInt(config, "settings/stateOverlay/locations/window/position/padding/vertical", overlayPositionPaddingY);
	overlaySizeX = getConfigInt(config, "settings/stateOverlay/locations/window/size/horizontal", overlaySizeX);
	overlaySizeY = getConfigInt(config, "settings/stateOverlay/locations/window/size/vertical", overlaySizeY);
	
	overlayBarWidth = getConfigInt(config, "settings/stateOverlay/locations/macroTimerIndicator/coloredBar/size/horizontal", overlayBarWidth);
	overlayBarHeight = getConfigInt(config, "settings/stateOverlay/locations/macroTimerIndicator/coloredBar/size/vertical", overlayBarHeight);
	overlayBarOutlineSize = getConfigInt(config, "settings/stateOverlay/locations/macroTimerIndicator/coloredBar/outlineSize", overlayBarOutlineSize);

	overlayTextSize = getConfigInt(config, "settings/stateOverlay/locations/macroTimerIndicator/valueText/textSize", overlayTextSize);
	overlayTextOutlineSize = getConfigInt(config, "settings/stateOverlay/locations/macroTimerIndicator/valueText/outlineSize", overlayTextOutlineSize);
	overlayTextVerticalOffset = getConfigInt(config, "settings/stateOverlay/locations/macroTimerIndicator/valueText/offsets/vertical", overlayTextVerticalOffset);
}

void WindowSwitcher::RuntimeData::saveCurrentNonLinkedForgroundWindow() {
	if (!settings.load()->automaticallyReturnToLastWindow) {
		previouslyActiveWindow.store(NULL);
		return;
	}

	HWND hwnd = GetForegroundWindow();

	bool linkedManually = windowIsLinkedManually(hwnd);
	bool linkedAutomatically = windowIsLinkedAutomatically(hwnd);
	//std::cout << hwnd << ": " << linkedManually << ", " << linkedAutomatically << '\n';
	
	if (!linkedManually && !linkedAutomatically) { // Commented, this was only to avoid a bug
		previouslyActiveWindow.store(hwnd);
	}
}

void WindowSwitcher::RuntimeData::activatePrevActiveWindow() {
	if (!settings.load()->automaticallyReturnToLastWindow) return;

	HWND curForgr = GetForegroundWindow();

	HWND hwnd = previouslyActiveWindow.load();
	if (IsWindow(hwnd)) {
		bool savedLinkedManually = windowIsLinkedManually(hwnd);
		bool savedLinkedAutomatically = windowIsLinkedAutomatically(hwnd);

		bool currentLinkedManually = windowIsLinkedManually(curForgr);
		bool currentLinkedAutomatically = windowIsLinkedAutomatically(curForgr);

		// If last saved window is not linked, but current foreground one is
		if ((!savedLinkedManually && !savedLinkedAutomatically) && (currentLinkedManually || currentLinkedAutomatically)) {
			SetForegroundWindow(hwnd);
			//SetFocus(hwnd);
		}
	}
}