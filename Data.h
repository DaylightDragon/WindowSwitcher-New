#pragma once

#include "ConfigOperations.h"
#include "DataStash.h"
#include "InputRelated.h"
#include "WindowSwitcherNew.h"

#include <yaml-cpp/yaml.h>

#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <Windows.h>

struct Settings {
public:
	KeySequence* mainSequence;
	std::map<std::string, KeySequence*> knownOtherSequences = std::map<std::string, KeySequence*>();

	// Main misc settings variables
	int macroDelayInitial;
	int macroDelayBeforeSwitching;
	int macroDelayBetweenSwitchingAndFocus;
	int macroDelayAfterFocus;
	int delayWhenDoneAllWindows;
	bool specialSingleWindowModeEnabled;
	std::string specialSingleWindowModeKeyCode;
	bool usePrimitiveInterruptionAlgorythm = false;
	int primitiveWaitInterval = 100;

	// Input randomness
	int sleepRandomnessPersent = 10;
	int sleepRandomnessMaxDiff = 40;

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

	Settings(const YAML::Node& config);
};

struct RuntimeData {
	std::atomic<HWND> previouslyActiveWindow;
	std::atomic<bool> hadCooldownOnPrevWindow{ false };

	void saveCurrentNonLinkedForgroundWindow();
	void activatePrevActiveWindow();
};
