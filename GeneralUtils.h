#pragma once

#include <string>
#include <gdiplus.h>

std::string getProgramFolderPath(const std::string& programPath);
void initializeRandom();
int randomizeValue(int value, int percentage, int maxDifference);
RECT GetTaskbarSize();
MONITORINFO GetMonitorInfoByIndex(int monitorIndex);
Gdiplus::Color changeBrightness(Gdiplus::Color color, int brightnessCorrection);
