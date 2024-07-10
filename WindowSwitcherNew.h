#pragma once

#include <iostream>
#include <map>
#include <string>
#include <Windows.h>
#include <atomic>
#include <shared_mutex>

#include "WindowRelated.h"
#include "Data.h"
#include "gui/MainUi.h"

extern std::map<std::string, int> mapOfKeys;
extern std::map<HWND, WindowGroup*> handleToGroup;
extern std::atomic<WindowSwitcher::Settings*> settings;
//extern std::shared_mutex mapMutex; // MAY CAUSE CRASHES ON START!!!

extern bool hideNotMainWindows;

std::atomic<bool>& getStopMacroInput();
std::string getCurrentVersion();
float getOverlayValue();
int getOverlayActiveStateFullAmount();
int getOverlayActiveStateCurrentAmount();
std::string getProgramPath();
bool windowIsLinkedManually(HWND hwnd);
bool windowIsLinkedAutomatically(HWND hwnd);

bool checkHungWindow(HWND hwnd);
std::wstring getWindowName(HWND hwnd);
