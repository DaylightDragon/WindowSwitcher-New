#include <atomic>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"

struct Settings {
	bool allowAnyToBackgroundWindows;
	std::vector<std::string> allowToBackgroundWindows;
	int minCellsGridSizeX = 2;
	int minCellsGridSizeY = 2;
	int maxCellsGridSizeX = 2;
	int maxCellsGridSizeY = 2;
	int minimalGameWindowSizeX = 802;
	int minimalGameWindowSizeY = 631;
	bool ignoreManuallyLinkedWindows = true;

	Settings(const YAML::Node& config) {
		minCellsGridSizeX = getConfigInt(config, "settings/windowLinking/cellGrid/minCells/x", minCellsGridSizeX);
		minCellsGridSizeY = getConfigInt(config, "settings/windowLinking/cellGrid/minCells/y", minCellsGridSizeY);
		maxCellsGridSizeX = getConfigInt(config, "settings/windowLinking/cellGrid/maxCells/x", maxCellsGridSizeX);
		maxCellsGridSizeY = getConfigInt(config, "settings/windowLinking/cellGrid/maxCells/y", maxCellsGridSizeY);

		minimalGameWindowSizeX = getConfigInt(config, "settings/windowLinking/constants/minimalGameWindowSize/x", minimalGameWindowSizeX);
		minimalGameWindowSizeY = getConfigInt(config, "settings/windowLinking/constants/minimalGameWindowSize/y", minimalGameWindowSizeY);

		ignoreManuallyLinkedWindows = getConfigBool(config, "settings/windowLinking/behaviour/ignoreManuallyLinkedWindows", ignoreManuallyLinkedWindows);
	}
};
