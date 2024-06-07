#include "Data.h"

Settings::Settings(const YAML::Node& config) {
	macroDelayInitial = getConfigInt(config, "settings/macro/general/initialDelayBeforeFirstIteration", 100);
	macroDelayBeforeSwitching = getConfigInt(config, "settings/macro/general/delayBeforeSwitchingWindow", 25);
	macroDelayAfterFocus = getConfigInt(config, "settings/macro/general/delayAfterSwitchingWindow", 200);
	delayWhenDoneAllWindows = getConfigInt(config, "settings/macro/general/delayWhenDoneAllWindows", 100);
	macroDelayBetweenSwitchingAndFocus = getConfigInt(config, "settings/macro/general/settingsChangeOnlyWhenReallyNeeded/afterSettingForegroundButBeforeSettingFocus", 10);
	specialSingleWindowModeEnabled = getConfigBool(config, "settings/macro/general/specialSingleWindowMode/enabled", false);
	specialSingleWindowModeKeyCode = getConfigString(config, "settings/macro/general/specialSingleWindowMode/keyCode", defaultMacroKey);

	sleepRandomnessPersent = getConfigInt(config, "settings/macro/general/randomness/delays/delayOffsetPersentage", 10);
	sleepRandomnessMaxDiff = getConfigInt(config, "settings/macro/general/randomness/delays/delayOffsetLimit", 40);

	minCellsGridSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/minCells/x", minCellsGridSizeX);
	minCellsGridSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/minCells/y", minCellsGridSizeY);
	maxCellsGridSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/maxCells/x", maxCellsGridSizeX);
	maxCellsGridSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/cellGrid/maxCells/y", maxCellsGridSizeY);
	minimalGameWindowSizeX = getConfigInt(config, "settings/windowOperations/windowLinking/constants/minimalGameWindowSize/x", minimalGameWindowSizeX);
	minimalGameWindowSizeY = getConfigInt(config, "settings/windowOperations/windowLinking/constants/minimalGameWindowSize/y", minimalGameWindowSizeY);

	ignoreManuallyLinkedWindows = getConfigBool(config, "settings/windowOperations/windowLinking/behaviour/ignoreManuallyLinkedWindows", ignoreManuallyLinkedWindows);

	usePrimitiveInterruptionAlgorythm = getConfigBool(config, "settings/macro/interruptions/advanced/primitiveInterruptionsAlgorythm/enabled", true);
	primitiveWaitInterval = getConfigInt(config, "settings/macro/interruptions/advanced/primitiveInterruptionsAlgorythm/checkEvery_milliseconds", 100);

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