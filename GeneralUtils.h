#pragma once

#include <string>

std::string getProgramFolderPath(const std::string& programPath);
void initializeRandom();
int randomizeValue(int value, int percentage, int maxDifference);
RECT GetTaskbarSize();
MONITORINFO GetMonitorInfoByIndex(int monitorIndex);